#!/usr/bin/env python3
"""Prepare locally owned Killer Instinct (arcade 1994) files without publishing them.

Usage: python3 tools/r36s/prepare_data.py /path/to/kinst.zip /path/to/kinst.chd
Requires no pip packages. Uses the expected ROM checksums from tools/check_data.py.
"""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.check_data import KINST  # noqa: E402

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom_zip", type=Path)
    parser.add_argument("chd", type=Path)
    parser.add_argument("--skip-extract", action="store_true", help="Only validate and arrange original files")
    args = parser.parse_args()
    archive, chd = args.rom_zip.resolve(), args.chd.resolve()
    if not archive.is_file() or not chd.is_file():
        parser.error("Both kinst.zip and kinst.chd must exist.")
    if not zipfile.is_zipfile(archive):
        parser.error("First file must be a valid zip archive.")
    if chd.stat().st_size < 10_000_000:
        parser.error("CHD is unexpectedly small. Check that you supplied the full arcade disk image.")

    out = ROOT / "data" / "roms" / "kinst"
    out.mkdir(parents=True, exist_ok=True)
    print("Validating the nine required Killer Instinct ROM files...")
    with zipfile.ZipFile(archive) as z:
        selected = {}
        for name, (size, crc, sha, role) in KINST.items():
            entries = [e for e in z.infolist() if Path(e.filename).name.lower() == name]
            good = None
            for e in entries:
                if e.file_size != size: continue
                payload = z.read(e)
                if hashlib.sha1(payload).hexdigest() == sha:
                    good = payload
                    break
            if good is None:
                sys.exit(f"Missing or wrong revision: {name} ({role}). Expected BIOS L1.5D and kinst sound ROMs.")
            selected[name] = good
            print(f"  OK {name} (SHA1 verified)")
        for name, contents in selected.items():
            (out / name).write_bytes(contents)

    zip_dest = out / "kinst.zip"
    chd_dest = out / "kinst.chd"
    if archive != zip_dest:
        shutil.copyfile(archive, zip_dest)
    if chd != chd_dest:
        print("Copying CHD locally (not to GitHub)...")
        shutil.copyfile(chd, chd_dest)
    else:
        print("CHD is already in the expected local directory.")

    if args.skip_extract:
        print("Data validated. Re-run without --skip-extract to generate rom-{0,1,2}.bin.")
        return

    print("Unpacking original boot ROM through the upstream external kipack tool...")
    subprocess.run(["bash", str(ROOT / "tools" / "extract_game_code.sh")], cwd=str(ROOT), check=True)
    print("Data ready for tools/r36s/build.sh. Do NOT git add data/ or build output.")

if __name__ == "__main__":
    main()
