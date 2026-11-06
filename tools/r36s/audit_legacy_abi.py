#!/usr/bin/env python3
"""Refuse any ArkOS4Clone ELF requiring GLIBC newer than the target.

This intentionally audits *every bundled* ELF library, not merely the
player. It does not claim that runtime SDL2/DRM/GLES support is validated:
only a real ArkOS4Clone run can establish that.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys

V_GLIBC = re.compile(r"(?<![A-Z0-9_])GLIBC_(\d+)\.(\d+)(?:\.(\d+))?\b")
V_GLIBCXX = re.compile(r"(?<![A-Z0-9_])GLIBCXX_(\d+)\.(\d+)(?:\.(\d+))?\b")
V_CXXABI = re.compile(r"(?<![A-Z0-9_])CXXABI_(\d+)\.(\d+)(?:\.(\d+))?\b")

def version(v):
    return tuple(int(x or 0) for x in v)

def fmt(v):
    return ".".join(map(str, v)).rstrip(".0") if v else "none"

def elf_files(paths):
    found = []
    for path in paths:
        p = Path(path)
        if not p.exists():
            raise SystemExit(f"Missing ELF input: {p}")
        candidates = sorted(p.rglob("*")) if p.is_dir() else [p]
        for f in candidates:
            if f.is_file() and f.open("rb").read(4) == b"\x7fELF":
                found.append(f)
    if not found:
        raise SystemExit("No ELF files found")
    return found

def readelf(path, flag):
    p = subprocess.run(["readelf", "-W", flag, str(path)],
                       capture_output=True, text=True)
    if p.returncode:
        raise RuntimeError(f"readelf {flag} failed for {path}: {p.stderr}")
    return p.stdout

def maximum(pattern, source):
    matches = [version(m.groups()) for m in pattern.finditer(source)]
    return max(matches) if matches else (0, 0, 0)

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--max-glibc", default="2.30")
    p.add_argument("--max-glibcxx", default=None,
                   help="Optional requirement for SYSTEM libstdc++; omit when bundled")
    p.add_argument("path", nargs="+", help="Player ELF and any bundled library folders")
    args = p.parse_args()
    glibc_cap = version(args.max_glibc.split("."))
    cxx_cap = version(args.max_glibcxx.split(".")) if args.max_glibcxx else None
    if len(glibc_cap) == 2: glibc_cap += (0,)
    if cxx_cap and len(cxx_cap) == 2: cxx_cap += (0,)
    failed = False
    for f in elf_files(args.path):
        header = readelf(f, "-h")
        if "AArch64" not in header:
            print(f"FAIL {f}: not AArch64", file=sys.stderr)
            failed = True
            continue
        versions = readelf(f, "--version-info")
        deps = readelf(f, "-d")
        glibc = maximum(V_GLIBC, versions)
        glibcxx = maximum(V_GLIBCXX, versions)
        cxxabi = maximum(V_CXXABI, versions)
        if glibc > glibc_cap or (cxx_cap and glibcxx > cxx_cap):
            failed = True
            print(f"FAIL {f}: GLIBC {glibc}, GLIBCXX {glibcxx}, "
                  f"allowed {glibc_cap}/{cxx_cap}", file=sys.stderr)
        if "GLIBC_PRIVATE" in versions:
            print(f"FAIL {f}: uses private GLIBC symbols", file=sys.stderr)
            failed = True
        if re.search(r"\((RPATH|RUNPATH)\)", deps) and (
            "/opt/" in deps or "/usr/local/" in deps):
            print(f"FAIL {f}: contains build-container library search path",
                  file=sys.stderr)
            failed = True
        print(f"{f}: GLIBC={glibc} GLIBCXX={glibcxx} CXXABI={cxxabi}")
    if failed:
        raise SystemExit(1)
    print(f"Legacy AArch64 ABI PASS: GLIBC requirements <= {args.max_glibc}")
if __name__ == "__main__":
    main()
