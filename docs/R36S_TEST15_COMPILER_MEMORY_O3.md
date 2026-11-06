# Killer Instinct R36S — Test15: aggressive MIPS compiler, RAM direct loads & O3

## Actual DarkOSRE Test14 measurements (provided by user)

Runs: `ki-r36s-mips-turbo.log` and `ki-r36s(8).log`.
Device RK3326 Cortex-A35, CPU clock 1,512 MHz, dArkOSRE, GLES2.

| Last 10 PERF samples | MIPS idle OFF | MIPS idle ON |
|---|---:|---:|
| Guest FPS | **31.25** | **44.68** |
| Improvement (observed) | — | **+43.0%** |
| Renderer avg in those samples | 7.33 ms | 7.46 ms |
| At game exit: idle skip | 0 cycles | **2,814,671,748 cycles** |
| At game exit: idle iterations | 0 | **469,111,958 iterations** |
| Sound | DSP optimized ON | DSP optimized ON |
| Physical SD reads after loading | 0 MiB | 0 MiB |

The ending menu was opened during the final window, so the pure
gameplay improvement should be confirmed using identical fight scenes.
These data nevertheless prove that the exact MIPS IRQ polling skip
is used heavily in the game and has substantial practical value.

**Important:** original KI1 video refresh is ~58.98 Hz; claiming
`59.9 FPS` as an exact original-game clock would be inaccurate.
Target is full original ~58.98 guest frames/s without gameplay
acceleration, skipped rendering, audio disable or reduced precision.

## What changed in Test15 (real compiled code)

1. Promoted Test14 exact event-driven MIPS IRQ idle loops to **ON by
   default** in `Killer Instinct Arcade`; this is the biggest known
   verified optimization on RK3326. Disable via
   `KI_R36S_MIPS_IDLE_BASELINE=1` if gameplay regression is found.
2. Added `ki::load_recompiled<T>` and `store_recompiled<T>` in
   `runtime/include/ki/runtime.h`. The statically recompiled
   MIPS guest code uses these at every ordinary integer and FPU
   SRAM/DRAM access. When enabled, the helpers:
   - use exact LE memcpy for correctly aligned accesses in SRAM
     and DRAM through the two hardware KSEG aliases;
   - directly translate the only validated zero-page guest TLB mapping;
   - preserve normal read/write behavior and exceptions for alignment
     faults, invalid addresses, EPROM and I/O (fall back to the
     original MIPS guest-memory implementation);
   - honor the `KI_MEM_PROBE` debugging callback when present.
3. Added inlined guest `LWL/LWR/LDL/LDR` recompiled helpers with
   exact original mask/shift/partial 32-bit bus access behavior. This
   targets actual hotspots in the earlier native Test10 host profiler.
4. Both compiled code libraries are built with
   `KI_R36S_FAST_MEMORY`, which does *not* affect other platforms.
   On the ordinary player `KI_R36S_MEM_BASELINE=1` disables all
   memory specializations for reliable comparison.
5. Added **optional GCC -O3** generated MIPS translation build in
   CMake (`KI_R36S_MIPS_O3=ON`), separate from the normal -O2 binary.
   Only guest translation optimization level changes; no
   `-ffast-math`, cycle edits, dropped audio or frame skip.
   Aggressive code optimization is NOT guaranteed to be faster on
   the Cortex-A35 with a small instruction cache, so the Test15
   PortMaster ZIP includes two separately compiled AArch64 binaries
   to measure it fairly.

## Regression coverage and safety

The ROM-free `tests/mips_memory_fast_selftest.cpp` compares the
original and direct-memory backends on dozens of boundary addresses,
KSEG0/KSEG1 aliases, zero-page, I/O, invalid mapped addresses,
all access widths (8, 16, 32, 64-bit), alignment faults,
all offsets of `LWL/LWR/LDL/LDR`, RAM contents and I/O call counts.
The CI builds this on native ARM64 and invokes all ten CTest cases.

The standard interpreter and previous compiled backend remain
available for A/B comparisons, and audio, 4:3 nearest-neighbor,
complete video frames, speed of the game and Start+Select are kept.

## Installation / diagnostic comparison

Unzip Test15 in `roms/ports/` on dArkOSRE; **retain your own**
`killerinstinct/gamedata/kinst.zip`, `kinst.chd`, game saves
and preferences. Commercial game files are not included.

PortMaster entries:

- **Killer Instinct Arcade** — GCC -O2, MIPS Turbo ON, RAM fast ON.
  Log: `killerinstinct/logs/ki-r36s.log`.
- **KI R36S - Memory Baseline** — same GCC -O2 and MIPS Turbo, but
  memory fast path OFF. Log `ki-r36s-memory-baseline.log`.
- **KI R36S - Compiler O3** — same MIPS Turbo and RAM fast, but generated
  MIPS C++ compiled at GCC -O3 into `ki_play_o3.aarch64`.
  Log `ki-r36s-compiler-o3.log`.
- **KI R36S - MIPS Baseline** — disables both MIPS Turbo and memory
  fast (retains audio and other proven DSP optimizations).
  Log `ki-r36s-mips-baseline.log`.

Use the same character/stage/scene and 45–60 seconds of gameplay
for each of the first three launchers. Upload their logs for a
true independent measurement of RAM specialization and O3 compiler
flags. Do not use the diagnostic Host Profile FPS to estimate
non-profiled performance.

If O3 performs worse, use the main O2 launcher; if the new memory
specialization causes bugs, use `Memory Baseline`. If MIPS Turbo
affects sound or timing, use `MIPS Baseline`.

## Next possible large steps

If Test15 is still below ~58.98 fps, re-run native HOST_PROFILE with
MIPS Turbo enabled and inspect remaining guest hotspot functions,
especially sprite and panorama blitters. The previous Test10 hotspots
were sampled **before** the 43% MIPS Turbo gain, so their ranking may
have changed. Consider verified native NEON 2D blit overrides or
strictly guarded MIPS bulk-copy translation only where cycle
accounting, source/destination overlap, mask handling and memory
side effects can match the original.
Measure the SDL/OpenGLES2 buffer upload (render ~7.3ms per frame)
separately; forcing software rendering or discarding frames is not
acceptable. A raw GLES2 single-pass presentation path may merit its
own isolated A/B trial after profiling.

This file documents implementation and test method, **not** a claim
that the Test15 memory fastpath or O3 reaches full hardware speed.
