// Overrides around sprite and stage drawing (built with --override, see CMakeLists.txt).
#include "ki/hooks.h"
#include "recompiled.h"
#if defined(KI_R36S_GFX_PROFILE_SUPPORTED)
#include "../ki_r36s_gfx_profiler.h"
#define KI_GFX_TIME(part) ki_r36s::gfx_profiler::Scope ki_gfx_scope(ki_r36s::gfx_profiler::Part::part)
#else
#define KI_GFX_TIME(part) ((void)0)
#endif

namespace ki::gen {

// The scaled sprite blitter: lets the host observe each sprite drawn, then
// draws it with the recompiled original.
void func_880107a8(ki::Context& c) {
    KI_GFX_TIME(Sprite);
    if (ki::hooks::sprite) ki::hooks::sprite(c);
    func_880107a8_original(c);
    // The translation ends in tail calls (j to other parts of the blitter):
    // finish them here, so the "after" hook sees the sprite drawn. The call
    // site would run them next anyway.
    ki::drain(c);
    if (ki::hooks::sprite_after) ki::hooks::sprite_after(c);
}

// The stage panorama copy (sky and buildings of some stages): lets the host
// replace what was copied with its HD panorama.
void func_88018c98(ki::Context& c) {
    KI_GFX_TIME(Panorama);
    if (ki::hooks::panorama) ki::hooks::panorama(c);
    func_88018c98_original(c);
    ki::drain(c);
    if (ki::hooks::panorama_after) ki::hooks::panorama_after(c);
}

// The pre-rendered stage renderer (whole screen from a stage frame).
void func_88030b9c(ki::Context& c) {
    KI_GFX_TIME(Prerendered);
    if (ki::hooks::prerendered) ki::hooks::prerendered(c);
    func_88030b9c_original(c);
    ki::drain(c);
    if (ki::hooks::prerendered_after) ki::hooks::prerendered_after(c);
}

}  // namespace ki::gen

namespace ki::gen {

// Set-up step of the pre-rendered renderer (its source pointer is final after it).
void func_8803108c(ki::Context& c) {
    KI_GFX_TIME(Setup);
    if (ki::hooks::prerendered_setup) ki::hooks::prerendered_setup(c);
    func_8803108c_original(c);
    ki::drain(c);
    if (ki::hooks::prerendered_setup_after) ki::hooks::prerendered_setup_after(c);
}

// The end-of-fight screen (saved, darkened frame under the text).
void func_88023008(ki::Context& c) {
    KI_GFX_TIME(ScreenSave);
    func_88023008_original(c);
    ki::drain(c);
    if (ki::hooks::screen_save_after) ki::hooks::screen_save_after(c);
}

void func_8802309c(ki::Context& c) {
    KI_GFX_TIME(ScreenRestore);
    func_8802309c_original(c);
    ki::drain(c);
    if (ki::hooks::screen_restore_after) ki::hooks::screen_restore_after(c);
}

}  // namespace ki::gen
