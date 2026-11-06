# R36S Test19 — Cortex-A35 NEON + Mali-450 OpenGL ES2 upload experiments

## Why these are safe GPU/CPU acceleration candidates

The original arcade does **not** render polygons in real time: its MIPS
R4600 game code draws pre-rendered sprites, panoramas and effects into
320x240 packed RGB555 guest SRAM. The Mali-450 already accelerates the
final output through SDL's `opengles2` texture renderer.

On DarkOSRE, Test17 reduced the GLES2 upload latency substantially by
switching from SDL RGB24 to native GL_RGBA-compatible ABGR8888 textures.
Test18 kept that path and added audio smoothing. The user measured 53.24
average guest FPS with smooth audio against 52.45 with the original
audio in runs of different duration/scene composition. WSOLA is a
presentation-layer improvement, not proof that simulation is faster.

Test19 adds independent host-side optimizations while keeping *all* MIPS
CPU, DSP, framebuffer SRAM, original cycle accounting and sprites intact.

## What changed in Test19

- **Cortex-A35 ARM NEON SIMD**: 8 RGB555 pixels at a time -> exact
  R8 G8 B8 FF destination bytes using NEON lane masks, bit expansion
  and interleaved `vst4_u8`. ARM64 is supported by the R36S; x86 and
  other platforms stay scalar. Never introduces new colors or changes
  the original framebuffer. The existing scalar path is available via
  `KI_R36S_RGBA_SCALAR=1`.
- **Mali-450 / SDL2 direct texture fill**, enabled with
  `KI_R36S_DIRECT_TEXTURE=1`: `SDL_LockTexture` exposes the 320x240
  GLES2 streaming texture's staging memory; NEON converts straight
  into that memory, respecting the returned pitch. No extra CPU
  allocation/copy into a `std::vector<uint32_t>` is needed.
  `SDL_UnlockTexture` submits through SDL's renderer.
  If locking is unsupported or returns too-small pitch, logging
  reports a fallback to the previously tested `SDL_UpdateTexture`
  path. This is **not** an undocumented OpenGL shader rewrite.
- **Optional redundant clear skip**, enabled with `KI_R36S_NO_CLEAR=1`:
  the opaque 320x240 RGB555->RGBA texture fills the entire R36S 4:3
  panel, so the preliminary black `SDL_RenderClear` is potentially
  unnecessary. The original clear remains the default; if borders,
  trails or artifacts appear in no-clear mode, do not enable it.
- **Low-overhead GPU-only profiler** via `KI_R36S_GPU_PROFILE=1`:
  prints `GFX_SDL_ES2` and `TEST19` metrics without timing the
  original recompiled MIPS sprite functions at every call.

All Test19 launcher scripts enable the RGBA path, WSOLA sound, and
`KI_R36S_GPU_PROFILE` under otherwise identical settings.

| Launcher | NEON | Upload | SDL clear |
|---|---|---|---|
| `KI R36S - Test19 NEON Update.sh` | yes | original `SDL_UpdateTexture` | on |
| `KI R36S - Test19 NEON Locked.sh` | yes | `SDL_LockTexture` direct | on |
| `KI R36S - Test19 Scalar Locked.sh` | no | `SDL_LockTexture` direct | on |
| `KI R36S - Test19 NEON NoClear.sh` | yes | `SDL_LockTexture` direct | off |

If the locked texture falls back, its log reports
`TEST19 direct_texture_active=0`, so do not mislabel that result as
a successful direct upload.

## Verification and limits

The ARM64 `gles2_rgba_upload_selftest` compares vector and scalar
output across all **65,536** RGB555 input words (bit15 ignored),
plus odd widths, misaligned source/destination, texture row pitch and
padding canaries. It checks the original guest data is not mutated.
The SDL player is syntax-checked using AArch64 host SDL2 headers.

Hardware-only tests remain necessary: `SDL_LockTexture` is supported
by SDL2's streaming-texture API, but the firmware's GLES2 driver and
internal staging allocation determine whether it is actually faster.

No guest MIPS sprite or panorama logic has been moved to OpenGL ES2.
Those routines write guest-visible SRAM, so simply replacing them
with GPU rectangles would corrupt game state unless CPU-visible memory
and effects are kept exact. They remain priority bottlenecks
(about 2.38 and 1.86 ms per original guest frame in the profiled
Test16 fighting scenes). Future offloading needs identical framebuffer
hashes, guest memory semantics, cycle/interrupt equivalence and
hardware benchmarks before being enabled.

## Test on dArkOSRE (same scene, repeat A/B)

1. Back up any existing installation/saves; extract the private Test19
   ZIP into `roms/ports`, retaining legal `kinst.zip` and `kinst.chd`
   in `killerinstinct/gamedata`.
2. Run **NEON Update**, then **NEON Locked**, then **NEON NoClear**,
   each for at least 60 seconds on the *same fighting stage and fighters*.
   `Scalar Locked` can additionally isolate the NEON conversion.
3. Check screen colors, full-screen coverage, HUD/menu and audio.
   Confirm any direct-lock fallback logs.
4. Send the 3-4 `ki-r36s-test19-*.log` files from
   `killerinstinct/logs/`.

Important log fields: `PERF fps`, `GFX_SDL_ES2
upload_ms_per_frame`, `cpu_rgb_ms_per_frame`, and
`TEST19 direct_texture_active / scalar / locked_convert_ms_per_frame`.
With direct lock, the `upload_ms_per_frame` includes pixel conversion
inside the lock, so compare **total_render_ms_per_frame**, FPS and
`swap_ms_per_frame` as well. The speed gain is unknown until the real
hardware logs arrive; do not claim 59 FPS based only on CI.

WSOLA pitch-preserving adaptive output stays ON in every candidate,
and original MIPS/DSP speeds, guest clock, sample generation and full
frame presentation are unchanged.
