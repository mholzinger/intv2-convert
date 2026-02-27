# intv2convert v1.1.0

Adds a native graphical interface to `intv2convert`.  The same binary handles
both GUI and CLI — launch it without arguments for the GUI, or pass a subcommand
to use it as a command-line tool.

## What's new

- **Native GUI** — two-tab interface for Batch conversion and Single File conversion,
  with native OS file/folder pickers on all platforms
- **One binary** — no separate `-gui` executable; `intv2convert` opens its GUI when
  launched from Finder, Explorer, or a desktop shortcut, and behaves as a full CLI
  tool when invoked from a terminal with arguments
- **Windows console attachment** — on Windows the GUI binary automatically attaches
  to the parent terminal when called with arguments, so output appears normally in
  cmd, PowerShell, or any shell

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
