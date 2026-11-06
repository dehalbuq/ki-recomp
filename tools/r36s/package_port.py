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
p.add_argument("--libs", type=Path, help="Modern firmware-compatible non-glibc library directory")
p.add_argument("--legacy-binary", type=Path,
               help="Optional separate ArkOS4Clone/GLIBC 2.28 AArch64 ELF")
p.add_argument("--legacy-libs", type=Path,
               help="ArkOS4Clone-compatible SDL2, libstdc++ and libgcc_s directory")
p.add_argument("--release", action="store_true", help="Lean first release, only game launcher; no diagnostics/scripts")
args = p.parse_args()

if not args.binary.is_file():
    sys.exit("No executable was found. You must compile with your own legitimate game files first.")
head = args.binary.read_bytes()[:20]
if len(head) < 20 or head[:4] != b"\x7fELF" or head[5] != 1 or int.from_bytes(head[18:20], "little") != 183:
    sys.exit("The executable must be a little-endian Linux AArch64 ELF (EM_AARCH64=183).")
if bool(args.legacy_binary) != bool(args.legacy_libs):
    sys.exit("Dual ABI package needs both --legacy-binary and --legacy-libs")
if args.legacy_binary and not args.release:
    sys.exit("Dual ABI packaging requires --release")
if args.legacy_binary:
    if not args.legacy_binary.is_file():
        sys.exit("Legacy AArch64 executable is missing")
    legacy_head = args.legacy_binary.read_bytes()[:20]
    if len(legacy_head) < 20 or legacy_head[:4] != b"\x7fELF":
        sys.exit("Invalid legacy ELF")
    # The two releases must be real native ARM64, not a cross-arch stub.
    if legacy_head[5] != 1 or int.from_bytes(legacy_head[18:20], "little") != 183:
        sys.exit("Legacy executable is not AArch64")
if args.libs:
    if not args.libs.is_dir():
        sys.exit("--libs must point to a directory")
    forbidden = ("libc.so", "ld-linux", "libpthread.so", "libdl.so", "librt.so")
    bad = [f.name for f in args.libs.rglob("*") if f.is_file() and f.name.startswith(forbidden)]
    if bad:
        sys.exit("Do not bundle glibc/loader: " + ", ".join(bad))

if args.legacy_libs:
    if not args.legacy_libs.is_dir():
        sys.exit("--legacy-libs must be a directory")
    names = {p.name for p in args.legacy_libs.rglob("*") if p.is_file()}
    for required in ("libSDL2-2.0.so.0", "libstdc++.so.6", "libgcc_s.so.1"):
        if required not in names:
            sys.exit("Legacy directory is missing " + required)
    forbidden = ("libc.so", "ld-linux", "libpthread.so", "libdl.so", "librt.so", "libm.so")
    bad = [name for name in names if name.startswith(forbidden)]
    if bad:
        sys.exit("Do not bundle libc/loader/misc system libs: " + ", ".join(bad))
    # Refuse a modern binary masquerading as an ArkOS4Clone build.
    import subprocess
    audit = [sys.executable, str(HERE / "audit_legacy_abi.py"),
             "--max-glibc", "2.30", str(args.legacy_binary),
             str(args.legacy_libs)]
    if subprocess.call(audit) != 0:
        sys.exit("ArkOS4Clone ABI audit failed; refusing to make dual ZIP")

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
    if args.legacy_binary:
        shutil.copy2(args.legacy_binary, folder / "ki_play.glibc230.aarch64")
        shutil.copytree(args.legacy_libs, folder / "libs.glibc230")
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
    if args.legacy_binary:
        os.chmod(folder / "ki_play.glibc230.aarch64", 0o755)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as z:
        for file in sorted(dest.rglob("*")):
            if file.is_file():
                z.write(file, str(file.relative_to(dest)))
print(f"Created {'dual ArkOS4Clone/dArkOSRE' if args.legacy_binary else 'single'} "
      f"{'clean release' if args.release else 'prototype'} (no ROMs): {args.output}")
