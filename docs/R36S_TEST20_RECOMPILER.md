# R36S Test20 — Optimize actual MIPS recompilation bottlenecks

## What the R36S Test19 logs show

The user's four DarkOSRE / RK3326 runs use 1.512GHz Cortex-A35,
640x480 fullscreen 4:3 GLES2 and WSOLA audio. Measured 5-second windows:

| Test19 launcher | FPS average | SDL/GLES2 upload mean |
|--|--:|--:|
| NEON Update | 52.38 | 1.42 ms/frame |
| NEON Locked | 50.78 | 1.74 ms/frame |
| Scalar Locked | 52.17 | 1.96 ms/frame |
| NEON NoClear | 54.03 | 1.75 ms/frame |

These runs have different durations and scene mix. **NoClear is not
proved faster than Update** and might leave artifacts with a changed
viewport. The lowest upload time belongs to NEON Update. Busy 95–96%
CPU windows still achieve only 50–54 FPS at about 3.8–4.0 ms/frame
in SDL rendering, which implies other host work is significant. But
guest MIPS and emulated audio DSP are both included in that work;
neither can be called the dominant component without separate timing.

Test20 therefore defaults to the measured 32-bit NEON conversion
plus SDL_UpdateTexture GLES2, enabling the pitch-preserving WSOLA
output. The locked-texture and no-clear candidates are dropped from
the default launch path. The GLES2 driver still reports VSync active
despite requesting OFF; forcing driver changes without firmware
validation is not safe.

## One new reversible runtime optimization

Every indirect MIPS `jalr` or unresolved `jr` looks up its 32-bit
guest target using the sorted static function table. The old
`lookup_function()` always performs a binary search. Test20 uses a
256-entry direct-mapped cache for *successful* address-to-function
lookups. Cache hits bypass the binary search. Cache collisions fall
back to the original search; no guessed function is ever called.

The boot ROM table may change on reset. `set_boot_function_table()`
invalidates all cached entries. The cache is thread-local to avoid
races with other future host threads and does not modify guest MIPS
registers, SRAM, DRAM, interrupts or timing. Only R36S builds have
the cache; all other builds retain the original implementation.

The `KI_R36S_DISPATCH_BASELINE=1` environment setting disables it,
without requiring another PortMaster launcher. The CPU diagnostic
mode enables `KI_R36S_DISPATCH_PROFILE=1`, logging:
`MIPS_DISPATCH lookups=...`, `cache_hits=...`, `misses=...`,
`hit_rate=...` every ~5 seconds.

Two independent ROM-free ARM64 CTest modes use synthetic, sorted
function tables to verify hit equivalence, misses, boot ROM aliases,
table replacement/reset and actual `call_indirect()` behavior:
`dispatch_cache_selftest` and `dispatch_cache_baseline_selftest`.
The cache can be a **net regression** if indirect targets are rare or
frequently collide, so no FPS benefit is claimed before on-device
measurement.

## PortMaster scripts were cleaned up

The old 25 launchers were reduced to TWO:

1. `Killer Instinct Arcade.sh` — normal optimized release.
2. `KI R36S - Host Profile.sh` — optional CPU/DSP/GPU/dispatch
   profiling, **not for measuring user-visible FPS**.

Deleted 23 earlier A/B experiments and intentionally broken benchmarks
that muted audio or drew fewer frames. The underlying diagnostic
features remain in source under optional environment flags.

## Controlled next benchmark

1. Install the private Test20 ZIP over the PortMaster directory and
   preserve your original game data and save files.
2. Play a real fight using **Killer Instinct Arcade** for 60 seconds,
   then save `killerinstinct/logs/ki-r36s.log`.
3. Run **KI R36S - Host Profile** for a comparable scene,
   then save `killerinstinct/logs/ki-r36s-test20-cpu-profile.log`.
   This will identify how much CPU goes into generated MIPS functions,
   the ADSP audio core, GLES2, and whether indirect target caching is
   effective. Its profiling overhead invalidates direct FPS comparisons.
4. Compare the normal mode against Test19 `NEON Update` (similar
   stage/opponents; at least 60 seconds). To benchmark the cache itself
   on the same binary and same scene, temporarily export
   `KI_R36S_DISPATCH_BASELINE=1` before launching and compare with
   the environment unset. Do not alter original ~58.98 guest timing.
5. If the cache is ineffective or harmful, keep the old resolver.
   Use `HOST_TOP`, `PHASE dsp_per_frame`, and `GFX_SDL_ES2`
   results to choose the next *measured* MIPS hotspot or DSP task.

## Next high-impact options after profiling

- Optimize generated MIPS memory access dispatch and hot sprite /
  panorama routines only when a profiler identifies that code.
- Explore cross-TU link-time inlining (LTO) **in a separate, reversible
  build**, with exact game regression checks and hardware A/B results.
- Accelerate GPU blitters only if equivalence tests guarantee CPU-visible
  framebuffer SRAM, clipping, palette, order and game cycle behavior.
- Preserve full-frame 4:3 pixel scaling, original DSP chip timing and
  ~58.98 Hz guest cadence at all times. Do not enable fast-math, skip
  audio or video, increase emulated clock or silently change guest
  behavior just to show higher FPS.

This is not a claim of 58.98 FPS nor proof that the recompiler alone
consumes the majority of CPU. Only real stage-matched R36S profiling
can establish that.
