# R36S ARM64 port — technical progress and status

The current **source branch is `port/r36s-portmaster`**. The default
`main` branch preserves the original portable KI recompilation work.
This is an independent experimental R36S port, not an official
PortMaster submission or a finished full-campaign certification.

## What actually runs

- The original **Killer Instinct (1994 arcade)** MIPS R4600 game program
  is translated to C++ at build time using the `ki-recomp` toolchain.
  It executes as a native **AArch64** Linux executable on the RK3326's
  **Cortex-A35**, rather than executing the arcade game through MAME or
  an unrelated console emulator.
- The original arcade's **ADSP-2105/DCS sound board still needs a CPU
  emulator**; its optimized interpreter remains active and produces
  original DSP output, played via SDL2. Native CPU recompilation does
  not mean that every arcade component was rewritten as native code.
- The arcade draws pre-rendered 2D characters/scenery into its
  CPU-visible RGB555 framebuffer. The **Mali-450 through OpenGL ES 2**
  accelerates display of the final image, *not* the game's entire
  framebuffer blitter. Calling the stages hardware-rendered 3D would
  be misleading.
- Full-screen 4:3, **320×240 -> 640×480 2× pixel-perfect scaling**,
  nearest-neighbor filtering, controls and PortMaster entry integration.

## Development milestones

| Test/phase | Work completed | What the evidence supports |
|---|---|---|
| First prototype | Native MIPS recompilation, arcade boot, inputs and gameplay | Game reached playable fight scenes on RK3326; not perfect FPS |
| Tests 9–15 | Cortex-A35 code tuning; idle IRQ skipping; MIPS RAM/COP0 fast paths; separate -O3 build | R36S improvement reported, with regression tests for guest behavior |
| Tests 16–17 | Profile sprite/panorama + SDL GLES2; replace RGB24 output with RGBA8888; NEON | Typical sampled GPU upload fell from approximately 5ms to 1.4ms; scene-dependent |
| Test 18 | WSOLA pitch-preserving SDL audio smoothing | User reported smoother gameplay; original sound queue often ran dry in earlier logs. Gameplay still below target in demanding scenes |
| Test 19 | NEON SIMD color conversion; alternate direct GLES2 texture lock and no-clear test | Measured `SDL_UpdateTexture` upload 1.42ms vs direct lock 1.74ms. No-clear reported the highest FPS average but was not a controlled scene-matched comparison |
| Test 20 | MIPS indirect `jalr/jr` target cache, exact dispatch tests, optional IPO/LTO experiment, launcher cleanup | Cache profiling showed >99% hits in busy scenes; LTO compiled separately but remains experimental. No FPS gain attributed solely to these tests |
| Test 21 | Recognize and accelerate only the blue curtain animation during cold boot | Hardware log confirmed `BOOT_BLUE FAST` at guest frame 51 and return to normal at frame 397 |
| Clean v0.1.0 candidate | Release/O3 build, no per-frame profiling, no diagnostic launcher, stripped symbols | ARM64 Release build and ROM-free package verification passed; still needs final hardware regression |
| Next release candidate | Two startup screens: project/third-party credits and honest legal notice | Source tests validate screen layout; actual firmware test should check readability and skip controls |

## Dual AArch64 build — automated ArkOS4Clone selection

A private GitHub Actions run
[completed successfully](https://github.com/dehalbuq/ki-recomp-build-private/actions/runs/42238929556)
with two separately compiled executables in one PortMaster package.
The launcher chooses by native `getconf GNU_LIBC_VERSION`:

- **dArkOSRE:** `ki_play.aarch64` when GLIBC >=2.38,
  with a measured executable requirement of GLIBC **2.38**.
- **ArkOS4Clone:** `ki_play.glibc230.aarch64` when GLIBC
  2.28–2.37; its measured maximum required GLIBC symbol is **2.17**.
  The legacy build uses manylinux_2_28 ARM64 GCC14 and bundles a
  matching SDL2 with KMSDRM/OpenGLES2, libstdc++ and libgcc_s
  under `libs.glibc230`. Those libraries require at most GLIBC **2.27**.
- Versions below 2.28 or unknown GLIBC fail safely without invoking an
  incompatible binary. No system GLIBC or GPU drivers are bundled.

Both ELF files include the source credits/legal splash, clean Release
optimizations, original guest timing and WSOLA sound. The ZIP is ROM-free.
A synthetic `bash tests/r36s_dual_abi_launcher_selftest.sh` verifies
the selector against firmware GLIBC values 2.28–2.39 and missing-file
failure paths.

**Limit:** ABI symbols, packaging and CI builds were validated. This
does **not** prove game launch, SDL2/KMSDRM rendering, sound, controls,
or save behavior on actual ArkOS4Clone hardware. A user/device test
is required before claiming compatible gameplay.

## Observed R36S performance

Reference: the user's full **Test21** R36S log on **dArkOSRE**
(ARM64 RK3326 @ ~1.512 GHz). Post-startup 5-second windows showed
roughly **53.8 FPS on average**, with **48.3–58.3 FPS** across
those windows. This is not a benchmark of the final stripped build;
contention, stage, firmware, sound load, and CPU clock can change FPS.
The original arcade target is approximately **58.98 Hz**.

The cold boot blue-wipe detector activates in early guest frames only.
Gameplay restores original guest time progression and does not
fake a speedup by increasing the emulated clock. Startup game-data
loading itself was ~8 seconds in the observed log and is not yet
optimized by the blue-wipe change.

### Main outstanding technical limits

- Guest MIPS hotspots are still a significant CPU cost, including
  framebuffer blitters that must preserve original SRAM/guest timing.
  See [Test20 recompilation diagnosis](R36S_TEST20_RECOMPILER.md).
- DCS/ADSP sound emulation can consume a substantial fraction of a
  core; it cannot be removed without breaking game audio.
- DarkOSRE's SDL GLES2 renderer reports VSync active despite the port
  requesting it off; driver-level behavior may affect frame pacing.
- WSOLA is an experimental audio playback policy; rare underflow
  bursts may still occur in particularly CPU-heavy scenes.
- The whole game/fighter roster, saves, all levels, alternate firmware
  variants, and different controller models have **not** been
  comprehensively certified on every R36S.
- GitHub Ubuntu ARM64 artifacts can require a newer GLIBC than some
  handheld firmwares. **Do not swap the firmware's system GLIBC or
  Mali/EGL libraries** to try to run a package.

## Build/development location

- [R36S branch](https://github.com/dehalbuq/ki-recomp/tree/port/r36s-portmaster)
- [R36S build and PortMaster guide](R36S_PORT.md)
- [Blue-wipe timing design](R36S_TEST21_BLUE_BOOT.md)
- [R36S legal and attribution notes](R36S_LEGAL.md)
- [Contributing](../CONTRIBUTING.md)

Generated MIPS-to-C++ translation, original firmware, ROMs, disk
images, extracted textures/audio and MAME capture traces must stay
out of public pull requests and source control. Private users must
supply their own lawful game assets to compile and play.

## Next directions

1. Fully test the credits/legal screen and clean build on dArkOSRE
   without turning on performance instrumentation.
2. Measure identical fights before and after each performance change,
   using a separate diagnostic build.
3. Optimize measured MIPS framebuffer paths without changing original
   rendering, guest memory or logic; validate with deterministic tests.
4. Improve DSP run-loop speed and SDL/GLES2 driver integration where
   the measured result justifies the risk.
5. Expand documented hardware/firmware compatibility through
   reproducible contributions, not unverified compatibility claims.
