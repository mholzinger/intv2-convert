# intv2convert v1.2.0

Switches the Windows GUI from OpenGL to **DirectX 11**, eliminating the most
common failure case: VMs and machines with minimal GPU drivers that don't expose
an OpenGL 3.0 context.  macOS and Linux continue to use GLFW + OpenGL and are
unchanged.

## What's new

- **DirectX 11 on Windows** — the GUI no longer requires an OpenGL driver.  DX11
  works on every Windows machine including virtual machines (VMware, VirtualBox,
  Parallels, Hyper-V).
- **WARP software renderer fallback** — if no DX11 hardware is available,
  `intv2convert` automatically falls back to Windows' built-in CPU software
  renderer (WARP).  The GUI opens on every Windows 8+ machine, no exceptions.
- **Smaller Windows binary** — GLFW is no longer compiled or linked on Windows,
  reducing the executable size.

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
