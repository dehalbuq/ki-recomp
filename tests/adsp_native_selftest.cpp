// ROM-free differential test for the KI ADSP native opcode compiler.
// Tests registers/flags/stacks/cycles, memory side effects and instruction
// RAM modification fallback. No original game assets are needed.
#include "adsp2105.h"

#include <array>
#include <cstdio>
#include <cstdint>
#include <vector>

struct TestBus final : adsp::Bus {
    std::array<uint32_t, 0x4000> prog{};
    std::array<uint16_t, 0x4000> data{};
    std::vector<uint32_t> serial;
    unsigned timer_events = 0;

    uint16_t data_read(uint32_t addr) override { return data[addr & 0x3fffu]; }
    void data_write(uint32_t addr, uint16_t v) override { data[addr & 0x3fffu] = v; }
    uint32_t program_read(uint32_t addr) override { return prog[addr & 0x3fffu]; }
    void program_write(uint32_t addr, uint32_t v) override { prog[addr & 0x3fffu] = v & 0xffffffu; }
    void sport_tx(int, uint32_t v) override { serial.push_back(v); }
    void timer_fired(bool) override { ++timer_events; }

    void init() {
        prog[0] = 0x18400fu; // unconditional jump into synthetic test program
        prog[0x400] = 0x40012au;
        prog[0x401] = 0x0d02a3u;
        prog[0x402] = 0x26e20fu;
        prog[0x403] = 0;          // uncompiled NOP
        prog[0x404] = 0x340001u;
        prog[0x405] = 0x440016u;
        prog[0x406] = 0x22000fu;
        prog[0x407] = 0x24000fu;
        prog[0x408] = 0x61000fu;
        prog[0x409] = 0x80005au;
        prog[0x40a] = 0x90005au;
        prog[0x40b] = 0x1840cfu; // jump to 0x40c
        prog[0x40c] = 0x300001u;
        prog[0x40d] = 0x18400fu; // repeat from 0x400
        for (unsigned i = 0; i < data.size(); ++i)
            data[i] = static_cast<uint16_t>((i * 7919u) ^ (i >> 3));
    }
};

int main() {
    TestBus normal_bus, compiled_bus;
    normal_bus.init();
    compiled_bus.init();
    adsp::Adsp2105 normal(normal_bus), compiled(compiled_bus);
    normal.reset();
    compiled.reset();
    compiled.enable_native_opcodes(true);

    const std::array<int, 25> budgets = {
        1, 2, 3, 4, 5, 7, 8, 9, 13, 17, 19, 31, 127, 128,
        129, 255, 256, 257, 1024, 1025, 2048, 8192, 10000, 20481, 50007
    };
    uint64_t instructions = 0;
    for (unsigned pass = 0; pass < 3; ++pass) {
        // Code mutation after warm-up invalidates ONLY that specialization.
        // Both machines must execute the changed opcode via the interpreter.
        if (pass == 1) {
            normal_bus.prog[0x401] = 0x0d02a2u;
            compiled_bus.prog[0x401] = 0x0d02a2u;
        }
        // Restore the old word and verify that the guarded native path resumes.
        if (pass == 2) {
            normal_bus.prog[0x401] = 0x0d02a3u;
            compiled_bus.prog[0x401] = 0x0d02a3u;
        }
        for (int n : budgets) {
            const int a = normal.run(n), b = compiled.run(n);
            instructions += static_cast<uint64_t>(a);
            if (a != b || !normal.guest_state_equals(compiled) ||
                normal_bus.data != compiled_bus.data ||
                normal_bus.prog != compiled_bus.prog ||
                normal_bus.serial != compiled_bus.serial ||
                normal_bus.timer_events != compiled_bus.timer_events) {
                std::fprintf(stderr,
                    "ADSP native mismatch pass=%u budget=%d PC=%04x/%04x cycle=%llu/%llu\n",
                    pass, n, normal.pc(), compiled.pc(),
                    static_cast<unsigned long long>(normal.total_cycles()),
                    static_cast<unsigned long long>(compiled.total_cycles()));
                return 1;
            }
        }
    }
    if (compiled.native_opcode_count() < 1000) {
        std::fprintf(stderr, "Expected native code to execute, got %llu hits\n",
                     static_cast<unsigned long long>(compiled.native_opcode_count()));
        return 2;
    }
    std::printf("ADSP native compiled-vs-interpreted equivalence: %llu guest cycles, "
                "%llu native opcodes, program-memory invalidation OK\n",
                static_cast<unsigned long long>(instructions),
                static_cast<unsigned long long>(compiled.native_opcode_count()));
    return 0;
}
