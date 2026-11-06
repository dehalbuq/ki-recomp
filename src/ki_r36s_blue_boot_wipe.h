// R36S Test21: bounded, opt-in, boot-only *presentation* fast-forward
// for the distinctive blue curtain wiping downward during cold startup.
// It NEVER alters MIPS instructions, DSP emulation, guest IRQ timing,
// framebuffer SRAM, or the 58.98-Hz normal gameplay pacer.
// A very cheap sample of RGB555 guest VRAM detects an opaque blue
// horizontal region growing from top to bottom. No hard-coded ROM PCs,
// timer-based guessing, or speed changes once the curtain is gone.
#pragma once
#include <cstddef>
#include <cstdint>

namespace ki_r36s {
class BlueBootWipe {
public:
    static constexpr unsigned kWidth = 320, kHeight = 240;
    static constexpr unsigned kProbeRows = 30;
    static constexpr unsigned kProbeCols = 12;
    static constexpr uint64_t kMaxBootFrames = 720;  // ~12 guest seconds

    struct Probe {
        unsigned blue_rows = 0;
        unsigned top_rows = 0;
        unsigned bottom_rows = 0;
    };
    static bool blue_pixel(uint16_t pixel) {
        const unsigned r = pixel & 31u;
        const unsigned g = (pixel >> 5) & 31u;
        const unsigned b = (pixel >> 10) & 31u;
        // Strong blue only; never mistake black, grayscale or the blue
        // highlight pixels of a character for an opaque blue screen.
        return b >= 10u && b >= r + 4u && b >= g + 3u;
    }
    static Probe probe(const uint8_t* sram_rgb555) {
        bool is_blue[kProbeRows]{};
        Probe out{};
        for (unsigned row=0; row<kProbeRows; ++row) {
            const unsigned y = row * 8u + 4u;
            unsigned blues=0;
            for (unsigned col=0; col<kProbeCols; ++col) {
                const unsigned x=13u + col*26u;
                const size_t off=2u*(y*kWidth+x);
                const uint16_t pix=static_cast<uint16_t>(
                    sram_rgb555[off] | (static_cast<uint16_t>(sram_rgb555[off+1])<<8));
                blues+=blue_pixel(pix);
            }
            // At least 75% of sampled row must be dominantly blue.
            is_blue[row] = blues >= 9u;
            out.blue_rows += is_blue[row];
        }
        while (out.top_rows < kProbeRows && is_blue[out.top_rows]) ++out.top_rows;
        while (out.bottom_rows < kProbeRows && is_blue[kProbeRows - 1 - out.bottom_rows])
            ++out.bottom_rows;
        return out;
    }

    bool update(const uint8_t* sram_rgb555, uint64_t frame) {
        if (finished_) return false;
        if (frame > kMaxBootFrames) {
            finished_ = true;
            boosted_ = false;
            return false;
        }
        const Probe p = probe(sram_rgb555);
        last_ = p;
        // Require an obvious, horizontally uniform BLUE curtain from the
        // *top*, with the lower screen not yet covered.
        const bool curtain = p.top_rows >= 4u && p.top_rows <= 26u &&
                             p.bottom_rows <= 2u &&
                             p.blue_rows >= p.top_rows &&
                             p.blue_rows <= p.top_rows + 4u;
        if (!boosted_) {
            if (curtain) {
                if (++confirmed_ >= 3u) {
                    boosted_=true;
                    ever_started_=true;
                }
            } else {
                confirmed_=0;
            }
        } else {
            // Stop at the end of the transition, if the blue area disappears
            // or covers almost the full display, or once the safe boot limit
            // is exceeded. Do not reactivate during gameplay.
            if (p.top_rows >= 28u || p.top_rows <= 1u ||
                p.bottom_rows >= 28u || !curtain) {
                finished_=true;
                boosted_=false;
            }
        }
        return boosted_;
    }
    bool active() const { return boosted_; }
    bool finished() const { return finished_; }
    bool ever_started() const { return ever_started_; }
    const Probe& last_probe() const { return last_; }

private:
    Probe last_{};
    unsigned confirmed_=0;
    bool boosted_=false, finished_=false, ever_started_=false;
};
} // namespace ki_r36s
