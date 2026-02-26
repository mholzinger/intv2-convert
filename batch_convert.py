#!/usr/bin/env python3
"""
batch_convert.py — Batch-convert a directory of Intellivision ROMs to INTV2 format.

Walks <source> recursively and produces -nt-noir.intv and -pocket.intv files
under <output>, mirroring the source directory structure.  Already-converted
games (both output files present) are skipped.

Usage:
    python3 batch_convert.py <source> <output> [--dry-run] [--force]

    <source>   Directory to walk (e.g. ~/Games/Roms/INTV-Sprint/output)
    <output>   Directory to write .intv files into (created if needed)

    --dry-run  Show what would be converted without writing any files.
    --force    Re-convert games that already have output files.

Source format priority (first match wins):
    1. .rom                  → make_intv2_from_rom.py
    2. .bin + .cfg           → make_intv2_from_cfg.py
    3. .int + .cfg           → make_intv2_from_cfg.py  (.int is identical to .bin)
"""

import os
import sys
import subprocess

TOOLS = os.path.dirname(os.path.abspath(__file__))


def find_source(directory, stem):
    """
    Return a tuple describing how to convert this game stem, or None.

    Returns one of:
        ('rom', rom_path)
        ('cfg', bin_or_int_path, cfg_path)
    """
    rom  = os.path.join(directory, stem + '.rom')
    bin_ = os.path.join(directory, stem + '.bin')
    int_ = os.path.join(directory, stem + '.int')
    cfg  = os.path.join(directory, stem + '.cfg')

    if os.path.exists(rom):
        return ('rom', rom)
    if os.path.exists(bin_) and os.path.exists(cfg):
        return ('cfg', bin_, cfg)
    if os.path.exists(int_) and os.path.exists(cfg):
        return ('cfg', int_, cfg)
    return None


def collect_games(source, target, force):
    """
    Walk <source> and return a list of game records:
        (source_dir, rel_dir, stem, source_fmt, target_dir, already_done)
    """
    games = []
    for dirpath, dirnames, filenames in os.walk(source):
        dirnames.sort()
        rel = os.path.relpath(dirpath, source)
        target_dir = target if rel == '.' else os.path.join(target, rel)

        stems = set()
        for fname in filenames:
            base, ext = os.path.splitext(fname)
            if ext in ('.bin', '.int', '.rom'):
                stems.add(base)

        for stem in sorted(stems):
            src = find_source(dirpath, stem)
            if not src:
                continue
            noir_out   = os.path.join(target_dir, stem + '-nt-noir.intv')
            pocket_out = os.path.join(target_dir, stem + '-pocket.intv')
            already_done = (not force) and os.path.exists(noir_out) and os.path.exists(pocket_out)
            games.append((dirpath, rel, stem, src, target_dir, already_done))

    return games


def convert_game(stem, src, target_dir):
    """Run the appropriate converter. Returns (success, message)."""
    os.makedirs(target_dir, exist_ok=True)
    output_stem = os.path.join(target_dir, stem)

    if src[0] == 'rom':
        cmd = [sys.executable, os.path.join(TOOLS, 'make_intv2_from_rom.py'),
               src[1], output_stem]
    else:
        cmd = [sys.executable, os.path.join(TOOLS, 'make_intv2_from_cfg.py'),
               src[1], src[2], output_stem]

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        err = next((l.strip() for l in result.stderr.splitlines() if l.strip()), '')
        if not err:
            err = next((l.strip() for l in result.stdout.splitlines()
                        if 'Error' in l or 'error' in l), result.stdout.strip())
        return False, err
    return True, ''


def fmt_source(src):
    if src[0] == 'rom':
        return '.rom'
    return os.path.splitext(src[1])[1] + '+.cfg'


def parse_args():
    args    = sys.argv[1:]
    dry_run = '--dry-run' in args
    force   = '--force'   in args
    paths   = [a for a in args if not a.startswith('--')]

    if len(paths) != 2:
        print("Usage: batch_convert.py <source> <output> [--dry-run] [--force]")
        print()
        print("  <source>   Directory to walk for Intellivision ROM files")
        print("  <output>   Directory to write converted .intv files into")
        print("  --dry-run  Show what would be converted without writing files")
        print("  --force    Re-convert games that already have output files")
        sys.exit(1)

    source = os.path.realpath(os.path.expanduser(paths[0]))
    target = os.path.realpath(os.path.expanduser(paths[1]))

    if not os.path.isdir(source):
        print(f"Error: source directory not found: {source}")
        sys.exit(1)

    return source, target, dry_run, force


def main():
    source, target, dry_run, force = parse_args()

    games   = collect_games(source, target, force)
    pending = [(d, r, s, src, td) for d, r, s, src, td, done in games if not done]
    skipped = [(d, r, s, src, td) for d, r, s, src, td, done in games if done]

    print(f"Source: {source}")
    print(f"Output: {target}")
    print(f"{'DRY RUN — ' if dry_run else ''}Found {len(games)} convertible games")
    print(f"  {len(skipped)} already done, {len(pending)} to convert")
    print()

    if not pending:
        print("Nothing to do.")
        return

    if dry_run:
        print("Would convert:")
        for _, rel, stem, src, _ in pending:
            label = os.path.join(rel, stem) if rel != '.' else stem
            print(f"  [{fmt_source(src):9s}]  {label}")
        print()
        print("Re-run without --dry-run to write files.")
        return

    converted = 0
    failed    = []

    for _, rel, stem, src, target_dir in pending:
        label = os.path.join(rel, stem) if rel != '.' else stem
        ok, err = convert_game(stem, src, target_dir)
        if ok:
            print(f"  OK   {label}  ({fmt_source(src)})")
            converted += 1
        else:
            print(f"  FAIL {label}  ({fmt_source(src)})")
            if err:
                print(f"       {err}")
            failed.append((label, err))

    print()
    print(f"Done: {converted} converted, {len(failed)} failed, {len(skipped)} skipped.")

    if failed:
        print()
        print("Failed games:")
        for label, err in failed:
            print(f"  {label}")
            if err:
                print(f"    {err}")


if __name__ == '__main__':
    main()
