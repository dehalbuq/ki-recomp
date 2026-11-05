# Killer Instinct R36S — Performance test 04

This is a **native ARM64 recompiled runtime**. GPU/SDL uses the `opengles2`
accelerated renderer on DarkOSre. In Test03 the SDL renderer reported
`flags=0xe`, which includes VSync, so Test04 asks SDL to turn VSync off.

## Changes in Test04

- No change to the original ~58.98-Hz guest game clock.
- Normal game still renders **every guest frame** (no frame skipping).
- Cortex-A35 tuning (`-mcpu=cortex-a35`) and safe `-O2` on Release.
- Framebuffer RGB888 conversion now reuses the same storage every frame.
- SDL2 renderer VSync disabled via SDL hint and `SDL_RenderSetVSync(0)`.
- New periodic profiler log with GPU/render/CPU/SD/memory/clock measurements.
- Normal Start+Select pause/exit menu unchanged.

## Installation

Extract the PortMaster ZIP in `roms/ports/`, replacing
`Killer Instinct Arcade.sh` and `killerinstinct/ki_play.aarch64`.
Keep your original `kinst.zip` and `kinst.chd` in
`roms/ports/killerinstinct/gamedata/`. These are **not** in the ZIP.

## Three tests

Use the same attract/demo scene or battle for each test. Let it run about
30 seconds, then exit with Start+Select. Save all three logs:

1. **Killer Instinct Arcade** — normal optimized game.
   Log: `killerinstinct/logs/ki-r36s.log`.
2. **KI R36S - Video Benchmark** — draws only 1 out of 4 guest frames. This
   deliberately looks choppy and is **diagnostic only**. The guest simulation
   still runs every frame. Log: `ki-r36s-bench-video.log`.
3. **KI R36S - DSP Benchmark** — skips the emulated ADSP2105 audio processor.
   The sound is intentionally absent. The game **might hang or behave
   differently**, so this test cannot be used as a playable patch. Log:
   `ki-r36s-bench-no-dcs.log`.

All logs are under `roms/ports/killerinstinct/logs/`. These tests do **not**
delete or change game data. Prefer to test with a copy/backed up save.

## How to interpret a [r36s] PERF line

- `fps`: guest VBLANK/frame rate, target ~58.98 Hz.
- `cpu`: process CPU time / elapsed wall time, as a percentage of one core
  (can exceed 100% if threads use multiple cores).
- `render`: average milliseconds spent rendering a shown frame.
- `rgb`: subset of render used to convert framebuffer BGR555 to RGB888.
- `swap`: subset of render blocked in `SDL_RenderPresent` (includes VSync
  or driver flush). High values can signal GPU/swap bottleneck.
- `drawn`: frames actually sent to the renderer during the 5-second sample.
- `sd_read`: physical read bytes accounted for the process during the sample.
  The CHD decompresses to **131,076,608 bytes** during startup, so expected
  gameplay disk I/O is near zero, except save files and other incidental reads.
- `rss`: process resident memory.
- `cpu_khz`: current CPU0 clock, if exposed by the firmware.

If skipping video greatly increases **guest** FPS, the renderer is costly.
If skipping DCS increases FPS and the game still boots, the DSP interpreter
may be the main bottleneck. If neither improves it, investigate the main
MIPS recompilation, memory accesses, and event timing. VSync off may help
only where the driver supports switching it off.

**Do not use these diagnostic launchers as a normal release.** Do not
raise the guest clock, overclock the board or patch ROM data merely to
inflate the displayed FPS. Establish baseline measurements first.
