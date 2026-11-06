# Contributing to the Killer Instinct R36S ARM64 port

Obrigado pelo interesse! / Thanks for helping preserve an accurate,
playable version of the 1994 arcade **Killer Instinct** on RK3326
handhelds. This repository is a community, experimental fork of
[kilnan/ki-recomp](https://github.com/kilnan/ki-recomp).

**Please contribute to [`port/r36s-portmaster`](https://github.com/dehalbuq/ki-recomp/tree/port/r36s-portmaster)** for R36S changes.
The default `main` branch retains the original cross-platform
recompilation code and does not necessarily include R36S features.

## Good first contributions

- Reproduce a game bug in a real fight or attract-mode sequence with
  consistent steps, original version, frame/timing comparison and
  firmware details.
- Improve the SDL2/OpenGL ES2 renderer without breaking RGB555 colors,
  texture scaling, framebuffer memory or clipping.
- Optimize measured Cortex-A35 MIPS hot functions or the ADSP-2105 core
  **without** accelerating the game clock or dropping required frames.
- Test R36S-specific controller mappings, dArkOSRE/ArkOS variants,
  exit menu, save persistence and boot sequence.
- Improve README, translations of **project-authored** documentation,
  optional UI accessibility and automated tests using synthetic data.
- Add reproducible, licensing-compliant CI/checks that don't require
  shipping original ROMs, CHD, game-derived captures or generated code.

## Repository map

| File/directory | Purpose |
|---|---|
| `tools/kirecomp/kirecomp.py` | MIPS R4600 -> C++ static translator |
| `runtime/src/runtime.cpp` | MIPS register, memory and function dispatch |
| `runtime/src/kinst_board.cpp` | Arcade board, video RAM, interrupts/IDE |
| `runtime/src/dcs.cpp`, `third_party/adsp2100/` | DCS/ADSP audio emulation |
| `src/overrides/` | Hand-written high-impact sprite/blitter and fight functions |
| `src/ki_play.cpp` | SDL2 frontend, original guest timing, input and audio |
| `src/ki_r36s_intro.h` | Credit and legal notice screens (boot only) |
| `src/ki_r36s_blue_boot_wipe.h` | Bounded blue startup animation detector |
| `src/ki_r36s_audio_smooth.h` | Experimental pitch-preserving playback |
| `tools/r36s/` | Native ARM64 build script, ROM-free tests and PortMaster ZIP |
| `docs/R36S_PROGRESS.md` | Measured performance, test milestones, known limits |
| `docs/R36S_LEGAL.md` | Distribution caveats, attribution and asset policy |

## Prepare a source-only contribution (without any game files)

1. Fork this GitHub repository, clone your fork and check out
   `port/r36s-portmaster`.
2. Create a topic branch such as `fix/r36s-save-persistence` or
   `perf/r36s-dsp-poll`.
3. Use an ARM64 Linux runner with SDL2 headers. The public CI already
   compiles guest-independent tests on Ubuntu AArch64:

   ```sh
   cmake -S . -B build-ci -G Ninja -DCMAKE_BUILD_TYPE=Debug \
     -DKI_BUILD_FULL=OFF -DKI_R36S=ON
   cmake --build build-ci --parallel 2
   ctest --test-dir build-ci --output-on-failure
   ```

4. For the startup screen, build and run the entirely ROM-free
   renderer test on Linux with SDL2:

   ```sh
   c++ -std=c++20 -O2 -Isrc $(sdl2-config --cflags) \
     tests/r36s_intro_selftest.cpp $(sdl2-config --libs) -o /tmp/ki-intro-test
   SDL_VIDEODRIVER=dummy /tmp/ki-intro-test
   ```

5. Submit a pull request **against `port/r36s-portmaster`**.
   State the changed code path, exact reason, before/after evidence,
   tests, and possible regressions. Favor one coherent problem per PR.

### Optional local game test (private, lawful game assets only)

To build the complete player for *your own private testing*, you must
supply your own lawful copy of `kinst.zip` and `kinst.chd`. The
build preparation steps are described in
[`docs/R36S_PORT.md`](docs/R36S_PORT.md) and
[`tools/r36s/build.sh`](tools/r36s/build.sh), including the Python
virtual environment and `rabbitizer==1.16.2`. The full build
requires extracted game images and a matching original boot ROM.
The public repository, CI and pull requests **must never receive**
those inputs or game-derived generated outputs.

For a clean, optimized ARM64 build, on a compatible AArch64 Linux
environment with the required private inputs present:

```sh
KI_BUILD_TYPE=Release KI_R36S_MIPS_O3=ON \
  KI_R36S_RELEASE_CLEAN=ON bash tools/r36s/build.sh
python3 tools/r36s/package_port.py build-r36s/ki_play --release \
  --output /tmp/ki-r36s-private-test.zip
```

This produces an executable based on original game-derived code.
**Do not publicly attach, upload or distribute the resulting binary
without reviewing and obtaining any required rights/permissions.**
The standard PortMaster ZIP packaging check only verifies that
separate commercial ROM/CHD files were not included.

## Benchmark standards

- Original gameplay runs at approximately **58.98 Hz**. Do not claim
  a speedup achieved by raising guest emulated clock, skipping host
  input/audio emulation, discarding full game frames, changing
  graphics colors or taking unsafe floating-point shortcuts.
- Report a scene-matched 60-second R36S A/B test on the same firmware,
  power profile, CPU clock, fighters and arena, preferably two runs.
- The diagnostic/profiling build affects FPS and must not be compared
  directly to the stripped clean release.
- Prove memory and guest-CPU correctness with synthetic
  `ctest` tests or authorized local MAME comparisons; generated
  guest-code and copyrighted reference captures stay private.
- Keep improvements optional until they have passed hardware tests,
  particularly LTO, sound DSP compilation and experimental GPU paths.

## A useful issue/bug report

```text
Title: [R36S][Audio/Video/Gameplay/Boot] brief symptom
Git SHA or release candidate:
Console / firmware / version:
Stage and characters:
Steps to reproduce:
Expected:
Observed:
Crash? Linux exit code:
R36S log (if using diagnostic build):
FPS before/after (if performance issue):
Screen recording or photo (optional, no copyrighted asset dump):
```

**Do not post device-identifying secrets, game ROMs, CHDs, private
GitHub Actions binaries, generated recompiled C++ or proprietary
code in issues.** You may redact personal paths in logs. Review
[`docs/R36S_LEGAL.md`](docs/R36S_LEGAL.md) first.

## Review checklist

- Compilation succeeds on AArch64 and guest-free selftests pass.
- Game clock, sound pitch, controls, framebuffer memory, saves, and
  full-frame rendering remain intact.
- Change is small and easily reverted, with a measured benefit.
- Credits, original copyright notices and third-party licenses
  are preserved.
- No game-derived or otherwise proprietary file is added.
- Public documentation accurately distinguishes verified features
  from proposals and experimental optimizations.

For discussion, use [GitHub Issues](https://github.com/dehalbuq/ki-recomp/issues)
and open pull requests against the R36S branch. Friendly,
specific, reproducible feedback is welcome.
