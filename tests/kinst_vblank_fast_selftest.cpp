// Synthetic, ROM-free differential test: compare the R36S next-event
// VBlank fast path with the original modulo-based arcade timing.
#include "ki/kinst_board.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

int main() {
    constexpr uint64_t F = ki::KinstBoard::kCyclesPerFrame;
    constexpr uint64_t V = ki::KinstBoard::kVblankStart;
    // Include the snapshot phase where cycle 0 begins inside VBlank.
    const std::array<uint64_t, 9> offsets{
        0, 1, V - 1, V, V + 1, F - 1, F, F + 1, 2 * F - 1
    };
    uint64_t checks = 0;
    for (uint64_t phase : offsets) {
        ki::KinstBoard normal({}, {}), fast({}, {});
        normal.set_cycles_to_vblank(phase);
        fast.set_cycles_to_vblank(phase);
        normal.set_fast_vblank_irq(false);
        fast.set_fast_vblank_irq(true);
        ki::Context ctx{};
        std::vector<uint64_t> sample_times;
        auto around = [&](uint64_t t) {
            if (t) sample_times.push_back(t-1);
            sample_times.push_back(t);
            sample_times.push_back(t+1);
        };
        for (uint64_t f = 0; f <= 6; ++f) {
            const uint64_t start = (phase % F ? phase % F : F) + f * F;
            around(start);
            around(start + (F - V));
        }
        for (uint64_t f = 0; f <= 6; ++f)
            for (uint64_t sub = 0; sub < 60; ++sub)
                sample_times.push_back(f * F + sub * (F / 60));
        std::sort(sample_times.begin(), sample_times.end());
        sample_times.erase(std::unique(sample_times.begin(), sample_times.end()), sample_times.end());
        // Run both board clocks through identical events; deliberately leave
        // some event boundaries unserviced until the next point (fallback).
        for (size_t n = 0; n < sample_times.size(); ++n) {
            ctx.cycles = sample_times[n];
            const auto expected = normal.interrupt_lines(ctx);
            const auto actual = fast.interrupt_lines(ctx);
            ++checks;
            if (expected != actual) {
                std::fprintf(stderr,
                    "VBlank mismatch phase=%llu cycle=%llu old=%x fast=%x\n",
                    (unsigned long long)phase, (unsigned long long)ctx.cycles,
                    expected, actual);
                return 1;
            }
            if ((n % 13) != 0) {
                normal.update(ctx);
                fast.update(ctx);
            }
        }
    }
    std::printf("VBlank fast deadline equivalent to modulo formula: %llu checks\n",
                (unsigned long long)checks);
    return 0;
}
