// ROM-free differential test for the exact KI L1.5D DSP polling loops.
// Confirms cycle accounting, instruction boundaries and register state against
// the unchanged MAME-derived ADSP interpreter at diverse slice lengths.
#include "adsp2105.h"

#include <array>
#include <cstdint>
#include <cstdio>

struct TestBus final : adsp::Bus {
    std::array<uint32_t, 0x4000> prog{};
    uint16_t data_read(uint32_t) override { return 0; }
    void data_write(uint32_t, uint16_t) override {}
    uint32_t program_read(uint32_t pc) override { return prog[pc & 0x3fffu]; }
    void program_write(uint32_t pc, uint32_t val) override { prog[pc & 0x3fffu] = val & 0xffffffu; }
    void sport_tx(int, uint32_t) override {}
    void timer_fired(bool) override {}
};

static bool same(const adsp::Adsp2105& a, const adsp::Adsp2105& b) {
    if (a.pc() != b.pc() || a.total_cycles() != b.total_cycles() ||
        a.idle() != b.idle()) return false;
    for (int i = 0; i < 8; ++i)
        if (a.ireg(i) != b.ireg(i) || a.mreg(i) != b.mreg(i) ||
            a.lreg(i) != b.lreg(i) || a.get_ibase(i) != b.get_ibase(i))
            return false;
    return true;
}

static int run_case(uint32_t head, uint32_t i7, uint32_t ay0) {
    TestBus bus;
    // A one-instruction setup copies I7 -> AY0 via the ordinary ADSP
    // internal-register move before entering either polling loop.
    bus.prog[0] = 0x0d0243u;
    bus.prog[1] = (0x18u << 16) | (head << 4) | 0x0fu;
    bus.prog[head] = 0x0d02a3u;
    bus.prog[head + 1] = 0x26e20fu;
    bus.prog[head + 2] = head == 0x130 ? 0x181305u : 0x181364u;
    adsp::Adsp2105 base(bus), fast(bus);
    base.reset();
    fast.reset();
    base.set_ireg(7, ay0);
    fast.set_ireg(7, ay0);
    base.run(1);   // AY0 <- I7 (distinct setup value)
    fast.run(1);
    base.set_ireg(7, i7);
    fast.set_ireg(7, i7);
    fast.enable_ki_waitloop_fastpath(true);

    // Include partial 1-2 instruction tails and long event slices.
    constexpr std::array<int, 20> slices = {
        1, 2, 3, 4, 5, 7, 8, 9, 10, 17, 19, 100, 101, 1024, 1025,
        4096, 4097, 8192, 10000, 50003
    };
    for (int n : slices) {
        const int a = base.run(n), b = fast.run(n);
        if (a != b || !same(base, fast)) {
            std::fprintf(stderr,
                "KI waitloop mismatch head=%04x I7=%04x slice=%d "
                "base PC=%04x cycles=%llu vs fast PC=%04x cycles=%llu\n",
                head, i7, n, base.pc(),
                static_cast<unsigned long long>(base.total_cycles()),
                fast.pc(), static_cast<unsigned long long>(fast.total_cycles()));
            return 1;
        }
    }
    // Ensure we really ran the optimized path in synthetic polling loops.
    // If the branch condition is false for this input, equal state still matters.
    if (head == 0x130 && i7 == 0 && fast.ki_waitloop_skipped_cycles() == 0) {
        std::fputs("Expected positive accelerated cycles for zero/loop130\n", stderr);
        return 2;
    }
    if (head == 0x136 && i7 == 0 && ay0 == 0x1000 &&
        fast.ki_waitloop_skipped_cycles() == 0) {
        std::fputs("Expected positive accelerated cycles for negative/loop136\n", stderr);
        return 3;
    }
    return 0;
}

int main() {
    // 0x0130 loops when AR - AY0 is nonnegative (condition 5),
    // 0x0136 loops when AR - AY0 is negative (condition 4).
    if (run_case(0x130, 0, 0)) return 1;
    if (run_case(0x130, 0x1000, 0)) return 2;
    if (run_case(0x130, 0, 0x1000)) return 3;  // untaken case
    if (run_case(0x136, 0, 0x1000)) return 4;
    if (run_case(0x136, 0, 0)) return 5;       // untaken case
    if (run_case(0x136, 0x1000, 0)) return 6;  // untaken case
    std::puts("KI DSP polling microblock: verified cycle/state equivalence and fast forwarding");
    return 0;
}
