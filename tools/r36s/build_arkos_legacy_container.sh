#!/usr/bin/env bash
# Build inside quay.io/pypa/manylinux_2_28_aarch64 (AlmaLinux 8 / glibc 2.28).
# Private game inputs must already have been prepared in /source/data/.
# Never run on a handheld. Never install libc or graphics drivers on firmware.
set -Eeuo pipefail
cd "$(dirname "$0")/../.."
test "$(uname -m)" = aarch64
echo "[ARKOS] container glibc: $(getconf GNU_LIBC_VERSION)"
test "$(getconf GNU_LIBC_VERSION)" = "glibc 2.28"
echo "[ARKOS] gcc: $(g++ --version | head -n1)"

# AlmaLinux 8 includes newer toolchain through manylinux GCC 14.
# All dnf operations affect this ephemeral, private CI container only.
if command -v dnf >/dev/null 2>&1; then
  dnf -y install dnf-plugins-core || true
  dnf config-manager --set-enabled powertools 2>/dev/null || true
  dnf -y install --setopt=install_weak_deps=False \
    cmake ninja-build make gcc gcc-c++ git curl tar gzip pkgconf-pkg-config \
    python3-devel libdrm-devel mesa-libgbm-devel \
    mesa-libEGL-devel mesa-libGLES-devel alsa-lib-devel \
    libX11-devel libXext-devel libXrandr-devel libXcursor-devel \
    libXi-devel libXfixes-devel libXinerama-devel libXrender-devel || {
      echo "[ARKOS] dependency installation failed (check AlmaLinux/EPEL repos)." >&2
      exit 10
    }
else
  echo "ERROR: expected manylinux_2_28 container with dnf" >&2
  exit 10
fi
# The manylinux image uses GCC14 and supplies /opt/python. Its python
# interpreter links only against the old 2.28 glibc. Do not use the
# Ubuntu runner's .venv, which requires a newer glibc.
PYTHON=""
for candidate in /opt/python/cp311-cp311/bin/python /opt/python/cp312-cp312/bin/python /usr/bin/python3; do
  if [ -x "$candidate" ]; then PYTHON="$candidate"; break; fi
done
test -n "$PYTHON"
"$PYTHON" -m pip install --disable-pip-version-check 'rabbitizer==1.16.2' 'cmake>=3.24,<4'
CMAKE_BIN="$("$PYTHON" -c 'import cmake; import os; print(os.path.join(cmake.CMAKE_BIN_DIR,"cmake"))')"
echo "[ARKOS] CMake: $("$CMAKE_BIN" --version | head -n1)"
# Package script builds with CMake found in PATH.
export PATH="$(dirname "$CMAKE_BIN"):$PATH"

# Build a modern SDL2 front-end against glibc 2.28 in the legacy container.
# The firmware may have an older SDL2 ABI; do not rely on that version.
# We retain KMSDRM, OpenGLES2, ALSA and joystick support.
SDL_VERSION=2.30.12
mkdir -p build-arkos-deps
curl -fL --retry 3 \
  "https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL2-${SDL_VERSION}.tar.gz" \
  -o build-arkos-deps/SDL2.tar.gz
tar -xzf build-arkos-deps/SDL2.tar.gz -C build-arkos-deps/
test -f "build-arkos-deps/SDL2-${SDL_VERSION}/CMakeLists.txt"
"$PYTHON" tools/r36s/patch_sdl2_kmsdrm_legacy.py \
  "build-arkos-deps/SDL2-${SDL_VERSION}/src/video/kmsdrm/SDL_kmsdrmvideo.c"
export PKG_CONFIG_PATH="/opt/ki-sdl2/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
# KMSDRM must be available, or this package will be unusable for
# handheld direct-screen rendering. Build with dynamic GBM/DRM loading.
cmake -S "build-arkos-deps/SDL2-${SDL_VERSION}" -B build-arkos-deps/sdl-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/ki-sdl2 \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF \
  -DSDL_KMSDRM=ON -DSDL_OPENGLES=ON -DSDL_ALSA=ON \
  -DSDL_X11=ON -DSDL_WAYLAND=OFF
cmake --build build-arkos-deps/sdl-build --parallel 2
cmake --install build-arkos-deps/sdl-build
test -f /opt/ki-sdl2/include/SDL2/SDL.h
test -e /opt/ki-sdl2/lib/libSDL2-2.0.so.0
grep -E -q '^#define SDL_VIDEO_DRIVER_KMSDRM 1' \
  /opt/ki-sdl2/include/SDL2/SDL_config.h || {
    echo "ERROR: legacy SDL2 was built without KMSDRM support!" >&2
    exit 11
  }

export KI_R36S_CMAKE_PREFIX_PATH=/opt/ki-sdl2
export KI_PYTHON="$PYTHON"
export KI_BUILD_TYPE=Release
export KI_R36S_MIPS_O3=ON
export KI_R36S_RELEASE_CLEAN=ON
export KI_R36S_LEGACY_SDL_ABI=ON
export KI_R36S_BUILD_DIR=build-r36s-legacy
export KI_R36S_SKIP_BUILD_RPATH=1
export KI_JOBS=2
bash tools/r36s/build.sh
test -s build-r36s-legacy/ki_play
strip --strip-unneeded build-r36s-legacy/ki_play
readelf -h build-r36s-legacy/ki_play | grep 'Machine:.*AArch64'
if readelf -d build-r36s-legacy/ki_play | grep -E '\((RPATH|RUNPATH)\)'; then
  echo "ERROR: legacy executable contains an unsafe build-container RUNPATH" >&2
  exit 12
fi

# GCC14 C++ ABI cannot be assumed present on ArkOS4Clone. Bundle
# *only* the toolchain C++ libraries and our glibc-2.28 SDL2.
# NEVER bundle libc, ELF loader, libm, system GPU/GBM/EGL or Mesa.
mkdir -p build-arkos-deps/libs.glibc230
cp -L /opt/ki-sdl2/lib/libSDL2-2.0.so.0 build-arkos-deps/libs.glibc230/libSDL2-2.0.so.0
cp -L "$(g++ -print-file-name=libstdc++.so.6)" \
  build-arkos-deps/libs.glibc230/libstdc++.so.6
cp -L "$(gcc -print-file-name=libgcc_s.so.1)" \
  build-arkos-deps/libs.glibc230/libgcc_s.so.1

"$PYTHON" tools/r36s/audit_legacy_abi.py --max-glibc 2.30 \
  build-r36s-legacy/ki_play build-arkos-deps/libs.glibc230
echo "[ARKOS] Finished separate legacy ELF and 3 ABI-compatible libraries"
