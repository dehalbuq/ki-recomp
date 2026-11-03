# R36S / RK3326 experimental port

Branch: `port/r36s-portmaster`. Original project by [kilnan](https://github.com/kilnan/ki-recomp) (MIT). This port does **not** ship game assets. R36S changes are guarded by `-DKI_R36S=ON`, so default upstream builds remain unchanged.

## What is implemented

The SDL2 player selects full-screen 4:3, nearest-neighbor scaling for its native 320x240 framebuffer, and has accelerated-renderer initialization checks with diagnostic software fallback. Gamepad Start+Select (SDL BACK or GUIDE) held ~1.2 s safely exits. Existing arcade timing (~58.98 Hz) is kept unchanged. No HD pack is recommended.

## Required arcade data (not included)

Place the original MAME Killer Instinct `kinst` ROMs and `kinst.chd` in `data/roms/kinst/`, including `ki-l15d.u98`, as documented in the root README. Original ROMs are also needed to generate recompiled C++ code, which must stay out of public git history.

From Linux with required packages, install Python/rabbitizer in a venv:

```sh
python3 -m venv .venv
.venv/bin/pip install rabbitizer==1.16.2
./tools/extract_game_code.sh
```

This runs an external kipack utility from ki-rom to produce `data/kipack-out/rom-{0,1,2}.bin`. Follow the upstream README for dependencies and source licensing.

## Build

Use an AArch64 Linux environment (or Kali Linux x86_64 with `aarch64-linux-gnu-g++` and an **R36S-compatible AArch64 sysroot**). The firmware's glibc, libstdc++, SDL2 and graphics stack must be compatible; a generic Debian ARM64 binary may not run on ArkOS4Clone or DarkOSre.

```sh
bash tools/r36s/build.sh
# Or with a target sysroot on x86_64:
KI_CXX=aarch64-linux-gnu-g++ KI_SYSROOT=/path/to/r36s/sysroot bash tools/r36s/build.sh
```

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
