# Killer Instinct — R36S ARM64 / PortMaster (source template)

This source folder contains the PortMaster package template for the
independent [dehalbuq](https://github.com/dehalbuq) port based on
[kilnan/ki-recomp](https://github.com/kilnan/ki-recomp).

**Status: v0.1.0 release candidate**, verified in ROM-free CI and
on dArkOSRE in earlier Test21 builds. The latest startup credits/legal
screens and the stripped binary still require final R36S hardware
validation. This is **not** an official PortMaster catalog release.

## Minimal release package

The clean release ZIP is created by:

```sh
KI_BUILD_TYPE=Release KI_R36S_MIPS_O3=ON \
  KI_R36S_RELEASE_CLEAN=ON bash tools/r36s/build.sh
python3 tools/r36s/package_port.py build-r36s/ki_play --release \
  --output /tmp/ki-r36s-v0.1.0-private.zip
```

The release ZIP includes **only `Killer Instinct Arcade.sh`** as
launchable PortMaster entry, a stripped AArch64 executable, project
license files, a plain-text README and metadata. Legacy Test3–21
diagnostic launchers are omitted; previous exact launcher names
are safely cleaned up on a user's first launch.

A source/dev ZIP *without* `--release` additionally includes the
optional `KI R36S - Host Profile.sh` diagnostic launcher, but do not
confuse that with the public-facing release.

On boot, two small SDL2 pages show **project credits** and **legal
information** before the user-supplied original game data is read.
The page renderer is inactive during emulation.

## Hardware, controls, data

- RK3326 Cortex-A35, Linux AArch64 / PortMaster, dArkOSRE verified
  in previous builds; other firmwares and GLIBC ABI need testing.
- Original arcade 320×240 -> native 4:3 640×480, exact nearest scaling.
- OpenGL ES2 / ARM NEON RGBA upload, static -O3 MIPS recompiler,
  ADSP-2105 DSP emulator, WSOLA audio playback and bounded blue
  startup fast-forward.
- D-pad/left stick move; X/Y/R1 high attacks; A/B/R2 low attacks.
  Start begins, Select/L1 inserts coin; hold Start+Select for exit.
- User must place their own legally acquired `kinst.zip` and
  `kinst.chd` into `killerinstinct/gamedata/`.

**No original ROM or CHD is bundled.** However, the compiled static
recompiler executable may contain game-derived copyrighted material.
Do not assume permission to redistribute it; review the rights
and obtain any necessary authorization before publishing binaries.

## Documentation and contributions

- [Progress and hardware evidence](https://github.com/dehalbuq/ki-recomp/blob/port/r36s-portmaster/docs/R36S_PROGRESS.md)
- [Detailed credits and legal notice](https://github.com/dehalbuq/ki-recomp/blob/port/r36s-portmaster/docs/R36S_LEGAL.md)
- [Contributing to the R36S branch](https://github.com/dehalbuq/ki-recomp/blob/port/r36s-portmaster/CONTRIBUTING.md)
- [Guia em português](https://github.com/dehalbuq/ki-recomp/blob/port/r36s-portmaster/docs/R36S_README_PTBR.md)
