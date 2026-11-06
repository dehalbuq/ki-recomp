#!/usr/bin/env python3
"""Prototype packaging: requires a real AArch64 ELF, never packages ROMs or CHD."""
import argparse
import os
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]

p = argparse.ArgumentParser()
p.add_argument("binary", type=Path, help="Path to build-r36s/ki_play")
p.add_argument("--output", type=Path, default=ROOT / "killerinstinct-r36s-prototype.zip")
p.add_argument("--libs", type=Path, help="Firmware-compatible non-glibc library directory")
p.add_argument("--release", action="store_true", help="Lean first release, only game launcher; no diagnostics/scripts")
args = p.parse_args()

if not args.binary.is_file():
    sys.exit("No executable was found. You must compile with your own legitimate game files first.")
head = args.binary.read_bytes()[:20]
if len(head) < 20 or head[:4] != b"\x7fELF" or head[5] != 1 or int.from_bytes(head[18:20], "little") != 183:
    sys.exit("The executable must be a little-endian Linux AArch64 ELF (EM_AARCH64=183).")
if args.libs:
    if not args.libs.is_dir():
        sys.exit("--libs must point to a directory")
    forbidden = ("libc.so", "ld-linux", "libpthread.so", "libdl.so", "librt.so")
    bad = [f.name for f in args.libs.rglob("*") if f.is_file() and f.name.startswith(forbidden)]
    if bad:
        sys.exit("Do not bundle glibc/loader: " + ", ".join(bad))

template = HERE / "portmaster"
with tempfile.TemporaryDirectory(prefix="ki-r36s-port-") as td:
    dest = Path(td)
    for entry in template.iterdir():
        if entry.is_file():
            if args.release and entry.suffix == ".sh" and entry.name != "Killer Instinct Arcade.sh":
                continue
            shutil.copy2(entry, dest / entry.name)
    if args.release:
        shutil.copy2(HERE / "RELEASE_README.md", dest / "README.md")
    folder = dest / "killerinstinct"
    (folder / "gamedata").mkdir(parents=True)
    (folder / "logs").mkdir()
    (folder / "conf").mkdir()
    shutil.copy2(args.binary, folder / "ki_play.aarch64")
    shutil.copy2(ROOT / "LICENSE", folder / "LICENSE.ki-recomp.txt")
    shutil.copy2(ROOT / "third_party/adsp2100/LICENSE", folder / "LICENSE.ADSPR2105.txt")
    (folder / "gamedata" / "COPY_YOUR_OWN_GAME_FILES_HERE.txt").write_text(
        "Place your lawful kinst.zip and kinst.chd here. No copyrighted game data included.\n"
    )
    if args.libs:
        shutil.copytree(args.libs, folder / "libs.aarch64")
    # All standalone PortMaster benchmark entries are launchable scripts,
    # not just the main game entry.
    for launcher in dest.glob("*.sh"):
        os.chmod(launcher, 0o755)
    os.chmod(folder / "ki_play.aarch64", 0o755)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as z:
        for file in sorted(dest.rglob("*")):
            if file.is_file():
                z.write(file, str(file.relative_to(dest)))
print(f"Created {'v0.1.0 clean release' if args.release else 'prototype'} (no ROMs): {args.output}")
