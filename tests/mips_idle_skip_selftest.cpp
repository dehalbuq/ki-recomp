// ROM-free differential test: simulate the exact five-op MIPS Cause poll
// loop and compare baseline execution with bounded event-driven fast-forward.
// This does NOT approximate MIPS time or skip events; counters and cycles match.
#include "ki/runtime.h"
#include "ki/kinst_board.h"
#include <array>
#include <cstdint>
#include <cstdio>

struct SyntheticBoard final : ki::IoHandler {
    uint64_t falling_or_rising = 100;
    uint64_t read(ki::Context&, uint32_t, int) override { return 0; }
    void write(ki::Context&, uint32_t, int, uint64_t) override {}
    uint32_t interrupt_lines(ki::Context& c) override {
        return c.cycles >= falling_or_rising ? 0x400 : 0;
    }
    uint64_t next_event(ki::Context&) override { return 200; }
    uint64_t irq_edge(ki::Context& c) override {
        return c.cycles < falling_or_rising ? falling_or_rising : 200;
    }
};
struct Observed {
    uint64_t cycles;
    uint64_t counter;
    uint64_t missed;
    uint64_t iterations;
};
static Observed run(bool fast, uint64_t irq_cycle, int inc) {
    ki::Context c;
    SyntheticBoard io;
    io.falling_or_rising = irq_cycle;
    c.io = &io;
    c.fast_mips_idle = fast;
    c.next_event = 200;
    c.r[9] = ki::sx32(0xfffffff0u);
    for (int i=0; i<1000; ++i) {
        // MFC0 Cause; NOP; ANDI $t0,$t0,0x400;
        const uint32_t masked = io.interrupt_lines(c) & 0x400;
        c.r[8] = ki::sx32(masked);
        // BEQ $t0,$zero,-4; ADDIU $t1,$t1,inc.
        c.r[9] = ki::sx32(ki::lo32(c.r[9]) +
                          static_cast<uint32_t>(inc));
        c.cycles += 6;
        if (masked != 0) break;
        ki::poll_irq_idle(c, 0x8800200c, 0x400, 8, 9, inc, 6);
    }
    return {c.cycles, c.r[9], c.mips_idle_skipped_cycles,
            c.mips_idle_skipped_iterations};
}
int main() {
    for (auto edge : {7u, 8u, 9u, 96u, 97u, 98u, 99u, 100u, 101u,
                       102u, 103u, 104u, 105u, 198u, 199u}) {
        for (int delta : {-17, -1, 0, 1, 7, 32767}) {
            const auto a = run(false, edge, delta);
            const auto b = run(true, edge, delta);
            if (a.cycles != b.cycles || a.counter != b.counter) {
                std::fprintf(stderr,
                    "MIPS idle mismatch IRQ=%u delta=%d baseline=%llu/%llx "
                    "fast=%llu/%llx fast_skip=%llu\n", edge, delta,
                    (unsigned long long)a.cycles, (unsigned long long)a.counter,
                    (unsigned long long)b.cycles, (unsigned long long)b.counter,
                    (unsigned long long)b.missed);
                return 1;
            }
        }
    }
    // The main KI board does not schedule the VBlank FALLING edge by
    // default. The idle-specific deadline must expose it accurately.
    constexpr uint64_t F = ki::KinstBoard::kCyclesPerFrame;
    constexpr uint64_t V = ki::KinstBoard::kVblankStart;
    ki::KinstBoard board({}, {});
    ki::Context c;
    board.set_cycles_to_vblank(100);
    c.cycles = 100;
    board.update(c);  // rising VBlank, schedules next rising event
    c.cycles = 101;
    const uint64_t expected = 100 + (F - V);
    if (board.irq_edge(c) != expected || board.next_event(c) <= expected) {
        std::fprintf(stderr, "VBlank falling edge expected %llu got %llu\n",
                     (unsigned long long)expected,
                     (unsigned long long)board.irq_edge(c));
        return 2;
    }
    std::puts("MIPS event-driven idle: cycles, counters and VBlank falling edge OK");
    return 0;
}
