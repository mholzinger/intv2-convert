#include "writer.hpp"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

// OPTION MAP 2 segment layout (ascending address order).
struct SegDef { uint32_t start, end; const char* name; };
static const SegDef SEGMENTS[] = {
    { 0x2100, 0x2FFF, "Seg3" },
    { 0x4810, 0x4FFF, "Seg5" },
    { 0x5000, 0x6FFF, "Seg0" },
    { 0x7100, 0x7FFF, "Seg4" },
    { 0xA000, 0xBFFF, "Seg1" },
    { 0xC040, 0xFFFF, "Seg2" },
};
static const int NUM_SEGMENTS = (int)(sizeof(SEGMENTS) / sizeof(SEGMENTS[0]));

// Parse an as1600 listing file.
// Each line: <4-hex-digit address>  [<4-hex-digit word> ...]  [assembly text...]
// Returns a map of address → word value.
static std::map<uint32_t, uint16_t> parse_lst_file(const std::string& path,
                                                     std::string& error) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        error = "file not found: " + path;
        return {};
    }

    std::map<uint32_t, uint16_t> memory;
    char buf[4096];

    while (fgets(buf, sizeof(buf), f)) {
        const char* p = buf;

        // Skip leading whitespace (listing format starts at column 0, but be safe)
        while (*p == ' ' || *p == '\t') ++p;

        // Find end of first token
        const char* tok_start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
        size_t tok_len = (size_t)(p - tok_start);

        // First token must be exactly 4 hex chars
        if (tok_len != 4) continue;
        bool all_hex = true;
        for (size_t i = 0; i < 4; ++i)
            if (!isxdigit((unsigned char)tok_start[i])) { all_hex = false; break; }
        if (!all_hex) continue;

        char addr_str[5] = { tok_start[0], tok_start[1], tok_start[2], tok_start[3], '\0' };
        uint32_t addr = (uint32_t)strtoul(addr_str, nullptr, 16);
        uint32_t offset = 0;

        // Parse subsequent tokens: collect 4-hex-digit word values
        while (true) {
            while (*p == ' ' || *p == '\t') ++p;
            if (!*p || *p == '\n' || *p == '\r') break;

            tok_start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
            tok_len = (size_t)(p - tok_start);

            if (tok_len != 4) break;  // non-data token ends the run

            all_hex = true;
            for (size_t i = 0; i < 4; ++i)
                if (!isxdigit((unsigned char)tok_start[i])) { all_hex = false; break; }
            if (!all_hex) break;

            char word_str[5] = { tok_start[0], tok_start[1], tok_start[2], tok_start[3], '\0' };
            memory[addr + offset] = (uint16_t)strtoul(word_str, nullptr, 16);
            ++offset;
        }
    }
    fclose(f);
    return memory;
}

// Convert an IntyBASIC OPTION MAP 2 listing file to a single INTV2 file.
// pocket=true: Analogue Pocket variant (pad odd-length chunks).
// Returns "" on success, error description on failure.
std::string convert_lst(const std::string& lst_path, const std::string& output_path,
                        bool pocket, bool quiet) {
    if (!quiet) printf("Parsing: %s\n", lst_path.c_str());

    std::string error;
    auto memory = parse_lst_file(lst_path, error);
    if (!error.empty()) return error;
    if (memory.empty()) return "no assembled data found in listing";

    if (!quiet) printf("  %d words found across all segments\n\n", (int)memory.size());

    std::vector<Segment> segs;

    for (int si = 0; si < NUM_SEGMENTS; ++si) {
        const SegDef& sdef = SEGMENTS[si];

        // Find highest address with data in this segment range
        uint32_t last_addr = sdef.start;
        bool has_data = false;
        for (const auto& kv : memory) {
            if (kv.first >= sdef.start && kv.first <= sdef.end) {
                if (!has_data || kv.first > last_addr) last_addr = kv.first;
                has_data = true;
            }
        }

        if (!has_data) {
            if (!quiet) printf("  %s: no data, skipping\n", sdef.name);
            continue;
        }

        uint32_t wc     = last_addr - sdef.start + 1;
        uint32_t padded = wc + (wc & 1u);

        if (!quiet) {
            printf("  %s: $%04X-$%04X  %5u words%s  (%6u bytes)\n",
                sdef.name, sdef.start, last_addr, wc,
                (pocket && (wc & 1u)) ? "  (+1 pad)" : "          ",
                padded * 2u);
        }

        Segment seg;
        seg.load_addr = sdef.start;
        seg.words.resize(wc);
        for (uint32_t i = 0; i < wc; ++i) {
            auto it = memory.find(sdef.start + i);
            seg.words[i] = (it != memory.end()) ? it->second : 0u;
        }
        segs.push_back(std::move(seg));
    }

    if (segs.empty()) return "no segment data found in listing";

    if (!quiet) {
        const char* target = pocket ? "Analogue Pocket" : "Nt Mini Noir";
        printf("\nWriting: %s  (%s)\n", output_path.c_str(), target);
    }

    if (!write_intv2(segs, output_path, pocket, quiet))
        return "write failed";

    return "";
}

int cmd_lst(int argc, char* argv[]) {
    bool pocket = false;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--pocket") pocket = true;
        else pos.push_back(argv[i]);
    }

    if (pos.size() != 2) {
        fprintf(stderr, "Usage: intv2convert lst <input.lst> <output.intv> [--pocket]\n");
        return 1;
    }

    std::string err = convert_lst(pos[0], pos[1], pocket, false);
    if (!err.empty()) {
        fprintf(stderr, "Error: %s\n", err.c_str());
        return 1;
    }
    return 0;
}
