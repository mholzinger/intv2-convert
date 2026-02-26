#include "writer.hpp"
#include <cstdio>
#include <cstdint>

static void put_u16le(FILE* f, uint16_t v) {
    uint8_t b[2] = { uint8_t(v), uint8_t(v >> 8) };
    fwrite(b, 1, 2, f);
}

static void put_u32le(FILE* f, uint32_t v) {
    uint8_t b[4] = { uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24) };
    fwrite(b, 1, 4, f);
}

bool write_intv2(const std::vector<Segment>& segs, const std::string& path,
                 bool pocket, bool quiet) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot open for writing: %s\n", path.c_str());
        return false;
    }

    int      chunks      = 0;
    uint32_t total_words = 0;

    for (const auto& seg : segs) {
        uint32_t data_wc = (uint32_t)seg.words.size();
        // Use the explicit header count if set, otherwise fall back to data size.
        // The cfg converter sets header_count to the full CFG-stated mapping size
        // even when the source file is shorter — matching Python's behaviour.
        uint32_t hdr_base = seg.header_count ? seg.header_count : data_wc;
        uint32_t padded   = hdr_base + (hdr_base & 1u);
        uint32_t hdr_wc   = pocket ? padded : hdr_base;

        if (!quiet) {
            printf("  $%04X-$%04X  %5u words%s  (%6u bytes)\n",
                seg.load_addr,
                seg.load_addr + hdr_base - 1,
                hdr_base,
                (pocket && (hdr_base & 1u)) ? "  (+1 pad)" : "          ",
                hdr_wc * 2u);
        }

        put_u32le(f, seg.load_addr);
        put_u32le(f, hdr_wc);
        for (uint16_t w : seg.words) put_u16le(f, w);  // only actual data
        if (pocket && (hdr_base & 1u) && data_wc == hdr_base) put_u16le(f, 0);

        total_words += hdr_base;
        ++chunks;
    }

    // Terminating sentinel
    put_u32le(f, 0);
    put_u32le(f, 0);

    long size = ftell(f);
    fclose(f);

    if (!quiet)
        printf("  -> %d chunks, %u words, %ld bytes\n", chunks, total_words, size);

    return true;
}

bool write_intv2_pair(const std::vector<Segment>& segs, const std::string& stem,
                      bool quiet) {
    std::string noir_path   = stem + "-nt-noir.intv";
    std::string pocket_path = stem + "-pocket.intv";

    if (!quiet) printf("Writing Nt Mini Noir -> %s\n", noir_path.c_str());
    if (!write_intv2(segs, noir_path, false, quiet)) return false;
    if (!quiet) printf("\n");

    if (!quiet) printf("Writing Pocket       -> %s\n", pocket_path.c_str());
    if (!write_intv2(segs, pocket_path, true, quiet)) return false;

    return true;
}
