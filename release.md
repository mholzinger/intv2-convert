# intv2convert v1.2.1

Patch release fixing a silent data-corruption bug in bank-switch detection.

## Bug fix

- **PAGE detection now catches lettered pages** — `.cfg` files that use `PAGE A`,
  `PAGE B`, `PAGE C`, etc. (as opposed to `PAGE 1`, `PAGE 2`, …) were not
  recognised as bank-switched.  They were silently converted to corrupt INTV2
  files with dozens of overlapping chunks instead of being rejected with an error.
  The fix applies to both the Python scripts and the native C++ binary.

  **Affected ROMs** include titles that use Intellicart bank-switching with
  alphabetic page labels (e.g. *Sorrow of Gadhlan Thur*).  If you converted any
  such ROMs with v1.2.0 or earlier, delete the output files and reconvert — the
  tool will now correctly refuse them with a clear error message.

## Downloads

| Asset | Platform |
|-------|---------|
| `intv2convert-windows-x64.zip` | Windows — no install required, just extract and run |
| `intv2convert-macos-universal.tar.gz` | macOS — universal binary (Apple Silicon + Intel) |
| `intv2convert-linux-x64.tar.gz` | Linux x64 |
| `intv2convert-python.zip` | All platforms — Python 3.6+ scripts, no build needed |

## CLI usage

```
intv2convert rom   <input.rom>  <output_stem>
intv2convert cfg   <input.bin>  <input.cfg>  <output_stem>
intv2convert lst   <input.lst>  <output.intv>  [--pocket]
intv2convert batch <source_dir> <output_dir>  [--dry-run] [--force]
```

## Notes

- macOS: Gatekeeper will block the binary on first launch — see README for the
  one-time `xattr` workaround
- The native binary and the Python scripts produce **byte-identical output**
- Bank-switched ROMs (`.cfg` files with `PAGE` annotations) are not supported
