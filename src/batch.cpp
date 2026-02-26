#include "batch.hpp"
#include "writer.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Forward declarations of converter functions (defined in rom.cpp / cfg.cpp)
std::string convert_rom(const std::string& rom_path, const std::string& output_stem,
                        bool quiet);
std::string convert_cfg(const std::string& bin_path, const std::string& cfg_path,
                        const std::string& output_stem, bool quiet);

enum class SrcType { ROM, CFG };

struct Source {
    SrcType  type;
    fs::path data;   // .rom, .bin, or .int
    fs::path cfg;    // only for CFG type
};

struct GameRecord {
    fs::path    src_dir;
    std::string rel;         // relative path from source root ("." if top-level)
    std::string stem;
    Source      src;
    fs::path    target_dir;
    bool        already_done;
};

// Find the best source format for a given stem in dir.
// Priority: .rom > .bin+.cfg > .int+.cfg
static std::optional<Source> find_source(const fs::path& dir, const std::string& stem) {
    fs::path rom = dir / (stem + ".rom");
    fs::path bin = dir / (stem + ".bin");
    fs::path inT = dir / (stem + ".int");
    fs::path cfg = dir / (stem + ".cfg");

    if (fs::exists(rom))                      return Source{ SrcType::ROM, rom, {} };
    if (fs::exists(bin) && fs::exists(cfg))   return Source{ SrcType::CFG, bin, cfg };
    if (fs::exists(inT) && fs::exists(cfg))   return Source{ SrcType::CFG, inT, cfg };
    return std::nullopt;
}

static std::string fmt_source(const Source& src) {
    if (src.type == SrcType::ROM) return ".rom";
    return src.data.extension().string() + "+.cfg";
}

// Return a display-friendly forward-slash path string.
static std::string fwd(const std::string& s) {
    std::string r = s;
    for (char& c : r) if (c == '\\') c = '/';
    return r;
}

// Walk source recursively and collect game records.
// Uses std::map<fs::path, ...> so directories iterate in sorted order;
// stems come from std::set<> so they're sorted too — matches Python's sort behaviour.
static std::vector<GameRecord> collect_games(const fs::path& source,
                                              const fs::path& target,
                                              bool force) {
    std::map<fs::path, std::set<std::string>> dir_stems;
    dir_stems[source];  // ensure the root itself appears even if it has no files

    for (const auto& entry : fs::recursive_directory_iterator(source)) {
        if (!fs::is_regular_file(entry)) continue;
        std::string ext = entry.path().extension().string();
        // Case-insensitive extension match
        for (char& c : ext) c = (char)tolower((unsigned char)c);
        if (ext == ".bin" || ext == ".int" || ext == ".rom")
            dir_stems[entry.path().parent_path()].insert(entry.path().stem().string());
    }

    std::vector<GameRecord> games;
    for (const auto& [dir, stems] : dir_stems) {
        fs::path rel_path   = fs::relative(dir, source);
        std::string rel_str = (rel_path == fs::path(".")) ? "." : rel_path.string();
        fs::path tgt_dir    = (rel_str == ".") ? target : (target / rel_path);

        for (const std::string& stem : stems) {
            auto src_opt = find_source(dir, stem);
            if (!src_opt) continue;

            fs::path noir_out   = tgt_dir / (stem + "-nt-noir.intv");
            fs::path pocket_out = tgt_dir / (stem + "-pocket.intv");
            bool done = !force && fs::exists(noir_out) && fs::exists(pocket_out);

            games.push_back({ dir, rel_str, stem, *src_opt, tgt_dir, done });
        }
    }
    return games;
}

BatchResult run_batch(const std::string& source_str, const std::string& output_str,
                      bool dry_run, bool force,
                      const std::function<void(const std::string&)>& log_fn) {
    BatchResult result{0, 0, 0};

    std::error_code ec;
    fs::path source = fs::canonical(fs::path(source_str), ec);
    if (ec || !fs::is_directory(source)) {
        log_fn("Error: source directory not found: " + source_str);
        result.failed = 1;
        return result;
    }
    fs::path output = fs::weakly_canonical(fs::path(output_str));

    auto games = collect_games(source, output, force);

    int total   = (int)games.size();
    int skipped = 0;
    for (const auto& g : games) if (g.already_done) ++skipped;
    int pending = total - skipped;

    result.skipped = skipped;

    log_fn("Source: " + fwd(source.string()));
    log_fn("Output: " + fwd(output.string()));
    log_fn(std::string(dry_run ? "DRY RUN -- " : "") +
           "Found " + std::to_string(total) + " convertible games");
    log_fn("  " + std::to_string(skipped) + " already done, " +
           std::to_string(pending) + " to convert");
    log_fn("");

    if (pending == 0) {
        log_fn("Nothing to do.");
        return result;
    }

    if (dry_run) {
        log_fn("Would convert:");
        for (const auto& g : games) {
            if (g.already_done) continue;
            std::string label = (g.rel == ".") ? g.stem : fwd(g.rel) + "/" + g.stem;
            // Left-justify source format in a 9-char field: "  [.rom     ]  Soccer"
            std::string src_field = fmt_source(g.src);
            while (src_field.size() < 9) src_field += ' ';
            log_fn("  [" + src_field + "]  " + label);
        }
        log_fn("");
        log_fn("Re-run without --dry-run to write files.");
        return result;
    }

    std::vector<std::pair<std::string, std::string>> failed;

    for (const auto& g : games) {
        if (g.already_done) continue;

        std::string label = (g.rel == ".") ? g.stem : fwd(g.rel) + "/" + g.stem;

        std::error_code dir_ec;
        fs::create_directories(g.target_dir, dir_ec);
        if (dir_ec) {
            log_fn("  FAIL " + label + "  (" + fmt_source(g.src) + ")");
            log_fn("       cannot create output directory: " + dir_ec.message());
            failed.push_back({ label, dir_ec.message() });
            continue;
        }

        std::string output_stem = (g.target_dir / g.stem).string();
        std::string err;

        if (g.src.type == SrcType::ROM)
            err = convert_rom(g.src.data.string(), output_stem, true);
        else
            err = convert_cfg(g.src.data.string(), g.src.cfg.string(), output_stem, true);

        if (err.empty()) {
            log_fn("  OK   " + label + "  (" + fmt_source(g.src) + ")");
            ++result.converted;
        } else {
            log_fn("  FAIL " + label + "  (" + fmt_source(g.src) + ")");
            size_t nl = err.find('\n');
            log_fn("       " + (nl == std::string::npos ? err : err.substr(0, nl)));
            failed.push_back({ label, err });
        }
    }

    result.failed = (int)failed.size();
    log_fn("");
    log_fn("Done: " + std::to_string(result.converted) + " converted, " +
           std::to_string(result.failed) + " failed, " +
           std::to_string(result.skipped) + " skipped.");

    if (!failed.empty()) {
        log_fn("");
        log_fn("Failed games:");
        for (const auto& [label, err] : failed) {
            log_fn("  " + label);
            if (!err.empty()) {
                std::string line;
                for (char c : err) {
                    if (c == '\n') { log_fn("    " + line); line.clear(); }
                    else           { line += c; }
                }
                if (!line.empty()) log_fn("    " + line);
            }
        }
    }

    return result;
}

int cmd_batch(int argc, char* argv[]) {
    bool dry_run = false;
    bool force   = false;
    std::vector<const char*> pos;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--dry-run") dry_run = true;
        else if (a == "--force") force = true;
        else pos.push_back(argv[i]);
    }

    if (pos.size() != 2) {
        fprintf(stderr,
            "Usage: intv2convert batch <source> <output> [--dry-run] [--force]\n\n"
            "  <source>   Directory to walk for Intellivision ROM files\n"
            "  <output>   Directory to write converted .intv files into\n"
            "  --dry-run  Show what would be converted without writing files\n"
            "  --force    Re-convert games that already have output files\n");
        return 1;
    }

    BatchResult r = run_batch(pos[0], pos[1], dry_run, force,
                              [](const std::string& s){ printf("%s\n", s.c_str()); });
    return r.failed > 0 ? 1 : 0;
}
