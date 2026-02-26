#!/usr/bin/env python3
"""
make_intv2_from_rom.py — Convert a jzIntv .rom file to INTV2 format
for Intellivision FPGA cores (Analogue Nt Mini Noir, Analogue Pocket).

Usage:
    python3 make_intv2_from_rom.py <input.rom> <output_stem>

    Writes two files:
        <output_stem>-nt-noir.intv   Nt Mini Noir (true word count in header)
        <output_stem>-pocket.intv    Analogue Pocket (odd chunks padded to even)

    The Pocket's openFPGA Chip32 VM requires both load address and word count
    to be multiples of 2; Nt Mini Noir uses the true word count.

.rom file format (jzIntv):
    Byte 0:    0xA8  (magic)
    Byte 1:    N     (number of segments)
    Byte 2:    0xFF ^ N  (consistency check)
    [× N, interleaved:]
      Byte:    seg_lo  — first 256-word page; load_addr = seg_lo × 256
      Byte:    seg_hi  — last 256-word page (inclusive, stored as hi not hi+1)
                         word_count = (seg_hi + 1 - seg_lo) × 256
      N×2 B:   ROM data, big-endian uint16 words (same encoding as .bin files)
      2 B:     CRC-16 (not verified)
    50 B:      Enable table (skipped)
    var:       Optional metadata tags (skipped)

INTV2 format (all values little-endian):
    For each ROM segment:
        [4 bytes: load address as uint32]
        [4 bytes: word count as uint32]
        [word_count × 2 bytes: ROM data as uint16 values]
    Terminating chunk:
        [4 bytes: 0x00000000]
        [4 bytes: 0x00000000]
"""

import struct
import sys
import os


def read_rom(rom_path):
    """
    Parse a jzIntv .rom file and return a list of (load_addr, rom_words) tuples.
    load_addr is a word address in Intellivision memory space.
    rom_words is a tuple of integer word values.
    """
    with open(rom_path, 'rb') as f:
        data = f.read()

    if len(data) < 3 or data[0] != 0xA8:
        print(f"Error: '{rom_path}' is not a jzIntv .rom file (bad magic byte).")
        sys.exit(1)

    num_segs = data[1]
    check    = data[2]
    if num_segs < 1 or num_segs != (0xFF ^ check):
        print(f"Error: '{rom_path}' has a corrupt header (segment count check failed).")
        sys.exit(1)

    pos = 3
    segments = []
    for i in range(num_segs):
        if pos + 2 > len(data):
            print(f"Error: '{rom_path}' is truncated (missing range for segment {i}).")
            sys.exit(1)

        seg_lo = data[pos];      pos += 1
        seg_hi = data[pos] + 1;  pos += 1   # stored as last page; +1 → exclusive end

        if seg_lo >= seg_hi:
            print(f"Error: '{rom_path}' segment {i} has a backwards address range.")
            sys.exit(1)

        word_count = (seg_hi - seg_lo) * 256
        byte_count = word_count * 2

        if pos + byte_count + 2 > len(data):
            print(f"Error: '{rom_path}' is truncated (segment {i} data).")
            sys.exit(1)

        rom_words = struct.unpack_from(f'>{word_count}H', data, pos)
        pos += byte_count
        pos += 2   # skip CRC-16

        load_addr = seg_lo * 256
        segments.append((load_addr, rom_words))

    return segments


def write_intv2(segments, output_path, pocket=False):
    """Write an INTV2 file from a list of (load_addr, rom_words) tuples."""
    total_words   = 0
    chunks_written = 0

    with open(output_path, 'wb') as f:
        for load_addr, rom_words in segments:
            word_count   = len(rom_words)
            padded_count = word_count + (word_count % 2)
            pad_word     = padded_count - word_count   # 0 or 1
            header_count = padded_count if pocket else word_count

            print(
                f"  ${load_addr:04X}–${load_addr + word_count - 1:04X}  "
                f"{word_count:5d} words"
                + (f"  (+1 pad)" if (pocket and pad_word) else "         ")
                + f"  ({header_count * 2:6d} bytes)"
            )

            f.write(struct.pack('<II', load_addr, header_count))
            for w in rom_words:
                f.write(struct.pack('<H', w))
            if pocket and pad_word:
                f.write(struct.pack('<H', 0))

            total_words    += word_count
            chunks_written += 1

        f.write(struct.pack('<II', 0, 0))

    size = os.path.getsize(output_path)
    print(f"  → {chunks_written} chunks, {total_words} words, {size} bytes")
    return size


def main():
    args   = [a for a in sys.argv[1:] if not a.startswith('--')]

    if len(args) != 2:
        print(f"Usage: {os.path.basename(sys.argv[0])} <input.rom> <output_stem>")
        print(f"  Writes <output_stem>-nt-noir.intv and <output_stem>-pocket.intv")
        sys.exit(1)

    rom_path, output_stem = args

    if not os.path.exists(rom_path):
        print(f"Error: file not found: {rom_path}")
        sys.exit(1)

    print(f"Reading rom:  {rom_path}")
    segments = read_rom(rom_path)
    print(f"  {len(segments)} segment(s)")
    print()

    noir_out   = output_stem + '-nt-noir.intv'
    pocket_out = output_stem + '-pocket.intv'

    print(f"Writing Nt Mini Noir → {noir_out}")
    write_intv2(segments, noir_out, pocket=False)
    print()

    print(f"Writing Pocket       → {pocket_out}")
    write_intv2(segments, pocket_out, pocket=True)


if __name__ == '__main__':
    main()
