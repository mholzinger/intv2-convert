#include "writer.hpp"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct Mapping {
    uint32_t file_start, file_end, rom_addr;
};

// Strip leading and trailing whitespace from s in-place.
static void trim(std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) { s.clear(); return; }
    size_t e = s.find_last_not_of(" \t\r\n");
    s = s.substr(b, e - b + 1);
}

// Parse a mapping line of the form: $HEX - $HEX = $HEX
// Returns true and fills file_start/file_end/rom_addr on success.
static bool parse_mapping_line(const std::string& line,
                                uint32_t& file_start,
                                uint32_t& file_end,
                                uint32_t& rom_addr) {
    const char* p = line.c_str();
    while (*p == ' ' || *p == '\t') ++p;
    if (*p != '$') return false; ++p;

    char* end;
    file_start = (uint32_t)strtoul(p, &end, 16);
    if (end == p) return false; p = end;

    while (*p == ' ' || *p == '\t') ++p;
    if (*p != '-') return false; ++p;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p != '$') return false; ++p;

    file_end = (uint32_t)strtoul(p, &end, 16);
    if (end == p) return false; p = end;

    while (*p == ' ' || *p == '\t') ++p;
    if (*p != '=') return false; ++p;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p != '$') return false; ++p;

    rom_addr = (uint32_t)strtoul(p, &end, 16);
    if (end == p) return false;

    return true;
}

// Return true if the line contains a PAGE bank-switching annotation
// (mirrors Python: re.compile(r'\bPAGE\s+(\w+)', re.IGNORECASE)).
static bool has_page_annotation(const std::string& line) {
    std::string upper;
    upper.reserve(line.size());
    for (char c : line) upper += (char)toupper((unsigned char)c);

    size_t pos = 0;
    while ((pos = upper.find("PAGE", pos)) != std::string::npos) {
        bool word_before = (pos == 0) || !isalnum((unsigned char)upper[pos - 1]);
        size_t after = pos + 4;
        bool word_after = (after >= upper.size()) || !isalpha((unsigned char)upper[after]);
        if (word_before && word_after) {
            size_t p2 = after;
            while (p2 < upper.size() && isspace((unsigned char)upper[p2])) ++p2;
            if (p2 > after && p2 < upper.size() && isalnum((unsigned char)upper[p2]))
                return true;
        }
        ++pos;
    }
    return false;
}

// Parse a jzIntv .cfg file.  Returns mappings on success; sets error and returns {} on failure.
static std::vector<Mapping> parse_cfg_file(const std::string& path, std::string& error) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        error = "cfg file not found: " + path;
        return {};
    }

    std::vector<Mapping>     mappings;
    std::vector<std::string> banked_lines;
    bool in_mapping = false;
    char buf[4096];

    while (fgets(buf, sizeof(buf), f)) {
        std::string line = buf;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();

        std::string s = line;
        trim(s);
        if (s.empty() || s[0] == ';') continue;

        if (s[0] == '[') {
            std::string lower = s;
            for (char& c : lower) c = (char)tolower((unsigned char)c);
            in_mapping = (lower.compare(0, 9, "[mapping]") == 0);
            continue;
        }

        if (!in_mapping) continue;

        if (has_page_annotation(s)) {
            banked_lines.push_back(s);
            continue;
        }

        uint32_t fs, fe, ra;
        if (parse_mapping_line(s, fs, fe, ra))
            mappings.push_back({fs, fe, ra});
    }
    fclose(f);

    if (!banked_lines.empty()) {
        error = "bank-switched ROM: INTV2 is a static load format and cannot "
                "represent PAGE-annotated ROMs";
        for (const auto& bl : banked_lines)
            error += "\n    " + bl;
        return {};
    }

    if (mappings.empty()) {
        error = "no [mapping] entries found in '" + path + "'";
        return {};
    }

    return mappings;
}

// Read a .bin (or .int) file and return word values (big-endian pairs → host uint16).
static std::vector<uint16_t> read_bin_words(const std::string& path, std::string& error) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        error = "bin file not found: " + path;
        return {};
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fclose(f);
        error = "empty or unreadable file: " + path;
        return {};
    }

    // Pad to even byte count
    size_t padded = (size_t)sz + ((size_t)sz & 1u);
    std::vector<uint8_t> data(padded, 0);
    fread(data.data(), 1, (size_t)sz, f);
    fclose(f);

    size_t num_words = padded / 2;
    std::vector<uint16_t> words(num_words);
    for (size_t i = 0; i < num_words; ++i)
        words[i] = ((uint16_t)data[i * 2] << 8) | data[i * 2 + 1];

    return words;
}

// Convert a .bin/.int + .cfg pair to both INTV2 variants.
// Returns "" on success, error description on failure.
std::string convert_cfg(const std::string& bin_path, const std::string& cfg_path,
                        const std::string& output_stem, bool quiet) {
    if (!quiet) printf("Reading cfg:  %s\n", cfg_path.c_str());

    std::string error;
    auto mappings = parse_cfg_file(cfg_path, error);
    if (mappings.empty()) return error;

    if (!quiet) printf("  %d mapping(s)\n", (int)mappings.size());
    if (!quiet) printf("Reading bin:  %s\n", bin_path.c_str());

    auto bin_words = read_bin_words(bin_path, error);
    if (bin_words.empty()) return error;

    if (!quiet) printf("  %d words\n\n", (int)bin_words.size());

    // Build Segment list from mappings + bin data
    std::vector<Segment> segs;
    segs.reserve(mappings.size());
    for (const auto& m : mappings) {
        uint32_t wc      = m.file_end - m.file_start + 1;
        uint32_t bin_len = (uint32_t)bin_words.size();
        // Mirror Python list-slice semantics: the header states the full CFG count,
        // but only the available file data is written (silent truncation, same as
        // Python).  Confirmed to work on Nt Mini Noir hardware.
        uint32_t actual_start = std::min(m.file_start, bin_len);
        uint32_t actual_end   = std::min(m.file_start + wc, bin_len);
        Segment seg;
        seg.load_addr    = m.rom_addr;
        seg.header_count = wc;          // full CFG-stated count goes in the header
        seg.words.assign(bin_words.begin() + actual_start,
                         bin_words.begin() + actual_end);
        segs.push_back(std::move(seg));
    }

    if (!write_intv2_pair(segs, output_stem, quiet))
        return "write failed";

    return "";
}

int cmd_cfg(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: intv2convert cfg <input.bin> <input.cfg> <output_stem>\n");
        fprintf(stderr, "  Writes <output_stem>-nt-noir.intv and <output_stem>-pocket.intv\n");
        return 1;
    }

    std::string err = convert_cfg(argv[1], argv[2], argv[3], false);
    if (!err.empty()) {
        fprintf(stderr, "Error: %s\n", err.c_str());
        return 1;
    }
    return 0;
}
