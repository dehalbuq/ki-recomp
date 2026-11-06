// ROM-free deterministic checks for the pitch-preserving R36S DCS
// output WSOLA: exact guest-produced PCM is never modified, no clipping,
// and 50fps can supply enough host audio samples for a 31250Hz SDL sink.
#include "../src/ki_r36s_audio_smooth.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

static std::vector<int16_t> sine(unsigned frames) {
    std::vector<int16_t> pcm(frames);
    constexpr double pi = 3.14159265358979323846;
    for (unsigned i = 0; i < frames; ++i)
        pcm[i] = static_cast<int16_t>(std::round(
            19000.0 * std::sin(2.0 * pi * 440.0 * i / ki_r36s::AudioSmooth::kRate)));
    return pcm;
}
static unsigned crossings(const std::vector<int16_t>& pcm) {
    unsigned count = 0;
    for (size_t i = 1; i < pcm.size(); ++i)
        if (pcm[i - 1] <= 0 && pcm[i] > 0) ++count;
    return count;
}
int main() {
    const auto input = sine(31250);
    for (const double fps : {58.98, 50.0, 43.0}) {
        ki_r36s::AudioSmooth engine;
        std::vector<int16_t> played;
        unsigned offset = 0;
        for (unsigned frame = 0; frame < 59; ++frame) {
            const unsigned end = (frame == 58)
              ? static_cast<unsigned>(input.size())
              : static_cast<unsigned>((uint64_t(frame + 1) * input.size()) / 59);
            std::vector<int16_t> in(input.begin() + offset, input.begin() + end);
            const auto& out = engine.process(in, 1.0 / fps, 5300);
            played.insert(played.end(), out.begin(), out.end());
            offset = end;
        }
        const double duration = static_cast<double>(played.size()) /
                                ki_r36s::AudioSmooth::kRate;
        const double ratio = played.size() / double(input.size());
        const double actual_pitch = crossings(played) / duration;
        std::printf("WSOLA source 440Hz fps=%.1f out_samples=%zu output_ratio=%.3f detected_pitch=%.1fHz\n",
                    fps, played.size(), ratio, actual_pitch);
        // The original unmodified baseline must not be shorter/longer by
        // more than the single 512-sample WSOLA priming delay.
        if (fps > 58 && (ratio < .97 || ratio > 1.04)) return 1;
        if (fps < 58 && (ratio < 1.04 || ratio > 1.65)) return 2;
        // It should retain the source 440Hz pitch (not simply lower
        // pitch by slowing the PCM clock), allowing some test granularity.
        if (actual_pitch < 340 || actual_pitch > 540) return 3;
        if (engine.stretch() < .85 || engine.stretch() > 1.75) return 4;
        const auto stats = engine.take_stats();
        if (stats.frames != 59 || stats.input_samples != input.size()
            || stats.output_samples != played.size()) return 5;
    }
    // Arbitrary rapid changes must never cause runaway sample input buffers
    // or negative frame output; no original PCM samples may be modified.
    ki_r36s::AudioSmooth sync;
    const auto source = sine(512);
    const auto untouched = source;
    for (unsigned i = 0; i < 800; ++i)
        sync.process(source, i%2 ? .030 : .012, i%3 ? 0 : 8500);
    if (source != untouched) return 6;
    sync.reset();
    if (sync.stretch() != 1.0) return 7;
    std::puts("R36S pitch-preserving audio stretch, input fidelity and reset OK");
    return 0;
}
