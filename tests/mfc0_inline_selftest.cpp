// ROM-free COP0 MFC0 differential verification: baseline vs R36S inline.
#include "ki/runtime.h"
#include <array>
#include <cstdio>
#include <cstdint>

struct FakeIo final : ki::IoHandler {
    uint32_t lines = 0;
    uint64_t read(ki::Context&, uint32_t, int) override { return 0; }
    void write(ki::Context&, uint32_t, int, uint64_t) override {}
    uint32_t interrupt_lines(ki::Context&) override { return lines; }
};

int main() {
    FakeIo io;
    ki::Context c;
    c.io = &io;
    for (unsigned r=0; r<32; ++r)
        c.cp0[r] = 0xdeadbeef00000000ull + 0x1000 * r + r;
    constexpr std::array<uint64_t, 9> times {
        0, 1, 2, 3, 999, 1000000, 0x7ffffffeULL,
        0xffffffffULL, 0x100000001ULL
    };
    constexpr std::array<uint32_t, 7> line_states {
        0, 0x400, 0x800, 0xc00, 0x7c00, 0xffffffff, 0x100
    };
    unsigned checks = 0;
    for (bool with_io : {false, true}) {
        c.io = with_io ? &io : nullptr;
        for (uint32_t lines : line_states) {
            io.lines = lines;
            for (uint64_t time : times) {
                c.cycles = time;
                c.count_base = 0x1234567800ULL ^ time;
                for (unsigned r = 0; r < 32; ++r) {
                    const uint64_t baseline = ki::mfc0(c, r, 0x80000000u);
                    c.fast_cop0 = true;
                    const uint64_t optimized = ki::mfc0_recompiled(c, r, 0x80000000u);
                    c.fast_cop0 = false;
                    const uint64_t fallback = ki::mfc0_recompiled(c, r, 0x80000000u);
                    ++checks;
                    if (baseline != optimized || baseline != fallback) {
                        std::fprintf(stderr,
                            "MFC0 mismatch: reg=%u cycles=%llu io=%d lines=%x "
                            "baseline=%llx inline=%llx fallback=%llx\n",
                            r, static_cast<unsigned long long>(time), int(with_io),
                            lines, static_cast<unsigned long long>(baseline),
                            static_cast<unsigned long long>(optimized),
                            static_cast<unsigned long long>(fallback));
                        return 1;
                    }
                }
            }
        }
    }
    std::printf("COP0 R36S inlining matches original for %u synthetic reads\n", checks);
    return 0;
}
