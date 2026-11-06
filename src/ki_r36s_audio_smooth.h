// R36S Test18: optional pitch-preserving, CPU-light WSOLA/SOLA audio
// time stretching. Prevents the 31250Hz SDL queue from starving when guest
// video runs below its original 58.98Hz rate. Does NOT advance DSP cycles,
// change IRQs, skip game frames, or alter game-produced PCM source samples.
//
// Important: with low FPS, original 31250 samples/s of *guest* sound arrive
// at ~25000 samples/s of *wall time*. Larger buffering alone cannot fix that
// continuous deficit. Instead, short overlapping PCM grains preserve pitch
// while adapting the number of output samples to wall-clock frame cadence.
//
// This is an experimental playback policy, not a change to arcade emulation.
// Normal SDL_QueueAudio playback remains available as an exact baseline.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace ki_r36s {

class AudioSmooth {
public:
    static constexpr unsigned kRate = 31250;
    static constexpr unsigned kHop = 256;
    static constexpr unsigned kWindow = 512;
    static constexpr unsigned kSearch = 12;
    static constexpr unsigned kTargetQueueMs = 85;
    static constexpr unsigned kStartQueueMs = 90;
    static constexpr unsigned kMaxQueueMs = 240;

    struct Stats {
        uint64_t input_samples = 0;
        uint64_t output_samples = 0;
        uint64_t frames = 0;
        uint64_t low_queue_frames = 0;
        uint32_t min_queue_ms = 0xffffffffu;
        uint32_t max_queue_ms = 0;
        double stretch = 1.0;
        void clear() { *this = {}; }
    };

    AudioSmooth() { input_.reserve(kRate / 2); output_.reserve(kRate / 5); }

    void reset() {
        input_.clear();
        output_.clear();
        initialized_ = false;
        last_start_ = 0;
        ratio_ = 1.0;
        stats_ = {};
    }

    // Samples are input at true emulated DSP sample cadence. The caller
    // passes the real time between guest frames; queue_bytes is the *actual*
    // SDL playback backlog measured immediately before this frame's upload.
    // The returned samples remain valid until the next process/reset call.
    const std::vector<int16_t>& process(const std::vector<int16_t>& pcm,
                                         double wall_frame_seconds,
                                         uint32_t queue_bytes) {
        output_.clear();
        const uint32_t queue_ms = static_cast<uint32_t>(
            (static_cast<uint64_t>(queue_bytes) * 1000) / (2 * kRate));
        stats_.min_queue_ms = std::min(stats_.min_queue_ms, queue_ms);
        stats_.max_queue_ms = std::max(stats_.max_queue_ms, queue_ms);
        stats_.low_queue_frames += queue_ms < 20 ? 1u : 0u;
        stats_.frames++;
        stats_.input_samples += pcm.size();

        // One emulated frame takes 1/58.98 seconds. If the host only
        // sustains 50 FPS, produce ~1.18 host samples per guest sample.
        // Keep the scheduler stable under jitter or menus.
        constexpr double guest_frame_seconds = 1.0 / 58.98;
        const double dt = std::clamp(wall_frame_seconds, 0.011, 0.050);
        double desired = std::clamp(dt / guest_frame_seconds, 0.90, 1.65);

        // Soft queue feedback: replenish a short reserve without crossing
        // 240ms latency. The feedback has limited authority so the audio
        // follows game time, rather than changing playback frequency wildly.
        const double queue_error =
            (static_cast<double>(kTargetQueueMs) - queue_ms) / kTargetQueueMs;
        desired *= std::clamp(1.0 + 0.10 * queue_error, 0.88, 1.10);
        desired = std::clamp(desired, 0.85, 1.75);
        ratio_ += (desired - ratio_) * 0.14;
        stats_.stretch = ratio_;

        input_.insert(input_.end(), pcm.begin(), pcm.end());
        if (!initialized_) {
            if (input_.size() < kWindow) return output_;
            output_.insert(output_.end(), input_.begin(), input_.begin() + kHop);
            for (unsigned i = 0; i < kHop; ++i) tail_[i] = input_[kHop + i];
            last_start_ = 0;
            initialized_ = true;
        }

        // WSOLA: input hop varies (~215 samples at 50fps), output hop
        // remains 256 samples at fixed SDL 31250Hz. The best nearby
        // waveform alignment is found by minimizing squared differences
        // against the previous overlap tail, preserving pitch far better
        // than naive per-frame resampling of the waveform.
        while (true) {
            const size_t input_hop = static_cast<size_t>(
                std::max(1.0, std::round(static_cast<double>(kHop) / ratio_)));
            const size_t expected = last_start_ + input_hop;
            const size_t first = std::max(last_start_ + 1,
                expected > kSearch ? expected - kSearch : 0u);
            if (input_.size() < kWindow + first) break;
            const size_t last = std::min(expected + kSearch,
                input_.size() - kWindow);
            if (first > last) break;

            size_t best = first;
            uint64_t best_error = UINT64_MAX;
            for (size_t pos = first; pos <= last; ++pos) {
                uint64_t error = 0;
                // Sparse cross-correlation preserves voiced wave phases.
                // Small fixed loop: no heap allocation or DSP mutation.
                for (unsigned n = 0; n < kHop; n += 4) {
                    const int32_t d =
                        static_cast<int32_t>(tail_[n]) - input_[pos + n];
                    error += static_cast<uint64_t>(d * static_cast<int64_t>(d));
                }
                if (error < best_error) { best_error = error; best = pos; }
            }

            for (unsigned n = 0; n < kHop; ++n) {
                // Linear cross-fade: amplitude never exceeds max of the
                // original two 16-bit sample values; no clipping.
                const int32_t left = static_cast<int32_t>(tail_[n]);
                const int32_t right = static_cast<int32_t>(input_[best + n]);
                const int32_t mix =
                    (left * static_cast<int32_t>(kHop - n) +
                     right * static_cast<int32_t>(n)) /
                    static_cast<int32_t>(kHop);
                output_.push_back(static_cast<int16_t>(mix));
                tail_[n] = input_[best + kHop + n];
            }
            last_start_ = best;
        }

        // Keep the last input window for future correlation and avoid
        // unbounded growth during long play sessions.
        if (last_start_ > 4096) {
            const size_t consumed = last_start_ - kWindow;
            input_.erase(input_.begin(), input_.begin() + consumed);
            last_start_ -= consumed;
        }
        stats_.output_samples += output_.size();
        return output_;
    }

    Stats take_stats() {
        const Stats result = stats_;
        stats_ = {};
        return result;
    }
    double stretch() const { return ratio_; }

private:
    std::vector<int16_t> input_;
    std::vector<int16_t> output_;
    std::array<int16_t, kHop> tail_{};
    size_t last_start_ = 0;
    double ratio_ = 1.0;
    bool initialized_ = false;
    Stats stats_{};
};

} // namespace ki_r36s
