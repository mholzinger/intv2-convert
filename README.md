# intv2-convert

Python tools for converting Intellivision ROM files to **INTV2** format, the chunked
ROM loading format used by the Intellivision FPGA cores on the
**Analogue Nt Mini Noir** and **Analogue Pocket**.

## Requirements

Python 3.6 or later. No third-party packages — standard library only.

---

## Tools

### `make_intv2_from_cfg.py` — jzIntv BIN + CFG pair

Converts a jzIntv `.bin` / `.cfg` pair to INTV2. Writes two output files: one for
the Nt Mini Noir (true word counts) and one for the Pocket (word counts padded to
even, as required by the Chip32 VM).

```
python3 make_intv2_from_cfg.py <input.bin> <input.cfg> <output_stem>
```

Writes:
- `<output_stem>-nt-noir.intv`
- `<output_stem>-pocket.intv`

**Example:**

```
python3 make_intv2_from_cfg.py "Sea Battle.bin" "Sea Battle.cfg" "Sea Battle"
```

**CFG format** (jzIntv word-offset mapping):

```
[mapping]
$file_start - $file_end = $rom_address
```

All values are 16-bit word offsets/addresses (not byte offsets). Segments with a
`PAGE` annotation (Mattel-style bank switching) are rejected with an error — INTV2
is a static load format and cannot represent banked ROMs.

---

### `make_intv2.py` — IntyBASIC assembler listing

Converts an IntyBASIC `OPTION MAP 2` assembler listing (`.lst` from `as1600`) to
INTV2. Use `--pocket` when targeting the Analogue Pocket.

```
python3 make_intv2.py <input.lst> <output.intv> [--pocket]
```

**Example:**

```
python3 make_intv2.py mygame.lst mygame-pocket.intv --pocket
python3 make_intv2.py mygame.lst mygame-nt-noir.intv
```

`OPTION MAP 2` segment layout:

| Segment | Address range | Size |
|---------|--------------|------|
| Seg 0   | $5000–$6FFF  | 8K   |
| Seg 1   | $A000–$BFFF  | 8K   |
| Seg 2   | $C040–$FFFF  | 16K  |
| Seg 3   | $2100–$2FFF  | 4K   |
| Seg 4   | $7100–$7FFF  | 4K   |
| Seg 5   | $4810–$4FFF  | 2K   |

---

### `make_intv2_from_rom.py` — jzIntv self-describing ROM

Converts a jzIntv `.rom` file directly, without a companion `.cfg`. The `.rom`
format is self-describing: its header contains the memory map, so no external
configuration is needed.

```
python3 make_intv2_from_rom.py <input.rom> <output_stem>
```

Writes:
- `<output_stem>-nt-noir.intv`
- `<output_stem>-pocket.intv`

---

### `batch_convert.py` — Batch converter

Walks a source directory recursively and converts all supported ROM files,
mirroring the directory structure under the output directory.  Already-converted
games (both output files present) are skipped automatically.

```
python3 batch_convert.py <source> <output> [--dry-run] [--force]
```

| Argument | Description |
|----------|-------------|
| `<source>` | Directory to walk for Intellivision ROM files |
| `<output>` | Directory to write converted `.intv` files into (created if needed) |
| `--dry-run` | Show what would be converted without writing any files |
| `--force` | Re-convert games that already have both output files |

**Source format priority** (first match wins for each game stem):

1. `.rom` → `make_intv2_from_rom.py`
2. `.bin` + `.cfg` → `make_intv2_from_cfg.py`
3. `.int` + `.cfg` → `make_intv2_from_cfg.py` (`.int` is identical to `.bin`)

**Example:**

```
python3 batch_convert.py ~/Games/Roms/INTV ~/Games/Roms/intv2
```

---

## Building from source (C++)

A single native binary — `intv2convert` / `intv2convert.exe` — provides all four
tools as subcommands.  Requires a C++17 compiler and CMake 3.16+.

```
intv2convert rom   <input.rom>  <output_stem>
intv2convert cfg   <input.bin>  <input.cfg>  <output_stem>
intv2convert lst   <input.lst>  <output.intv>  [--pocket]
intv2convert batch <source_dir> <output_dir>  [--dry-run] [--force]
```

### macOS / Linux

```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Windows — MSVC

```
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Windows — MinGW / MSYS2

```
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

The C++ binary and the Python scripts produce identical output and can be used
interchangeably.  The Python scripts require no build step and remain the
quickest option on systems where Python 3 is already present.

---

## Hardware variants

| Year | Hardware | Notes |
|------|----------|-------|
| 2015 | Analogue Nt | Original aluminum FPGA NES. No SD card, no core loading, no jailbreak. |
| Jan 2017 | Analogue Nt Mini | First model with jailbreak support (by Kevtris). SD card loading and additional cores — including Intellivision — became possible. |
| Mar 2020 | Analogue Nt Mini Noir | Final Nt Mini revision. The target hardware for the `-nt-noir.intv` files produced by these tools. |
| Dec 2021 | Analogue Pocket | Dual-FPGA handheld. The target hardware for the `-pocket.intv` files. |

**Nt Mini Noir** — Single Cyclone V. INTV2 files are loaded directly; chunk
headers use true word counts.

**Analogue Pocket** — Cyclone V 5CEFA2F23 (game cores + Chip32 openFPGA) +
Cyclone 10 10CL016 (system OS, display, UI). The Cyclone V runs the Chip32
openFPGA virtual machine alongside game cores, and requires both the load address
and word count in each chunk header to be multiples of 2.

These tools target a platform frozen in time. The Intellivision core on the Nt
Mini was a community jailbreak achievement; the Pocket core arrived through
openFPGA. The Nt Mini Noir is no longer in production; the Analogue Pocket
remains available with periodic restocks.

---

## INTV2 Format

INTV2 is a simple chunked binary format. All multi-byte values are little-endian.

```
For each ROM segment:
    [4 bytes]  load address  (uint32 LE)
    [4 bytes]  word count    (uint32 LE)
    [N×2 bytes] ROM data     (uint16 LE words)

Terminator:
    [4 bytes]  0x00000000
    [4 bytes]  0x00000000
```

For the Pocket variant, any chunk with an odd word count is padded to even by
appending one zero word, and the header word count reflects the padded length.

---

## Input format reference

| Format | Description | Toolchain |
|--------|-------------|-----------|
| `.bin` + `.cfg` | Raw binary image + jzIntv memory-map config | jzIntv, IntyPC |
| `.lst` | as1600 assembler listing | IntyBASIC / as1600 |
| `.rom` | Self-describing ROM image with embedded segment table | jzIntv |

---

## Known limitations

- **Bank-switched ROMs** (`.cfg` files with `PAGE` annotations) cannot be converted.
  INTV2 maps each chunk to a fixed address at load time; there is no runtime page-flip
  mechanism. `make_intv2_from_cfg.py` will detect these and exit with an error.
- `.rom` segments containing Mattel-style bank switching are likewise not supported.
