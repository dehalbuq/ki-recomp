# Killer Instinct R36S — Test17 native GLES2 RGBA upload (exact RGB555 colors)

## Test16 user's DarkOSRE profile: what actually hurts

From `ki-r36s-gfx-profile.log`, 30 diagnostic windows. Filtering
24 windows with >5,000 original sprite-blitter calls (actual fights)
gives:

| Sample mean | ms per guest framebuffer/frame |
|---|---:|
| SDL_UpdateTexture streaming RGB24 | **5.007 ms** |
| SDL_RenderCopyF (draw submission) | **0.016 ms** |
| SDL_RenderPresent | **2.075 ms** |
| BGR555 -> RGB24 software conversion | **0.231 ms** |
| Total player rendering from frame setup to present | **7.343 ms** |
| Original translated MIPS sprite draws (func_880107a8) | **2.377 ms** |
| Original translated MIPS panorama draws (func_88018c98) | **1.859 ms** |
| Observed FPS with diagnostic profile | **45.25 FPS** |

The pre-rendered stage renderer 0x88030b9c registered no calls during
this scene; screen-save/restore appear only during transition events.

This isolates two separate bottlenecks:
- **5ms/frame** overhead inside SDL2's RGB24 texture upload on GLES2,
  possibly due to converting a 3-byte texture source to a GL-compatible
  4-byte internal format or copying through an extra software staging
  texture. This is CPU/driver timing, **not** automatically GPU
  execution time.
- ~4.24ms/frame executing original recompiled MIPS sprite/panorama
  blits (in addition to the final GPU presentation). Actual hardware
  offload of these blitters needs careful framebuffer coherency.

## What is implemented now (ROM-free, optional)

The original KI arcade framebuffer lives in SRAM as packed BGR555:
- bits 0..4: red5;
- bits 5..9: green5;
- bits 10..14: blue5;
- bit 15 unused.

Original expansion of any 5-bit channel to 8 bits is exact:
`(value5 << 3) | (value5 >> 2)`.

Test17 adds `KinstBoard::framebuffer_rgba_into()`, writing
`SDL_PIXELFORMAT_ABGR8888` on little-endian AArch64:
memory bytes are **R8,G8,B8,A8** with A8=255. Each 5-bit source
channel retains the exact original 8-bit expanded result.

Under `KI_R36S_GLES2_RGBA=1`, the SDL renderer requests a native
ABGR8888 streaming texture and uses `SDL_UpdateTexture` with
320x240x4 bytes. This avoids having to upload **RGB24**, which SDL's
GLES2 backend may otherwise convert to another format internally.
The image is still scaled to **640x480 nearest-neighbor** (perfect 4:3
pixel mapping), with full sound and original 58.98Hz guest cadence.

The old RGB24 path remains default and unchanged. If the SDL renderer
cannot create ABGR8888, the code logs the error and falls back to
RGB24, still displaying every guest frame.

**Do not claim 5ms saved without an actual R36S benchmark.** It is
possible the RGBA staging copy or extra 32-bit texture bandwidth will
cost more, or the SDL GLES2 renderer still converts to a slow driver
format. Even if the host upload becomes faster, guest MIPS and DSP
costs remain.

## Color and program correctness

The ROM-free `gles2_rgba_upload_selftest` compares every one of the
**32768** BGR555 source colors against the original RGB24 channel
expansion. It also fills a full synthetic 320x240 KI framebuffer
(including bit15 combinations) and compares all 76800 pixels,
checking byte-exact RGB channel values and opaque alpha.
This test is executed on native ARM64 CI and private builds.

Test17 does not edit MIPS guest memory, trigger extra emulated cycles,
disable the audio DSP, skip guest video frames, or modify the
sprite/panorama software rasterizers. The normal launcher's code path
is identical except for an OFF-by-default environment check.

## R36S A/B benchmark

Install Test17 over `roms/ports/` without overwriting game data
and save files:

1. **KI R36S - Graphics Profile**: original RGB24 GLES2 SDL upload.
   Log `killerinstinct/logs/ki-r36s-gfx-profile.log`.
2. **KI R36S - GLES2 RGBA Fast**: exact-color 32-bit RGBA texture.
   Log `killerinstinct/logs/ki-r36s-gles2-rgba-fast.log`.
3. Optionally **Killer Instinct Arcade**: baseline no extra profiler
   overhead; normal full-audio performance.

Run same moving/background stage and same character for 45–60 seconds
in each mode. Read `[r36s] GLES2 framebuffer upload:` to verify
that ABGR8888 mode is active (else test fell back to RGB24).
Compare `GFX_SDL_ES2 upload_ms_per_frame=...`, render/swap,
FPS, visual colors, sound, and the presence of game UI.

If RGBA significantly reduces SDL upload without glitches, consider
making it the default in Test18. If not, we can isolate an explicit
native GLES2 GL_UNSIGNED_SHORT_5_5_5_1 framebuffer upload and
shader display path; that is a **larger renderer rewrite** that must
not compete with SDL's GL state in the same frame without correct
render-context ownership and menu integration.

A second independent candidate is native GPU offload of
`func_880107a8` and `func_88018c98`, but these routines can
modify memory that the MIPS CPU reads during rendering. To replace
them, first verify clipping, overlap, palette state, source identity,
BGR555 output, and framebuffer readback requirements. Batching
textured quads *after* retaining the original MIPS draws would not
eliminate the CPU work or improve FPS.

References:
- Source `src/ki_play.cpp`
- Board conversion `runtime/src/kinst_board.cpp`
- Original guest graphics wrappers `src/overrides/sprites.cpp`
- Previous profile notes: `docs/R36S_TEST16_GLES2_GPU_BLITTER_STUDY.md`
