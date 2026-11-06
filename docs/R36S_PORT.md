# R36S / RK3326 experimental port

Branch: `port/r36s-portmaster`. Original project by [kilnan](https://github.com/kilnan/ki-recomp) (MIT). This port does **not** ship game assets. R36S changes are guarded by `-DKI_R36S=ON`, so default upstream builds remain unchanged.

## What is implemented

The SDL2 player selects full-screen 4:3, nearest-neighbor scaling for its native 320x240 framebuffer, and has accelerated-renderer initialization checks with diagnostic software fallback. Gamepad Start+Select (SDL BACK or GUIDE) held ~1.2 s safely exits. Existing arcade timing (~58.98 Hz) is kept unchanged. No HD pack is recommended.

## Required arcade data (not included)

Place the original MAME Killer Instinct `kinst` ROMs and `kinst.chd` in `data/roms/kinst/`, including `ki-l15d.u98`, as documented in the root README. Original ROMs are also needed to generate recompiled C++ code, which must stay out of public git history.

On Linux (e.g. Kali x86_64), install the build-host tools and prepare the original game set:

```sh
sudo apt update
sudo apt install -y git gcc g++ make cmake ninja-build python3-venv pkg-config unzip gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
python3 -m venv .venv
.venv/bin/pip install rabbitizer==1.16.2
python3 tools/r36s/prepare_data.py "/path/to/kinst.zip" "/path/to/kinst.chd"
```

The preparation helper verifies all nine ROMs by SHA1, copies the player's own files locally into `data/roms/kinst/`, and calls the external `kipack` utility from ki-rom. That produces `data/kipack-out/rom-{0,1,2}.bin`. The raw CHD content is checked later when the full player loads it. Neither original assets nor game-derived generated C++ go in the public repository. See the upstream README for licensing and further dependencies.

## Build

Use an AArch64 Linux environment (or Kali Linux x86_64 with `aarch64-linux-gnu-g++` and an **R36S-compatible AArch64 sysroot**). The firmware's glibc, libstdc++, SDL2 and graphics stack must be compatible; a generic Debian ARM64 binary may not run on ArkOS4Clone or DarkOSre.

```sh
# If a previous attempt used host /usr/bin/cc, clean generated build outputs only:
rm -rf build-r36s

# The script now selects BOTH C and C++ AArch64 cross-compilers:
bash tools/r36s/build.sh

# Or with a target sysroot on x86_64:
KI_CC=aarch64-linux-gnu-gcc KI_CXX=aarch64-linux-gnu-g++ \
  KI_SYSROOT=/path/to/r36s/sysroot bash tools/r36s/build.sh
```

**Cross-architecture link failure:** If the linker reports `Relocations in generic ELF (EM: 62)` or `file in wrong format`, C libraries such as libchdr were compiled as x86_64. Never mix native `cc` with `aarch64-linux-gnu-g++`. The corrected script sets both compiler paths, checks their `-dumpmachine` output, and rejects old CMake caches. `rm -rf build-r36s` affects generated objects only, not `data/roms/kinst` or `data/kipack-out`.

The script builds a debug `build-r36s/ki_play` by default and prints ELF library/GLIBC requirements. Change `KI_BUILD_TYPE=Release` only after a working debug test. On cross compilation also set target-appropriate `PKG_CONFIG_LIBDIR`/`PKG_CONFIG_PATH` if SDL2 detection picks up host libraries. **Never bundle random glibc libraries** to overcome mismatches.

## Package

```sh
python3 tools/r36s/package_port.py build-r36s/ki_play
```

The script verifies a real little-endian AArch64 ELF and creates a prototype ZIP without game data, using files under `tools/r36s/portmaster/`. Extract the launcher and game directory to `/roms/ports/` (or your firmware's configured PortMaster ports directory), then copy your `kinst.zip` and `kinst.chd` into `killerinstinct/gamedata/` locally. No dummy executable is created.

Collect firmware information on the handheld using:

```sh
bash tools/r36s/diagnose.sh /roms/ports/killerinstinct/logs/firmware.txt /roms/ports/killerinstinct/ki_play.aarch64
```

Review the diagnostic before sharing; the log contains machine/library details, not ROM data. Also inspect `killerinstinct/logs/ki-r36s.log`.

## CI scope and pending validation

The ARM64 GitHub Actions workflow can build the guest-independent runtime self-test and syntax-check the SDL2 player **without copyrighted ROMs**. It does **not** produce a playable KI binary, verify 58.98 FPS, prove glibc compatibility, or validate controls, audio and graphics on R36S hardware.

The PortMaster packaging is a **prototype** pending real tests and the required gameplay screenshot for public submission. Do not release ROMs, CHD, extracted game data, or generated game-derived source. Honor the original MIT and third-party BSD licenses.

## Dual AArch64 ABI package — dArkOSRE and ArkOS4Clone candidate

A single root `Killer Instinct Arcade.sh` now determines the runtime
firmware glibc via `getconf GNU_LIBC_VERSION` and chooses:

| Firmware GLIBC version | Executable | Libraries |
|---|---|---|
| 2.38 or newer | `killerinstinct/ki_play.aarch64` | firmware native (or `libs.aarch64/` if included) |
| 2.28–2.37 | `killerinstinct/ki_play.glibc230.aarch64` | `killerinstinct/libs.glibc230/` |
| earlier/unknown | launcher stops with explanation | none |

The legacy binary is **compiled separately** with GCC 14 and
C++20 inside `quay.io/pypa/manylinux_2_28_aarch64` (AlmaLinux 8 /
GLIBC 2.28). The private container builds a compatible SDL2 with
KMSDRM/OpenGLES2 and bundles only the old-ABI SDL2, libstdc++ and
libgcc_s. It does not bundle or change system libc, ELF loader, GBM,
EGL or GPU drivers.

Build both executables **only in a private environment** with
your own legitimate game files:

```sh
KI_BUILD_TYPE=Release KI_R36S_MIPS_O3=ON \
 KI_R36S_RELEASE_CLEAN=ON bash tools/r36s/build.sh
docker run --rm --platform linux/arm64 \
 -v "$PWD:/src" -w /src quay.io/pypa/manylinux_2_28_aarch64 \
 bash tools/r36s/build_arkos_legacy_container.sh
python3 tools/r36s/audit_legacy_abi.py --max-glibc 2.30 \
 build-r36s-legacy/ki_play build-arkos-deps/libs.glibc230
python3 tools/r36s/package_port.py build-r36s/ki_play --release \
 --legacy-binary build-r36s-legacy/ki_play \
 --legacy-libs build-arkos-deps/libs.glibc230 \
 --output KI-R36S-v0.1.0-DUAL-ARM64.zip
```

The ZIP contains both native AArch64 binaries and just **one**
PortMaster launcher, with startup credits and legal disclaimer.
`ki_play.glibc230.aarch64` is intended for ArkOS4Clone but needs
a real test on that firmware: `readelf` verifies exported symbol
requirements, **not** drivers, kernel ABI, audio or controls.
To test selection without launching, developers may set
`KI_R36S_ABI_SELECT_ONLY=1`. Run `bash
tests/r36s_dual_abi_launcher_selftest.sh` for ROM-free routing
regressions.

The older-ABI binary may still require device-specific SDL2/KMSDRM
drivers or other firmware libraries. Do not declare it ArkOS4Clone
compatible until boot, gameplay, sound and save behavior have been
verified on real hardware. Redistribution of the executable requires
separate rights review because the recompiled binary can contain
game-derived copyrighted code.

## ArkOS4Clone troubleshooting — SDL2 GBM/EGL window failure

One ArkOS4Clone log (kernel 4.4.189, AArch64, glibc 2.30) proved
that the ABI selector correctly chose `ki_play.glibc230.aarch64`.
The actual failure occurred **before the game files were opened**:

```text
SDL_CreateWindow failed: Can't window GBM/EGL surfaces on window creation.
```

This is **not** a MIPS/recompiler crash or a reason to update the
firmware's glibc. It is SDL2/KMSDRM's EGL/GBM window negotiation.
SDL2 2.30.12's original KMSDRM backend hardcodes ARGB8888, which
not all Mali GBM/EGL implementations support. The experimental
correction:
- Compiles the legacy SDL2 with a localized retry across
  XRGB8888, ARGB8888 and RGB565 visual/scanout formats.
- Starts the *legacy* launcher with `SDL_VIDEODRIVER=kmsdrm`,
  `SDL_VIDEO_FORCE_EGL=1` and `SDL_RENDER_DRIVER=opengles2`
  unless the firmware/user already specifies a driver.
- Reports SDL2 version, selected backend and the reason each GBM
  format failed to `killerinstinct/logs/ki-r36s.log`.
- On this specific pre-game window failure, automatically retries
  once with the firmware's own SDL2, while keeping old-ABI C++
  libraries and without touching GPU/EGL/GLIBC system files.
- Avoids newer SDL_RenderSetVSync/SDL_SetTextureScaleMode symbol
  imports from the legacy game executable so older firmware SDL2
  is a feasible fallback.

This is an **experimental firmware compatibility fix**, not evidence
that it is already working on ArkOS4Clone hardware. Never install
new `libEGL.so`, `libgbm.so`, `libmali.so` or libc files into
the handheld root filesystem to fix this error. If a new launch still
fails, provide both `ki-r36s.log` and
`ki-r36s-legacy-video-attempt.log` from the game's logs directory.
The latter records the exact surface failure in the first attempt.
