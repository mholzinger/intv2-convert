#!/usr/bin/env python3
"""
make_intv2.py — Convert an IntyBASIC OPTION MAP 2 ROM to INTV2 format
for Intellivision FPGA cores (Analogue Nt Mini Noir, Analogue Pocket).

Usage:
    python3 tools/make_intv2.py <input.lst> <output.intv> [--pocket]

    --pocket  Pad odd-length chunks to even word count in the header.
              Required by the Analogue Pocket openFPGA Chip32 VM, which
              requires both load address and length to be multiples of 2.
              Omit for the Nt Mini Noir (true word count in header).

INTV2 format (all values little-endian):
    For each ROM segment:
        [4 bytes: load address as uint32]
        [4 bytes: word count as uint32]
        [word_count × 2 bytes: ROM data as uint16 values]
    Terminating chunk:
        [4 bytes: 0x00000000]
        [4 bytes: 0x00000000]

OPTION MAP 2 segment layout:
    Seg 0:  $5000–$6FFF   8K
    Seg 1:  $A000–$BFFF   8K
    Seg 2:  $C040–$FFFF  16K  (includes ZMUS engine at $D000)
    Seg 3:  $2100–$2FFF   4K
    Seg 4:  $7100–$7FFF   4K
    Seg 5:  $4810–$4FFF   2K
"""

import struct
import sys
import os

# OPTION MAP 2 segments in load order (ascending address)
SEGMENTS = [
    (0x2100, 0x2FFF, "Seg3"),
    (0x4810, 0x4FFF, "Seg5"),
    (0x5000, 0x6FFF, "Seg0"),
    (0x7100, 0x7FFF, "Seg4"),
    (0xA000, 0xBFFF, "Seg1"),
    (0xC040, 0xFFFF, "Seg2"),
]


def parse_lst(lst_path):
    """
    Parse an as1600 listing file and return a dict of {address: word_value}.

    Listing lines with assembled data look like:
        5000   000D 0050                  BIDECLE _ZERO
        A000   0004 0150 0014             CALL CLRSCR
        2100   0275                       BEGIN

    The first token is a 4-hex-digit address. Subsequent 4-hex-digit tokens
    are word values at consecutive addresses. Any non-hex token ends the data.
    """
    memory = {}
    with open(lst_path, 'r', errors='replace') as f:
        for line in f:
            parts = line.split()
            if not parts:
                continue
            # Address token must be exactly 4 hex characters
            if len(parts[0]) != 4:
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            # Collect consecutive 4-hex-digit word values
            for i, part in enumerate(parts[1:]):
                if len(part) != 4:
                    break
                try:
                    memory[addr + i] = int(part, 16)
                except ValueError:
                    break
    return memory


def write_intv2(memory, segments, output_path, pocket=False):
    """Write INTV2 chunked file from the memory dict.

    pocket=True:  header count rounded up to even (Analogue Pocket Chip32 requirement).
    pocket=False: header count is true word count (Nt Mini Noir).
    """
    total_words = 0
    chunks_written = 0

    with open(output_path, 'wb') as f:
        for start, end, name in segments:
            # Find all words in this segment's address range
            seg_words = {addr: val for addr, val in memory.items()
                         if start <= addr <= end}

            if not seg_words:
                print(f"  {name}: no data, skipping")
                continue

            last_addr = max(seg_words.keys())
            word_count = last_addr - start + 1

            # Pocket: both address and length must be multiples of 2.
            # Round odd word_count up by one and pad data with a zero word.
            padded_count = word_count + (word_count % 2)
            pad_word = padded_count - word_count   # 0 or 1

            header_count = padded_count if pocket else word_count

            print(f"  {name}: ${start:04X}–${last_addr:04X}  {word_count:5d} words"
                  + (f"  (+1 pad)" if (pocket and pad_word) else "         ")
                  + f"  ({padded_count * 2:6d} bytes)")

            # Chunk header: 4-byte address + 4-byte word count (both LE uint32)
            f.write(struct.pack('<II', start, header_count))

            # Chunk data: words in address order, zero-fill any gaps
            for addr in range(start, last_addr + 1):
                f.write(struct.pack('<H', seg_words.get(addr, 0)))

            # Pocket alignment padding: one zero word when word_count is odd
            if pocket and pad_word:
                f.write(struct.pack('<H', 0))

            total_words += word_count
            chunks_written += 1

        # Terminating sentinel: address=0, count=0
        f.write(struct.pack('<II', 0, 0))

    print(f"\n  {chunks_written} chunks, {total_words} words total "
          f"({total_words * 2 + chunks_written * 8 + 8} bytes with headers)")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    pocket = '--pocket' in sys.argv

    if len(args) != 2:
        print(f"Usage: {os.path.basename(sys.argv[0])} <input.lst> <output.intv> [--pocket]")
        sys.exit(1)

    lst_path, output_path = args

    if not os.path.exists(lst_path):
        print(f"Error: listing file not found: {lst_path}")
        sys.exit(1)

    print(f"Parsing: {lst_path}")
    memory = parse_lst(lst_path)
    print(f"  {len(memory)} words found across all segments\n")

    target = 'Analogue Pocket' if pocket else 'Nt Mini Noir'
    print(f"Writing: {output_path}  ({target})")
    write_intv2(memory, SEGMENTS, output_path, pocket=pocket)

    size = os.path.getsize(output_path)
    print(f"  Output: {size} bytes")


if __name__ == '__main__':
    main()
