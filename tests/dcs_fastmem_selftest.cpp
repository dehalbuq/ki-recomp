// ROM-free regression test: the fast DCS memory map must behave identically
// to the original (virtual Bus) ADSP2105 path on synthetic data.
#include "ki/dcs.h"
#include <array>
#include <cstdio>
#include <vector>

int main() {
    std::vector<std::vector<uint8_t>> roms(8, std::vector<uint8_t>(0x1000, 0));
    // No commercial assets; a zero-filled firmware page executes NOPs.
    ki::Dcs fast(roms), baseline(roms);
    fast.enable_fast_memory(true);
    baseline.enable_fast_memory(false);

    // All DCS program/data bank mirrors, writes and mixed low-byte behavior.
    const std::array<uint32_t, 12> program_addrs{
        0x0000, 0x03ff, 0x0400, 0x07ff, 0x0800, 0x0ffe,
        0x1000, 0x17ff, 0x1800, 0x1fff, 0x2000, 0x4800
    };
    for (uint32_t addr : program_addrs) {
        const uint32_t value = (addr * 0x1555u) & 0xffffffu;
        fast.program_write(addr, value);
        baseline.program_write(addr, value);
        if (fast.program_read(addr) != baseline.program_read(addr)) {
            std::fprintf(stderr, "Program map mismatch at %04x\n", addr);
            return 1;
        }
    }
    const std::array<uint32_t, 12> data_addrs{
        0x0000, 0x03ff, 0x0800, 0x17ff, 0x1fff, 0x2000,
        0x2fff, 0x3000, 0x3800, 0x39ff, 0x3a00, 0x7800
    };
    for (uint32_t addr : data_addrs) {
        if ((addr & 0x3fff) != 0x3000) { // bank switching is compared separately
            const uint16_t value = static_cast<uint16_t>(addr ^ 0x5a5au);
            fast.data_write(addr, value);
            baseline.data_write(addr, value);
        }
        if (fast.data_read(addr) != baseline.data_read(addr)) {
            std::fprintf(stderr, "Data map mismatch at %04x\n", addr);
            return 2;
        }
    }
    fast.data_write(0x3000, 5);
    baseline.data_write(0x3000, 5);
    if (fast.data_read(0x2123) != baseline.data_read(0x2123)) return 3;

    // Run exactly the same synthetic DSP program and compare register timing.
    // Re-establish a zero-filled boot program to prevent a test write from
    // deliberately introducing an unrelated instruction with side effects.
    for (uint32_t addr = 0; addr < 0x400; ++addr) {
        fast.program_write(addr, 0);
        baseline.program_write(addr, 0);
    }
    for (uint64_t target : {1000ull, 5000ull, 20000ull}) {
        fast.run_until(target);
        baseline.run_until(target);
        if (fast.cycles() != baseline.cycles() || fast.dsp_pc() != baseline.dsp_pc() ||
            fast.samples() != baseline.samples()) {
            std::fprintf(stderr, "DCS run mismatch at target %llu\n",
                         static_cast<unsigned long long>(target));
            return 4;
        }
    }
    std::puts("DCS direct memory access matches original Bus map and synthetic DSP execution");
    return 0;
}
