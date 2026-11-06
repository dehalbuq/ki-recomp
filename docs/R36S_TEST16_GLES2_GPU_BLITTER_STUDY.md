# R36S — Killer Instinct Test16: GPU offload feasibility and true renderer hotspots

## Graphics architecture: **pre-rendered 3D, but 2D execution**

The original 1994 Killer Instinct arcade does not rasterize textured
triangles or polygon models in real time. Rare pre-rendered characters
and backgrounds on Silicon Graphics workstations. The arcade R4600
combines sprite frames, pre-rendered stage frames/movies, panoramas and
screen effects into a pair of **320 × 240 BGR555 SRAM framebuffers**.

This makes a Mali-450 OpenGL ES 2.0 accelerator plausible for **2D
texture composition**, not for running a nonexistent original 3D
rendering engine.

The current port already uses hardware acceleration in
`src/ki_play.cpp`: `SDL_RENDERER_ACCELERATED` selects `opengles2`,
`SDL_UpdateTexture` uploads the 320 × 240 RGB image, and
`SDL_RenderCopyF` scales it to the 640 × 480 4:3 panel with nearest
sampling. **That only accelerates final presentation.** The original
MIPS guest rendering code still constructs source pixels in the guest
SRAM framebuffer.

Some of the guest blit routines are already wrapped:
- `func_880107a8`: scaled sprite drawing;
- `func_88018c98`: stage panorama/sky copy;
- `func_88030b9c`: pre-rendered background frame renderer;
- `func_8803108c`: pre-rendered background setup;
- `func_88023008`, `func_8802309c`: screen save/restore effects.

Thus the GPU *could* render their textured rectangles if the native
replacement exactly emulates scaling, clipping, palette changes,
transparency, effects, screen ordering and guest-visible framebuffer
side effects. Simply drawing additional GPU textures **after** the old
MIPS blit cannot help: the CPU cost remains, and redundant drawing adds
work. Equally, disabling guest MIPS drawing without considering guest
framebuffer reads may break effects, HUD, saves and stage visibility.

## Real Test15 numbers from user's four DarkOSRE logs

| Last 10 x5s samples | Guest FPS | Interpretation |
|---|---:|---|
| `ki-r36s-mips-baseline.log` | 33.11 | no MIPS Turbo, old memory |
| `ki-r36s-memory-baseline.log` | 44.42 | Turbo, original RAM path |
| `ki-r36s(9).log` | 43.62 | Turbo, inlined SRAM/DRAM, GCC O2 |
| `ki-r36s-compiler-o3.log` | **45.06** | Turbo, inlined SRAM/DRAM, GCC O3 |

The `Memory Baseline` run includes a later high-FPS (54.1 FPS)
window and other modes include menu activity, so these are not exact
same-scene controlled comparisons. O3 is a **promising candidate**
but should not be universally declared faster without scene-matched
runs. The direct-RAM path did not show clear independent improvement.

Across ordinary stable gameplay, the player logs approximately
**7.3 ms per guest frame** in the whole SDL render/present path,
including approximately **2.0 ms** spent in `SDL_RenderPresent` and
**0.23 ms** expanding BGR555 to RGB24. The difference between total
render and measured swap is **not** automatically GPU cost; it includes
CPU driver work and texture uploads. `SDL_RendererInfo.flags=0xe`
reports the GLES2 renderer and active VSync even though a disable
was requested; the hardware/driver may still synchronize swaps.

At 45 FPS the time budget is about **22.2 ms/frame**. The arcade
target is approximately **58.98 FPS**, or **16.95 ms/frame**. Achieving
that requires saving around **5.3 ms per guest frame** under equivalent
scenes without changing guest game speed or skipping frames.

## Test16 native graphics profiling instrumentation (implemented)

The PortMaster launcher `KI R36S - Graphics Profile` enables
`KI_R36S_GFX_PROFILE=1`, and writes to
`killerinstinct/logs/ki-r36s-gfx-profile.log`. This does NOT enable
different rendering or disable the DSP.

The original MIPS function wrappers now use diagnostic-only scoped
steady-clock timers. Each five seconds they report:

```
[r36s] GFX_PROFILE guest_frames=N timed_original_mips_blitters=1
[r36s] GFX_MIPS name=sprite_880107a8 calls=N ms_total=X ms_per_guest_frame=Y
[r36s] GFX_MIPS name=panorama_88018c98 ...
[r36s] GFX_MIPS name=prerendered_88030b9c ...
[r36s] GFX_MIPS name=stage_setup_8803108c ...
[r36s] GFX_MIPS name=screen_save_88023008 ...
[r36s] GFX_MIPS name=screen_restore_8802309c ...
[r36s] GFX_PROFILE_SUM ms_total=X ms_per_guest_frame=Y note=selected_blitters_only
[r36s] GFX_SDL_ES2 upload_ms_per_frame=X copy_submit_ms_per_frame=Y
        total_render_ms_per_frame=Z swap_ms_per_frame=W
        cpu_rgb_ms_per_frame=A samples=N
```

The wrappers surround the **entire original function including
translated tail calls**, so guest cycle side effects are preserved.
These are wall-clock values, not pure GPU-device timing; they may
include nested and other non-raster work. The measured six entry
points are **not** the total MIPS graphics cost. For remaining
hotspots, also run the existing native `KI R36S - Host Profile`
launcher (separately, because sampling adds overhead).

The normal launcher does not call the timer clock unless diagnostics
are enabled. Test16 includes a ROM-free `gfx_profile_selftest`, and
the original DSP, MIPS Turbo and both GCC O2/O3 variants remain
available.

## How to test on DarkOSRE

Extract Test16 into `roms/ports/`, preserving your own
`kinst.zip`, `kinst.chd`, configuration and saves.

1. Open **KI R36S - Graphics Profile** and enter a **real fight** on
   a moving or zooming stage. Play ~60 seconds and exit Start+Select.
2. To compare a different scene type, repeat on an arena that has
   panoramas and on a stage with a pre-rendered moving background.
3. Send `killerinstinct/logs/ki-r36s-gfx-profile.log`.
4. Optional: run `KI R36S - Host Profile` over the same scene to
   identify additional native generated MIPS functions that do not
   appear in the six wrapped functions.

Normal `Killer Instinct Arcade` stays optimized and free of
per-function timer overhead. Other existing PortMaster A/B entries
still isolate memory speed and O3 compiler flags.

## Next GPU engineering phase (not implemented in Test16)

1. First choose the blitter with the largest measured **millisecond
   total per guest frame** in a representative fight.
2. Decode the relevant original frame/sprite into packed BGR555 or
   RGBA texture data, with a **cache keyed by source identity and
   palette/animation state**. Texture upload must not occur each frame
   if the source content is unchanged.
3. Prototype native GLES2 **textured-quad batching**, nearest
   filtering and correct pixel-aligned/scaled placement.
4. Preserve guest memory semantics. If framebuffer is observed/read
   by the game after the blit, supply an exact software or CPU-visible
   shadow path; avoid synchronous GPU readback which could eliminate
   the speed gain.
5. Compare GPU candidate and original MIPS blitter on identical
   inputs with complete RGB555 frame checksums, game state, audio
   PCM and timer/IRQ checks.
6. Keep an environment-controlled fallback to the original MIPS
   rasterizer for any stage, effect, palette mode or scene not proven
   equivalent.
7. Re-measure on the Cortex-A35 / Mali-450 at 4:3 perfect pixel,
   full audio, native refresh ~58.98 Hz.

**This is an instrumentation build**, not a GPU-blitter release and
not an unsupported claim that the arcade has real-time 3D geometry.

References:
- MAME's `src/mame/rare/kinst.cpp`
- `src/ki_sprites.cpp`, `src/overrides/sprites.cpp`
- https://www.timeextension.com/features/the-making-of-killer-instinct-nintendos-street-fighter-and-mortal-kombat-killer
