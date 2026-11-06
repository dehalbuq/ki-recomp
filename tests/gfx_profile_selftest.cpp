// Standalone ROM-free test for opt-in R36S GPU offload feasibility timing.
#include "../src/ki_r36s_gfx_profiler.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
int main() {
    if (setenv("KI_R36S_GFX_PROFILE", "1", 1) != 0) return 2;
    namespace gfx = ki_r36s::gfx_profiler;
    if (!gfx::enabled()) return 3;
    {
        gfx::Scope span(gfx::Part::Sprite);
        volatile int n=0;
        for (int i=0;i<1000;++i) n=n+i;
        (void)n;
    }
    const auto& a=gfx::measurements[static_cast<unsigned>(gfx::Part::Sprite)];
    if (a.calls != 1u || a.nanos == 0u) return 4;
    gfx::report(1);
    if (gfx::measurements[static_cast<unsigned>(gfx::Part::Sprite)].calls != 0u) return 5;
    std::puts("GFX MIPS blitter timing and per-window reset passed");
    return 0;
}
