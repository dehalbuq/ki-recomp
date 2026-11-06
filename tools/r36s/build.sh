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

BUILD_FOLDER="${KI_R36S_BUILD_DIR:-build-r36s}"
if [[ ! "$BUILD_FOLDER" =~ ^[A-Za-z0-9_.-]+$ ]]; then
  echo "KI_R36S_BUILD_DIR must be a simple local folder name" >&2
  exit 2
fi
BUILD_DIR="$ROOT/$BUILD_FOLDER"
args=(-S "$ROOT" -B "$BUILD_DIR" -G Ninja
  "-DCMAKE_BUILD_TYPE=${KI_BUILD_TYPE:-Debug}"
  -DKI_R36S=ON -DKI_BUILD_FULL=ON -DKI_BUILD_TRACE=OFF -DKI_WITH_CHD=ON
  -DKI_SPLIT=32 "-DKI_PYTHON=$PYTHON_BIN")

# Preserve the validated Test17 MIPS compiler choice in the Test18 builds.
# Default remains OFF for conservative/manual builds; the private Test18
# workflow explicitly uses ON to match the measured -O3 performance.
MIPS_O3="${KI_R36S_MIPS_O3:-OFF}"
case "$MIPS_O3" in
  ON|OFF) ;;
  *) echo "KI_R36S_MIPS_O3 must be ON or OFF, got: $MIPS_O3" >&2; exit 2 ;;
esac
args+=("-DKI_R36S_MIPS_O3=$MIPS_O3")
RELEASE_CLEAN="${KI_R36S_RELEASE_CLEAN:-OFF}"
case "$RELEASE_CLEAN" in ON|OFF) ;; *) echo "KI_R36S_RELEASE_CLEAN must be ON or OFF" >&2; exit 2;; esac
args+=("-DKI_R36S_RELEASE_CLEAN=$RELEASE_CLEAN")
args+=("-DKI_R36S_LEGACY_SDL_ABI=${KI_R36S_LEGACY_SDL_ABI:-OFF}")
echo "R36S build: folder=$BUILD_FOLDER, type=${KI_BUILD_TYPE:-Debug}, generated MIPS O3=$MIPS_O3, clean release=$RELEASE_CLEAN"


# Both C and C++ MUST target AArch64. Otherwise libchdr's C objects end
# up as x86_64 (EM=62) and the AArch64 linker fails with "file in wrong format".
if [ "$(uname -m)" = "aarch64" ]; then
  CC_TARGET="${KI_CC:-cc}"
  CXX_TARGET="${KI_CXX:-c++}"
else
  CC_TARGET="${KI_CC:-aarch64-linux-gnu-gcc}"
  CXX_TARGET="${KI_CXX:-aarch64-linux-gnu-g++}"
  args+=(-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64)
fi

for compiler in "$CC_TARGET" "$CXX_TARGET"; do
  if ! command -v "$compiler" >/dev/null 2>&1; then
    echo "Missing compiler: $compiler" >&2
    echo "On Kali: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu" >&2
    exit 2
  fi
  if [[ "$("$compiler" -dumpmachine)" != aarch64* ]]; then
    echo "Wrong target for $compiler: $("$compiler" -dumpmachine) (expected AArch64)" >&2
    exit 2
  fi
done
CC_PATH="$(command -v "$CC_TARGET")"
CXX_PATH="$(command -v "$CXX_TARGET")"
args+=("-DCMAKE_C_COMPILER=$CC_PATH" "-DCMAKE_CXX_COMPILER=$CXX_PATH")
if [ -n "${KI_R36S_CMAKE_PREFIX_PATH:-}" ]; then
  args+=("-DCMAKE_PREFIX_PATH=$KI_R36S_CMAKE_PREFIX_PATH")
fi
if [ "${KI_R36S_SKIP_BUILD_RPATH:-0}" = 1 ]; then
  args+=("-DCMAKE_SKIP_BUILD_RPATH=ON")
fi
echo "Using C compiler: $CC_PATH ($("$CC_TARGET" -dumpmachine))"
echo "Using C++ compiler: $CXX_PATH ($("$CXX_TARGET" -dumpmachine))"

# CMake cannot safely switch compilers in an existing build tree. Detect the
# first (broken) x86_64 CMake cache rather than repeatedly failing at link.
CACHE="$BUILD_DIR/CMakeCache.txt"
if [ -f "$CACHE" ]; then
  for kind in C CXX; do
    key="CMAKE_${kind}_COMPILER"
    cached="$(sed -n "s/^${key}:[^=]*=//p" "$CACHE" | head -n 1)"
    if [ "$kind" = C ]; then desired="$CC_PATH"; else desired="$CXX_PATH"; fi
    if [ -n "$cached" ] && [ "$(readlink -f "$cached")" != "$(readlink -f "$desired")" ]; then
      echo "Incompatible existing CMake cache: $key=$cached" >&2
      echo "Expected: $desired" >&2
      echo "Fix (only removes generated objects): rm -rf $BUILD_FOLDER" >&2
      echo "Then run: bash tools/r36s/build.sh" >&2
      exit 3
    fi
  done
fi

if [ -n "${KI_SYSROOT:-}" ]; then
  args+=("-DCMAKE_SYSROOT=$KI_SYSROOT")
  export PKG_CONFIG_SYSROOT_DIR="$KI_SYSROOT"
elif [ "$(uname -m)" != "aarch64" ]; then
  echo "WARNING: cross-building without KI_SYSROOT; host SDL2/glibc may not match the handheld." >&2
fi

cmake "${args[@]}"
cmake --build "$BUILD_DIR" --target ki_play --parallel "${KI_JOBS:-2}"
file "$BUILD_DIR/ki_play"
if command -v readelf >/dev/null 2>&1; then
  echo "--- Dynamic libraries ---"
  readelf -d "$BUILD_DIR/ki_play" | grep NEEDED || true
  echo "--- Highest GLIBC symbol versions ---"
  readelf --version-info "$BUILD_DIR/ki_play" | grep -oE 'GLIBC_[0-9]+(\.[0-9]+)+' | sort -Vu | tail -8 || true
fi
echo "Full binary compiled, NOT YET validated on R36S hardware."
