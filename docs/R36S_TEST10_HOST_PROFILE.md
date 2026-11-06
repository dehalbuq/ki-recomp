# KI R36S — Test 10: native ARM64 hotspots after the DSP speedup

## Observations from Test09 logs (same DarkOSRE/RK3326)

The user provided `ki-r36s-dsp-timing.log` (fast KI ADSP microblocks)
and `ki-r36s-dsp-timing-baseline.log` (microblocks disabled).

Averaging the final **ten** 5-second samples in both logs:

| Measured quantity | Original DSP loops | Fast DSP loops |
|---|---:|---:|
| Guest FPS | 26.71 | 34.46 |
| Time per guest frame (approx.) | 37.44 ms | 29.02 ms |
| Host time inside `Dcs::run_until` | **13.79 ms/frame** | **5.48 ms/frame** |
| SDL renderer | 7.27 ms/frame | 7.29 ms/frame |
| Remaining host time (residual) | **16.39 ms/frame** | **16.31 ms/frame** |
| DSP percent of elapsed wall | 36.83% | 18.82% |
| CPU frequency | 1,512 MHz | 1,512 MHz |
| Physical SD read during play | 0 MiB | 0 MiB |

The DSP host time improved by ~60.3% and the guest frame rate
increased by ~29% in this comparison. The residual *did not* improve,
so additional DSP work alone cannot reach the original ~58.98 FPS.
A theoretical DSP-free A/B bound, holding everything else constant,
is ~42.4 FPS from the optimized measurement.

The residual includes statically recompiled MIPS R4600 execution,
input/event handling, SDL queueing, runtime memory helpers and system
overhead. It is **not** proof that all 16.3 ms are MIPS instructions.

## Test10 native ARM64 sampling — how it works

The user's objective is to identify the hot native functions before
investing in HLE graphics or guest-memory specialization.

`KI R36S - Host Profile.sh` sets `KI_R36S_HOST_PROFILE=1`.
Only then `ki_play.cpp` starts a Linux `ITIMER_PROF` interval timer
at 200 Hz of **process CPU time** (5 ms per signal).
The POSIX `SIGPROF` handler reads the current Linux AArch64 host PC
from `ucontext_t::uc_mcontext.pc` and writes that address into a
preallocated, lock-free, bounded 8192-address array.

Every 5 seconds the app pauses the timer, groups samples by host C++
function name using `dladdr` (symbols exported via `-rdynamic`),
then logs the top 15 functions and resumes sampling.

```
[r36s] HOST_PROFILE frames=... cpu_pc_samples=... retained=... unique_symbols=...
[r36s] HOST_TOP rank=1 pct=...% hits=... sym=...
```

This can report generated functions (`func_880...`), C++ memory
helpers, ADSP interpreter, and SDL renderer/driver functions separately.
It samples the **host CPU**, not guest DSP PCs, and does not alter game
clocking. The amount of sampling/logging overhead is intentionally
accepted only in the diagnostic launcher. The normal
`Killer Instinct Arcade` entry does not install any signal handler or
timer, and the earlier DSP/microblock acceleration remains enabled.

The profiler does not capture original ROM/CHD data or game state.
It reports function names/CPU fractions only. Names from dynamically
loaded drivers may be missing or aggregated as `<unknown>` because
`dladdr` cannot discover private symbols; those values should not be
mistaken for MIPS execution.

## R36S test procedure

1. Extract Test10 ZIP to `roms/ports`, keeping `kinst.zip`,
   `kinst.chd`, and save files.
2. First run `Killer Instinct Arcade` normally (no profiler) for
   the same attract-mode/fight scene, ~45 seconds.
3. Run **KI R36S - Host Profile** for 45–60 seconds in the same
   scene. This will be somewhat slower because of sampling.
4. Exit with Start+Select and share:
   `killerinstinct/logs/ki-r36s-host-profile.log`.

Use the native host profiles to identify hotspots rather than guessing:
- If generated `func_880...` routines dominate, inspect their
  MIPS assembly/translated code and candidate Ultra64Emu-inspired
  background and memory-copy HLE overrides.
- If `adsp::Adsp2105` still dominates, continue precise DSP HLE.
- If SDL/OpenGL/driver samples dominate, evaluate GPU upload/swap path
  while preserving 4:3 nearest-neighbor and all frames.
- If `<unknown>` dominates, improve driver symbolization next.

**Do not use diagnostic FPS as a release regression metric.**
All audio fidelity checks and normal gameplay must continue on the
nonprofiling build.
