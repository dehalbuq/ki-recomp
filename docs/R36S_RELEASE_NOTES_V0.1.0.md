# Killer Instinct Arcade R36S — v0.1.0 release candidate notes

**Version:** v0.1.0 (first ARM64 R36S candidate with in-game legal/credits pages).
**Status:** independent experimental port; hardware regression required
before marking this particular clean/splash build stable. No public
redistribution of the statically recompiled binary is authorized by
this document.

## Summary

- Native Linux **AArch64/Cortex-A35** KI arcade frontend, driven by
  MIPS R4600 static recompilation (upstream `kilnan/ki-recomp`).
- Original game clock ~58.98Hz, full-frame SDL2 OpenGL ES2 output at
  320×240 / 640×480 perfect pixel 4:3, GLES2 RGBA upload and ARM NEON.
- ADSP-2105 original sound system emulation, experimental WSOLA
  pitch-preserving smoothing, optional safe boot animation acceleration.
- Start+Select hold opens the pause/exit menu; PortMaster package
  migration removes 23 obsolete KI benchmark launchers by exact name.
- **New:** standalone credits page followed by a legal notice,
  drawn with an embedded 5×7 bitmap alphabet before the game data
  is loaded. A/Start may advance the pages; no SDL_ttf or images
  are required and no splash code executes during gameplay.
- **New:** README landing page on both `main` and the R36S branch,
  English technical progress history, Portuguese help, extensive
  legal/credits document, and contribution instructions.
- Clean Release build uses MIPS O3 and removes frequent CPU/FPS/
  graphics/DSP/PCM stats and native PC sampling. Executable symbols
  are stripped; only the main game launcher ships.
- No `kinst.zip`, `kinst.chd`, original game music, graphics,
  game firmware or other separate copyrighted original assets
  are included in PortMaster packages.

## R36S performance reference

Earlier real Test21 dArkOSRE log: ~53.8 FPS mean in post-boot
5-second windows, 48.3–58.3 FPS range, and early blue-wipe
acceleration correctly turning off at frame 397. This is historical
measurement from a diagnostic-capable Test21 build; it does not
establish the new clean-splash candidate's actual performance.
The observed launch spent ~8.3 seconds loading the CHD/disk before
emulation began. Other firmware versions were not yet certified.

## Acknowledgments

- Original commercial game: Rare / Midway and rightsholders.
- MIPS recompilation foundation: kilnan/ki-recomp.
- R36S ARM64 port, optimizations and PortMaster integration: dehalbuq.
- MAME-derived ADSP-21xx DSP interpreter: Aaron Giles and contributors.
- SDL2, libchdr, miniz, rabbitizer and original PortMaster community.

## Installation / legal status

Users must provide their **own lawfully obtained** `kinst.zip`
(L1.5D boot/sound ROM set) and `kinst.chd` privately in
`/roms/ports/killerinstinct/gamedata/`. Copying a private build over
an earlier release should not delete saves. See the package README
for exact file locations and controllers.

**Important:** The AArch64 executable can embed game-derived
recompiled code. Excluding the original ROM and CHD from a package
does NOT by itself authorize redistribution of the executable.
Review copyright/licensing and obtain any necessary authorization
before publishing the binary to a public GitHub Release or the
official PortMaster catalog.

For now, distribute project source, docs, scripts, independent
ROM-free tests and reproducible *private* build instructions.
See [R36S legal details](R36S_LEGAL.md).

## Pre-release QA checklist

- Launch credits and legal page on real dArkOSRE 640×480 device;
  verify texts are readable, A/Start advance, and the guest boots.
- Compare fight timing, WSOLA audio, controller input and
  Start+Select exit to the known-good Test21 build.
- Test saves and reopen; make sure legacy launchers disappear
  without touching user game files or settings.
- Check ABI requirements with the target firmware. Do not attempt
  to overwrite system GLIBC/EGL/SDL2 libraries.
- Obtain legal authorization if intending to redistribute the
  compiled, game-derived binary.
