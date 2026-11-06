// R36S Test16: opt-in native stage/sprite blitter time accounting.
// Records CPU+system wall time in the original translated MIPS routines.
// Does NOT change framebuffer output, game cycles, audio, or SDL rendering.
// Sampling is OFF on normal game runs: no clock calls unless explicitly set.
#pragma once
#if defined(KI_R36S) || defined(KI_R36S_GFX_PROFILE_SUPPORTED)
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace ki_r36s::gfx_profiler {
enum class Part : unsigned { Sprite, Panorama, Prerendered, Setup, ScreenSave, ScreenRestore, Count };
constexpr unsigned kCount = static_cast<unsigned>(Part::Count);
inline bool enabled() {
    static const bool on = std::getenv("KI_R36S_GFX_PROFILE") != nullptr;
    return on;
}
struct Measure {
    uint64_t nanos = 0;
    uint64_t calls = 0;
};
inline std::array<Measure, kCount> measurements{};
inline const char* name(Part part) {
    switch (part) {
        case Part::Sprite: return "sprite_880107a8";
        case Part::Panorama: return "panorama_88018c98";
        case Part::Prerendered: return "prerendered_88030b9c";
        case Part::Setup: return "stage_setup_8803108c";
        case Part::ScreenSave: return "screen_save_88023008";
        case Part::ScreenRestore: return "screen_restore_8802309c";
        default: return "unknown";
    }
}
struct Scope {
    Part part;
    bool active;
    std::chrono::steady_clock::time_point start;
    explicit Scope(Part p) : part(p), active(enabled()) {
        if (active) start = std::chrono::steady_clock::now();
    }
    ~Scope() {
        if (!active) return;
        const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start).count();
        auto& record = measurements[static_cast<unsigned>(part)];
        record.nanos += nanos > 0 ? static_cast<uint64_t>(nanos) : 0u;
        ++record.calls;
    }
};
inline void report(unsigned guest_frames) {
    if (!enabled()) return;
    double summed_ms = 0;
    std::fprintf(stderr, "[r36s] GFX_PROFILE guest_frames=%u timed_original_mips_blitters=1\n", guest_frames);
    for (unsigned i = 0; i < kCount; ++i) {
        const Part part = static_cast<Part>(i);
        auto& a = measurements[i];
        const double ms = a.nanos / 1000000.0;
        summed_ms += ms;
        std::fprintf(stderr,
            "[r36s] GFX_MIPS name=%s calls=%llu ms_total=%.3f ms_per_guest_frame=%.3f\n",
            name(part), static_cast<unsigned long long>(a.calls), ms,
            guest_frames ? ms / guest_frames : 0.0);
        a = {};
    }
    // Some guest graphics routines are not among the wrapped entry points;
    // this sum is NOT the total MIPS rendering time.
    std::fprintf(stderr,
        "[r36s] GFX_PROFILE_SUM ms_total=%.3f ms_per_guest_frame=%.3f note=selected_blitters_only\n",
        summed_ms, guest_frames ? summed_ms / guest_frames : 0.0);
}
} // namespace ki_r36s::gfx_profiler
#endif
