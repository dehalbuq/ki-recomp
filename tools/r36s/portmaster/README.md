# Killer Instinct (Arcade) — experimental PortMaster port

This is an **untested R36S prototype**, not an approved PortMaster release.

Thank you to [kilnan](https://github.com/kilnan/ki-recomp) and the original project contributors for the C++ static recompilation. Thanks to the authors of SDL2, MAME-derived ADSP support, libchdr and rabbitizer. R36S adaptation: [dehalbuq](https://github.com/dehalbuq).

## Game files

The game is not included. Supply your own legally obtained arcade `kinst.zip` (BIOS L1.5D, with the sound ROMs) and `kinst.chd` under `killerinstinct/gamedata/`. The script will not download ROMs.

## Controls

D-pad = movement; X/Y/R1 = high attacks; A/B/R2 = low attacks; Start = start; Select or L1 = coin. Hold Start+Select for about 0.8 seconds to open the pause/exit menu. Choose RESUME or EXIT with Up/Down and A; B returns to gameplay. A USB keyboard can also open the menu with F10 and exit immediately with Esc. Raw R36S Select=12 and Start=13 are read even without a GameController mapping. Actual button mapping depends on firmware's SDL2 database.

## Video and diagnostics

The guest runs at the original ~58.98 Hz, 320x240 rendered at 640x480 in 4:3 nearest scaling. SDL2 uses accelerated rendering first, software fallback only for diagnostics. Log: `killerinstinct/logs/ki-r36s.log`. The log reports the actual guest VBLANK/frame rate every five seconds; use this to measure real FPS and diagnose the bottleneck.

**Not ready for PortMaster submission:** test the ARM64 binary on ArkOS/DarkOSre, validate glibc and dependencies, add a real gameplay screenshot and optional cover art. Never bundle the game's data in public packages.
