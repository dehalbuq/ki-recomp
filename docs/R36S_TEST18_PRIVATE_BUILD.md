# Killer Instinct R36S — Test18 RGBA + adaptive WSOLA audio

**Status:** Test18 source and ROM-free ARM64 checks exist. A real playable
`ki_play.aarch64` can only be compiled with the owner's original arcade files.
The public GitHub Actions workflow intentionally cannot create it.

## What must remain identical between A/B runs

Both launchers set `KI_R36S_GLES2_RGBA=1`, keep the full DSP and guest
58.98 Hz cadence, present every game frame, and use the same full program:

- `KI R36S - RGBA Audio Original.sh`: unmodified 31,250 Hz DSP samples
  sent directly to SDL2 (control from Test17).
- `KI R36S - RGBA Audio Smooth.sh`: experimental pitch-preserving
  adaptive WSOLA after the emulated DSP and before the SDL queue.

Logs are written to `killerinstinct/logs/ki-r36s-rgba-audio-original.log`
and `killerinstinct/logs/ki-r36s-rgba-audio-smooth.log`.

## Build option A: privately on native ARM64 GitHub Actions

This is the recommended automated packaging route **only if you already have
a private repository with legally obtained original game assets**.

1. In a **PRIVATE** GitHub repository, add `data/kinst.zip` (including
   KI1 BIOS L1.5D and all sound ROMs) and `data/kinst.chd`, using Git LFS
   as appropriate. NEVER commit these to `dehalbuq/ki-recomp`.
2. Copy `tools/r36s/private-ci-workflow.yml.template` from the public
   `port/r36s-portmaster` branch to
   `.github/workflows/build-r36s.yml` in the PRIVATE repository.
3. In the private repo, open **Actions** → **KI R36S Test18 - PRIVATE
   optimized ARM64 build** → **Run workflow**.
4. If the job succeeds, download artifact
   `KI-R36S-Test18-RGBA-WSOLA-private` (a ZIP containing the
   PortMaster ZIP). It expires in three days. Do **not** publish the
   compiled binary or artifact publicly; translated code may reflect
   original game material even though the ZIP excludes ROM/CHD files.
5. Extract the inner `KI-R36S-Test18-RGBA-WSOLA-private.zip` over your
   existing `roms/ports` directory, preserving your existing saves and
   `killerinstinct/gamedata/`.

The workflow builds `Release` and sets `KI_R36S_MIPS_O3=ON`, to
preserve the tested Test17 fast MIPS variant. This does **not** guarantee
compatibility with DarkOSre/ArkOS4Clone: GitHub's Ubuntu 24.04 ARM64
runner can link against a newer glibc than the handheld firmware.
Check the dynamic library/GLIBC requirements printed by the job and
run the resulting executable on the actual R36S before treating it as
compatible. If necessary, rebuild with the target firmware sysroot.

## Build option B: local ARM64 / compatible AArch64 sysroot

After locally preparing your original arcade game files with
`tools/r36s/prepare_data.py` (see `docs/R36S_PORT.md`):

```bash
KI_BUILD_TYPE=Release KI_R36S_MIPS_O3=ON KI_JOBS=2 \
  bash tools/r36s/build.sh
python3 tools/r36s/package_port.py build-r36s/ki_play \
  --output KI-R36S-Test18-RGBA-WSOLA-private.zip
```

`build.sh` validates that BOTH compilers generate AArch64 code.
If rebuilding on x86_64, also supply target AArch64 compilers and a
firmware-compatible sysroot; never bundle random glibc/loader files.

## DarkOSre A/B hardware procedure

1. Copy your legitimate `kinst.zip` and `kinst.chd` to
   `roms/ports/killerinstinct/gamedata/` if not already present.
2. Open **KI R36S - RGBA Audio Original** and enter a real fight on a
   moving-background stage. Play at least 60 seconds. Note how often
   audio clicks, whether there is delay and the average FPS.
3. Open **KI R36S - RGBA Audio Smooth** and repeat the **same scene and
   characters** for 60 seconds. Listen for metallic/chorus artifacts,
   excessive latency, audio drops and changes in perceived pitch.
4. Compare the resulting two logs. Look for
   `guest vblank rate`, `GFX_SDL_ES2`, and `AUDIO_SMOOTH`.
   The smooth log reports `stretch`, input/output sample counts,
   `queue_min_ms`, `queue_max_ms`, `under20` and `primed`.
5. If the smoothed mode sounds worse, retain the original launcher.
   The original DSP samples are never changed by either mode.

**Acceptance criteria:** no crashes, correct colors at 4:3 nearest,
no accidental frame skipping, no game-speed increase, no noticeable
pitch drift, fewer or no underruns, and no significant FPS regression
relative to the matching RGBA Audio Original run. The original arcade
runs near 58.98 frames/s; Test17's ~50 FPS is a measured baseline,
not evidence that Test18 already fixes the audio on hardware.

## Public verification scope

The public workflow on the branch runs only source checks, synthetic
ROM-free audio/graphics regressions, ELF architecture checks and a
PortMaster packaging smoke test with a **fake synthetic ELF**. It must
never upload a playable KI build, game ROMs, CHD, or game-derived C++.
