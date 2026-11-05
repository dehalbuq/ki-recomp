// ROM-free DSP execution sampling regression (Cortex-A35 and x86).
#include "adsp2105.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

struct NopBus final : adsp::Bus {
    uint16_t data_read(uint32_t) override { return 0; }
    void data_write(uint32_t, uint16_t) override {}
    uint32_t program_read(uint32_t) override { return 0; }
    void program_write(uint32_t, uint32_t) override {}
    void sport_tx(int, uint32_t) override {}
    void timer_fired(bool) override {}
};

int main() {
    NopBus bus;
    adsp::Adsp2105 cpu(bus);
    cpu.reset();
    cpu.enable_pc_profiling(true);
    const int executed = cpu.run(8192);  // NOPs; exactly 8 samples
    if (executed != 8192 || cpu.total_cycles() != 8192) return 1;
    uint32_t sampled = 0;
    auto first = cpu.take_pc_hotspots(12, sampled);
    if (sampled != 8 || first.size() != 8) {
        std::fprintf(stderr, "Expected 8 PC samples, got %u over %zu PCs\n",
                     sampled, first.size());
        return 2;
    }
    uint32_t first_total = 0;
    for (const auto& [pc, count] : first) {
        if (pc >= 8192 || count != 1) return 3;
        first_total += count;
    }
    if (first_total != 8) return 4;
    // Reporting should clear only the profile, not the guest state.
    auto second = cpu.take_pc_hotspots(12, sampled);
    if (!second.empty() || sampled != 0 || cpu.total_cycles() != 8192) return 5;
    cpu.enable_pc_profiling(false);
    if (cpu.run(1024) != 1024 || cpu.total_cycles() != 9216) return 6;
    auto third = cpu.take_pc_hotspots(12, sampled);
    if (!third.empty() || sampled != 0) return 7;
    std::puts("DSP PC sampler: 1/1024, clear-on-read, no guest timing change");
    return 0;
}
