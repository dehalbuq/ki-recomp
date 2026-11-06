# KI R36S — Test09: measure DSP host time vs rest of game

## What Test08 confirmed

The user supplied two DarkOSRE R36S logs:
- `ki-r36s-dsp-loop-baseline.log`: new DSP microblock disabled, earlier fast DCS memory enabled.
- `ki-r36s(4).log`: new DSP microblock enabled.

For the **last ten 5-second PERF samples**, the baseline averaged **24.33 guest
FPS** and the optimized mode **33.86 guest FPS** (**+39.17%**). This is a
meaningful observed improvement, though different attract/fight scenes can
cause residual run-to-run variation.

The optimized run skipped **610,039,455** redundant guest ADSP
instructions. The baseline log skipped zero. Both runs used GLES2, an
RK3326 CPU at 1,512,000 kHz, ~99% of one CPU core, no physical SD reads
during gameplay, and ~7.3 ms video render per frame (including ~2.04 ms
SDL_RenderPresent). The normal game still renders every frame.

The DSP interpreter is much cheaper than before, but the machine has
not reached the original ~58.98 guest FPS. We must determine whether the
remaining performance problem is DSP processing, native MIPS recompiled
CPU work, or GL/SDL presentation before another risky optimization.

## Test09 — opt-in cost attribution

Only the dedicated benchmark launchers set `KI_R36S_TIME_DSP=1`.
The normal game has **no extra clock calls**.

Inside `Dcs::run_until`, the benchmark records:
- host wall-clock nanoseconds in the DCS executor,
- the number of calls to `run_until`,
- guest ADSP cycles completed.

Every 5 seconds `ki_play.cpp` emits:

```
[r36s] PHASE dsp_wall=...ms dsp_per_frame=...ms dsp_wall_pct=...% run_calls=... guest_cycles=...
```

`dsp_wall` measures *only* the DCS execution path, including internal
timer/DAC transfers; it excludes SDL render and native MIPS runtime code.
It is a **profiling estimate** (clock calls and scheduling introduce
minor overhead). `dsp_wall_pct` is percent of elapsed wall time, not
share of total multi-core CPU utilization.

Menus in the Test09 ZIP:

1. **Killer Instinct Arcade** — the normal optimized build, no profiler.
2. **KI R36S - DSP Timing** — optimized DSP with DCS time measurements.
3. **KI R36S - DSP Timing Baseline** — same profiler but the two
   polling-loop microblocks disabled, keeping DCS memory fast path.

Run each in the same attract/fight scene for 45–60 seconds. Do not
compare the first 10–15 seconds alone (startup transients). Exit with
Start+Select, and upload:
- `killerinstinct/logs/ki-r36s-dsp-timing.log`
- `killerinstinct/logs/ki-r36s-dsp-timing-baseline.log`

The regular `ki-r36s.log` is already available from Test08, but may be
retaken in the same scene for a zero-profiler control.

## How we will act on the result

- If optimized DCS time is still very high, prioritize firmware-gated
  KI-only HLE sample-decode and mixing kernels. Compare ADSP command
  timing and PCM checksums; always retain a cycle-exact interpreter.
- If DCS time is now relatively small, use the existing native MIPS
  override infrastructure to target measured screen/background and
  bulk-memory functions (ideas identified in the user's Ultra64Emu
  archive), preserving original guest-visible side effects.
- If renderer time is significant, examine EGL buffer swapping and
  16-bit GPU texture upload, preserving GLES2 and 4:3 pixel scaling.

Do **not** change clock frequency, disable audio, skip video frames,
or move DSP execution to a parallel thread without deterministic
synchronization. Those approaches can inflate FPS or break game timing.
