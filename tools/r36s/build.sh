#!/usr/bin/env bash
# Local-only full build: needs a legitimately obtained Killer Instinct arcade set.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
for asset in data/roms/kinst/ki-l15d.u98 data/kipack-out/rom-0.bin data/kipack-out/rom-1.bin data/kipack-out/rom-2.bin; do
  if [ ! -f "$asset" ]; then
    echo "Missing $asset. Read docs/R36S_PORT.md and run tools/extract_game_code.sh locally." >&2
    exit 2
  fi
done

PYTHON_BIN="${KI_PYTHON:-$ROOT/.venv/bin/python}"
if [ ! -x "$PYTHON_BIN" ]; then
  echo "Python + rabbitizer environment missing: $PYTHON_BIN" >&2
  exit 2
fi

args=(-S "$ROOT" -B "$ROOT/build-r36s" -G Ninja
  "-DCMAKE_BUILD_TYPE=${KI_BUILD_TYPE:-Debug}"
  -DKI_R36S=ON -DKI_BUILD_FULL=ON -DKI_BUILD_TRACE=OFF -DKI_WITH_CHD=ON
  -DKI_SPLIT=32 "-DKI_PYTHON=$PYTHON_BIN")

if [ -n "${KI_CXX:-}" ]; then
  args+=("-DCMAKE_CXX_COMPILER=$KI_CXX")
elif [ "$(uname -m)" != "aarch64" ]; then
  command -v aarch64-linux-gnu-g++ >/dev/null || { echo "Missing aarch64-linux-gnu-g++" >&2; exit 2; }
  args+=("-DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++")
fi

if [ -n "${KI_SYSROOT:-}" ]; then
  args+=("-DCMAKE_SYSROOT=$KI_SYSROOT")
  export PKG_CONFIG_SYSROOT_DIR="$KI_SYSROOT"
elif [ "$(uname -m)" != "aarch64" ]; then
  echo "WARNING: cross-building without KI_SYSROOT; host SDL2/glibc may not match the handheld." >&2
fi

cmake "${args[@]}"
cmake --build "$ROOT/build-r36s" --target ki_play --parallel "${KI_JOBS:-2}"
file "$ROOT/build-r36s/ki_play"
if command -v readelf >/dev/null 2>&1; then
  echo "--- Dynamic libraries ---"
  readelf -d "$ROOT/build-r36s/ki_play" | grep NEEDED || true
  echo "--- Highest GLIBC symbol versions ---"
  readelf --version-info "$ROOT/build-r36s/ki_play" | grep -oE 'GLIBC_[0-9]+(\.[0-9]+)+' | sort -Vu | tail -8 || true
fi
echo "Full binary compiled, NOT YET validated on R36S hardware."
