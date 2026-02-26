#include "writer.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Byte-swap a big-endian uint16 read from the .rom data stream to host order.
static inline uint16_t bswap16(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}

// Parse a jzIntv .rom file.
// Returns the segment list on success; on failure, sets 'error' and returns {}.
// Never prints; all errors are returned via 'error'.
static std::vector<Segment> read_rom_file(const std::string& path, std::string& error) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        error = "file not found: " + path;
        return {};
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> data((size_t)sz);
    fread(data.data(), 1, (size_t)sz, f);
    fclose(f);

    if (sz < 3 || data[0] != 0xA8) {
        error = "not a jzIntv .rom file (bad magic byte)";
        return {};
    }

    int num_segs = data[1];
    if (num_segs < 1 || data[2] != (uint8_t)(0xFF ^ num_segs)) {
        error = "corrupt header (segment count check failed)";
        return {};
    }

    size_t pos = 3;
    std::vector<Segment> segs;

    for (int i = 0; i < num_segs; ++i) {
        if (pos + 2 > (size_t)sz) {
            error = "truncated (missing range for segment " + std::to_string(i) + ")";
            return {};
        }

        int seg_lo = data[pos++];
        int seg_hi = data[pos++] + 1;  // stored as last page; +1 makes it exclusive end

        if (seg_lo >= seg_hi) {
            error = "segment " + std::to_string(i) + " has a backwards address range";
            return {};
        }

        uint32_t wc = (uint32_t)(seg_hi - seg_lo) * 256;

        if (pos + (size_t)wc * 2 + 2 > (size_t)sz) {
            error = "truncated (segment " + std::to_string(i) + " data)";
            return {};
        }

        Segment seg;
        seg.load_addr = (uint32_t)seg_lo * 256;
        seg.words.resize(wc);
        for (uint32_t j = 0; j < wc; ++j) {
            uint16_t be;
            memcpy(&be, data.data() + pos + j * 2, 2);
            seg.words[j] = bswap16(be);
        }
        pos += wc * 2 + 2;  // data + CRC-16
        segs.push_back(std::move(seg));
    }

    return segs;
}

// Convert a .rom file to both INTV2 variants.
// quiet=false: print progress to stdout (interactive use).
// quiet=true:  suppress progress; errors returned as string (batch use).
// Returns "" on success, error description on failure.
std::string convert_rom(const std::string& rom_path, const std::string& output_stem,
                        bool quiet) {
    if (!quiet) printf("Reading rom:  %s\n", rom_path.c_str());

    std::string error;
    auto segs = read_rom_file(rom_path, error);
    if (segs.empty())
        return error.empty() ? "unknown parse error" : error;

    if (!quiet) printf("  %d segment(s)\n\n", (int)segs.size());

    if (!write_intv2_pair(segs, output_stem, quiet))
        return "write failed";

    return "";
}

int cmd_rom(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: intv2convert rom <input.rom> <output_stem>\n");
        fprintf(stderr, "  Writes <output_stem>-nt-noir.intv and <output_stem>-pocket.intv\n");
        return 1;
    }

    std::string err = convert_rom(argv[1], argv[2], false);
    if (!err.empty()) {
        fprintf(stderr, "Error: %s\n", err.c_str());
        return 1;
    }
    return 0;
}
