#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Segment {
    uint32_t load_addr;
    std::vector<uint16_t> words;
    // header_count: word count written into the chunk header.
    // Normally 0 (meaning: use words.size()).  The cfg converter sets this to
    // the full CFG-stated mapping size when the source file is shorter, matching
    // Python's slice semantics (full count in header, truncated data written).
    uint32_t header_count = 0;
};

// Write one INTV2 file.
//   pocket=false  true word count in header (Nt Mini Noir)
//   pocket=true   odd-length chunks padded to even; header reflects padded count (Pocket)
//   quiet=true    suppress per-chunk progress output (for batch use)
// Returns true on success.
bool write_intv2(const std::vector<Segment>& segs, const std::string& path,
                 bool pocket, bool quiet = false);

// Write both -nt-noir.intv and -pocket.intv from a segment list.
// Returns true if both files were written successfully.
bool write_intv2_pair(const std::vector<Segment>& segs, const std::string& stem,
                      bool quiet = false);
