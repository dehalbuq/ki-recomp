# Killer Instinct (Arcade) — R36S v0.1.0 (first release candidate)

Native ARM64 R36S port, based on the open-source work by
[kilnan](https://github.com/kilnan/ki-recomp) and adapted for R36S
by [dehalbuq](https://github.com/dehalbuq). Source:
https://github.com/dehalbuq/ki-recomp/tree/port/r36s-portmaster

**Requirements:** AArch64 handheld running a compatible PortMaster
firmware (tested on dArkOSRE/RK3326; other firmwares not yet confirmed).
Use 640x480 4:3, native GLES2, nearest-neighbor exact 2x pixels.

**Install:** Extract this ZIP directly into the SD card's
`roms/ports` folder. Keep your own lawfully obtained `kinst.zip` with
L1.5D BIOS and `kinst.chd` under
`roms/ports/killerinstinct/gamedata/`. They are **not included**.
Launch **Killer Instinct Arcade** from the Ports menu. The launcher
migrates/removes the known older KI benchmark scripts from past tests
and leaves the game files, saves and other ports untouched. If menu
entries remain until refreshed, restart the launcher frontend.

**Controls:** D-pad or analog stick to move; X/Y/R1 high attacks,
A/B/R2 low attacks; Start begins game; Select/L1 inserts a coin.
Hold Start+Select for 0.8s to open the pause/exit menu. Use up/down
and A to choose RESUME or EXIT, B cancels. Mapping varies by firmware.

**Optimizations retained:** AArch64 Cortex-A35 -O3 static MIPS
recompilation, physical RAM fast path and guarded IRQ idle skipping,
MIPS indirect jump target cache, optimized ADSP2105 audio interpreter,
WSOLA pitch-preserving audio, OpenGL ES2 native RGBA upload with ARM
NEON 16-to-32-bit conversion, full-frame 4:3 pixel-perfect scaling,
and bounded auto acceleration of the blue boot wipe.

**Removed in the clean build:** All optional CPU PC-sampling, runtime
DSP timing, periodic FPS/GLES2 profiling, per-sample PCM tracing,
graphics instrumentation, experimental alternative launchers, and
host telemetry overhead. This build is NOT the experimental O3+LTO
candidate. It retains only operational errors/warnings and the
PortMaster cleanup required for user safety.

**Real hardware performance (Test21 reference):** ~53.8 FPS average
after the first boot periods, 48.3–58.3 FPS across measured 5-second
windows, according to the debug/profiling-enabled *prior* Test21 logs.
This lean release has not yet been benchmarked and may be different.
Original arcade target 58.98 FPS; no fake rate cap increase or guest
clock changes.

**Legal/credits:** This independent fan-made technology project is
not affiliated with or endorsed by Rare, Midway, Nintendo or the
current rightsholders. The packaging does not contain ROMs or CHD,
but **the statically recompiled executable may contain game-derived
code obtained from copyrighted original ROMs**. Before publishing
the binary publicly, review distribution rights and obtain permission
where required. A source-only release and private compilation using
the user's own game files are safer interim distribution options.

Licenses for the open-source engine and DSP sources are included in
`killerinstinct/LICENSE.*.txt`. The source tree includes additional
third-party dependency notices.

## Recovery

If the executable fails on a different firmware, restore the previously
tested Test21 build. The executable is at
`killerinstinct/ki_play.aarch64`; saves and original files stay in
their existing locations. No new save format is introduced in v0.1.0.
To disable only the optional accelerated blue boot transition,
set `KI_R36S_BOOT_BLUE_FAST=0` before launching.
