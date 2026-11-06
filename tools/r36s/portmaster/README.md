# Killer Instinct Arcade — R36S native ARM64 / PortMaster prototype

A native AArch64 statically recompiled KI arcade engine, adapted by
[dehalbuq](https://github.com/dehalbuq) from
[kilnan/ki-recomp](https://github.com/kilnan/ki-recomp).
Additional components include the independently licensed SDL2,
libchdr and MAME-derived audio core. This is **not** an official
PortMaster submission and has no game assets included.

## Minimal launch menu (Test20)

Exactly two scripts are shipped; all earlier Test3–Test19 benchmark
launchers have been removed from the new ZIP:

- `Killer Instinct Arcade.sh` — normal optimized build: Cortex-A35
  tuned generated MIPS (-O3), safe indirect-function cache,
  original arcade 58.98 Hz timing, OpenGLES2 RGBA8888 / ARM NEON
  conversion with `SDL_UpdateTexture`, 4:3 exact 2x pixel scaling,
  full-frame rendering, experimental pitch-preserving WSOLA audio.
  The slower Test19 direct SDL texture lock is **not** used.
- `KI R36S - Host Profile.sh` — CPU performance diagnosis only:
  native function PC samples, DSP timing, GPU upload and MIPS
  indirect-lookup cache statistics. It can reduce FPS; do not
  benchmark this launcher against normal mode.

The runtime cache can be disabled for a controlled experiment by
setting `KI_R36S_DISPATCH_BASELINE=1` in the environment before
launching. No special PortMaster menu launcher is required.

## Legal game data (not bundled)

Copy your own legally obtained `kinst.zip` (with KI1 L1.5D BIOS and
sound ROMs) and `kinst.chd` into
`/roms/ports/killerinstinct/gamedata/`.
This project never downloads original game ROMs or CHD automatically.

## Controls

D-pad = movement; X/Y/R1 = high attacks; A/B/R2 = low attacks.
Start = start; Select/L1 = coin. Hold **Start + Select** for the
pause/exit menu (~0.8 seconds). Press A to confirm RESUME/EXIT,
or B to resume. Actual mapping depends on the firmware's SDL2 database.

## Performance logs

Normal log: `killerinstinct/logs/ki-r36s.log`.
CPU diagnostic log: `killerinstinct/logs/ki-r36s-test20-cpu-profile.log`.

Test19 on dArkOSRE measured 52.38 FPS average with NEON Update
versus 50.78 with NEON Locked and 52.17 with Scalar Locked.
NEON NoClear registered 54.03 FPS, but had different scene durations
and its speedup remains unproven; removing SDL clear is not default.

See `docs/R36S_TEST20_RECOMPILER.md` for Test20 measurements,
correctness checks, and limitations. The GLIBC and SDL2 requirements
of a GitHub-hosted Ubuntu ARM64 build may differ from the user's
firmware; verify on the actual R36S.
