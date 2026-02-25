#!/usr/bin/env python3
"""
make_intv2_from_cfg.py — Convert a .bin + .cfg ROM pair to INTV2 format
for Intellivision FPGA cores (Analogue Nt Mini Noir, Analogue Pocket).

Usage:
    python3 tools/make_intv2_from_cfg.py <input.bin> <input.cfg> <output_stem>

    Writes two files:
        <output_stem>-nt-noir.intv   Nt Mini Noir (true word count in header)
        <output_stem>-pocket.intv    Analogue Pocket (odd chunks padded to even)

    The Pocket's openFPGA Chip32 VM requires both load address and word count
    to be multiples of 2; Nt Mini Noir uses the true word count.

.cfg file format (jzIntv word-offset mapping):
    [mapping]
    $file_word_start - $file_word_end = $rom_address
    ...

    All values are 16-bit word offsets/addresses (not bytes).

INTV2 format (all values little-endian):
    For each ROM segment:
        [4 bytes: load address as uint32]
        [4 bytes: word count as uint32]
        [word_count × 2 bytes: ROM data as uint16 values]
    Terminating chunk:
        [4 bytes: 0x00000000]
        [4 bytes: 0x00000000]
"""

import re
import struct
import sys
import os


def parse_cfg(cfg_path):
    """
    Parse a jzIntv .cfg file and return a list of (file_start, file_end, rom_addr).
    All values are word offsets/addresses (not bytes).

    Raises SystemExit if the cfg uses bank switching (PAGE annotations), which
    INTV2 cannot represent.  INTV2 is a static load format — it snapshots address
    space once; there is no runtime mechanism to swap pages in and out.  A banked
    ROM silently converted would have every page overwriting the previous one at
    the same load address, producing corrupt code.
    """
    mappings = []
    banked_lines = []
    in_mapping = False

    with open(cfg_path, 'r') as f:
        for line in f:
            stripped = line.strip()
            if stripped.lower() == '[mapping]':
                in_mapping = True
                continue
            if stripped.startswith('['):
                in_mapping = False
                continue
            if not in_mapping or not stripped or stripped.startswith(';'):
                continue

            # Detect bank-switching: PAGE annotation after the address range
            page_match = re.search(r'\bPAGE\s+(\d+)', stripped, re.IGNORECASE)
            if page_match:
                banked_lines.append(stripped)
                continue

            # Pattern: $XXXX - $XXXX = $XXXX
            m = re.match(
                r'\$([0-9A-Fa-f]+)\s*-\s*\$([0-9A-Fa-f]+)\s*=\s*\$([0-9A-Fa-f]+)',
                stripped,
            )
            if m:
                file_start = int(m.group(1), 16)
                file_end   = int(m.group(2), 16)
                rom_addr   = int(m.group(3), 16)
                mappings.append((file_start, file_end, rom_addr))

    if banked_lines:
        print(f"Error: '{cfg_path}' uses bank switching (PAGE annotations).")
        print()
        print("  INTV2 is a static load format — it maps each chunk to a fixed")
        print("  address once at load time.  Banked ROMs swap multiple pages into")
        print("  the same address window at runtime, which INTV2 has no mechanism")
        print("  to represent.  Each page would silently overwrite the previous one,")
        print("  producing a corrupt ROM that runs garbage code.")
        print()
        print("  Banked segments found:")
        for bl in banked_lines:
            print(f"    {bl}")
        print()
        print("  This ROM cannot be converted to INTV2 format.")
        sys.exit(1)

    return mappings


def read_bin_words(bin_path):
    """Read a jzIntv .bin file as a list of 16-bit ROM words.

    jzIntv .bin files store each ROM word big-endian (MSB first).
    We return plain integer values; the caller writes them little-endian
    into the INTV2 file as required by the Analogue FPGA core.
    """
    with open(bin_path, 'rb') as f:
        data = f.read()
    if len(data) % 2:
        data += b'\x00'  # pad to even length
    words = list(struct.unpack_from(f'>{len(data) // 2}H', data))
    return words


def write_intv2(mappings, bin_words, output_path, pocket=False):
    """Write an INTV2 file from cfg mappings and bin word array."""
    total_words = 0
    chunks_written = 0

    with open(output_path, 'wb') as f:
        for file_start, file_end, rom_addr in mappings:
            word_count = file_end - file_start + 1
            chunk_words = bin_words[file_start : file_start + word_count]

            # Pocket: both address and length must be multiples of 2
            padded_count = word_count + (word_count % 2)
            pad_word = padded_count - word_count  # 0 or 1
            header_count = padded_count if pocket else word_count

            print(
                f"  ${rom_addr:04X}–${rom_addr + word_count - 1:04X}  "
                f"{word_count:5d} words"
                + (f"  (+1 pad)" if (pocket and pad_word) else "         ")
                + f"  ({header_count * 2:6d} bytes)"
            )

            # Chunk header: load address + word count
            f.write(struct.pack('<II', rom_addr, header_count))

            # Chunk data
            for w in chunk_words:
                f.write(struct.pack('<H', w))

            # Pocket alignment pad (one zero word when word_count is odd)
            if pocket and pad_word:
                f.write(struct.pack('<H', 0))

            total_words += word_count
            chunks_written += 1

        # Terminating sentinel
        f.write(struct.pack('<II', 0, 0))

    size = os.path.getsize(output_path)
    print(f"  → {chunks_written} chunks, {total_words} words, {size} bytes")
    return size


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]

    if len(args) != 3:
        print(f"Usage: {os.path.basename(sys.argv[0])} <input.bin> <input.cfg> <output_stem>")
        print(f"  Writes <output_stem>-nt-noir.intv and <output_stem>-pocket.intv")
        sys.exit(1)

    bin_path, cfg_path, output_stem = args

    for path in (bin_path, cfg_path):
        if not os.path.exists(path):
            print(f"Error: file not found: {path}")
            sys.exit(1)

    print(f"Reading cfg:  {cfg_path}")
    mappings = parse_cfg(cfg_path)
    print(f"  {len(mappings)} mapping entries")

    print(f"Reading bin:  {bin_path}")
    bin_words = read_bin_words(bin_path)
    print(f"  {len(bin_words)} words ({len(bin_words) * 2} bytes)")
    print()

    noir_out   = output_stem + '-nt-noir.intv'
    pocket_out = output_stem + '-pocket.intv'

    print(f"Writing Nt Mini Noir → {noir_out}")
    write_intv2(mappings, bin_words, noir_out, pocket=False)
    print()

    print(f"Writing Pocket       → {pocket_out}")
    write_intv2(mappings, bin_words, pocket_out, pocket=True)


if __name__ == '__main__':
    main()
