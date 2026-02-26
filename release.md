# intv2convert v1.0.1

First public release.

Converts Intellivision ROM files to **INTV2** format for the **Analogue Nt Mini Noir** and **Analogue Pocket** FPGA cores.

## Downloads

| Asset | Platform |
|-------|---------|
| `intv2convert-windows-x64.zip` | Windows — no install required, just extract and run |
| `intv2convert-macos-universal.tar.gz` | macOS — universal binary (Apple Silicon + Intel) |
| `intv2convert-linux-x64.tar.gz` | Linux x64 |
| `intv2convert-python.zip` | All platforms — Python 3.6+ scripts, no build needed |

## Subcommands

```
intv2convert rom   <input.rom>  <output_stem>
intv2convert cfg   <input.bin>  <input.cfg>  <output_stem>
intv2convert lst   <input.lst>  <output.intv>  [--pocket]
intv2convert batch <source_dir> <output_dir>  [--dry-run] [--force]
```

Each converter writes two output files — `*-nt-noir.intv` and `*-pocket.intv` — except `lst` which takes an explicit output path and an optional `--pocket` flag.

## Notes

- The native binary and the Python scripts produce **byte-identical output** and are interchangeable
- Bank-switched ROMs (`.cfg` files with `PAGE` annotations) are not supported — INTV2 is a static load format
