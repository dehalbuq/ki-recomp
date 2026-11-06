// ROM-free, adversarial MIPS memory regression: R36S direct SRAM/DRAM
// compiled path vs the original guest memory/IO/fault implementation.
// Covers KSEG0/KSEG1, low TLB page, unmapped IO, boundaries, alignment,
// all load/store widths, and every offset of LDL/LDR/LWL/LWR.
#include "ki/runtime.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <exception>
#include <random>
#include <vector>

struct StubBoard final : ki::IoHandler {
    uint64_t reads = 0, writes = 0, last_write = 0;
    uint64_t read(ki::Context&, uint32_t p, int n) override {
        ++reads;
        return 0xdecafbad12345678ull ^ (uint64_t(p) << 3) ^ uint64_t(n);
    }
    void write(ki::Context&, uint32_t p, int n, uint64_t v) override {
        ++writes;
        last_write ^= (uint64_t(p) << 3) ^ (uint64_t(n) << 32) ^ v;
    }
};

struct Result {
    uint64_t value = 0;
    bool fault = false;
    ki::Fault kind = ki::Fault::Unimplemented;
    uint32_t address = 0;
};

template<class Fn> Result evaluate(Fn&& fn) {
    Result r;
    try { r.value = fn(); }
    catch (const ki::GuestFault& f) {
        r.fault = true;
        r.kind = f.fault;
        r.address = f.addr;
    }
    return r;
}

bool same(const Result& a, const Result& b) {
    return a.fault == b.fault && (!a.fault ? a.value == b.value :
           a.kind == b.kind && a.address == b.address);
}

template<class T> bool load_test(ki::Context& old, ki::Context& fast,
                                uint32_t a, unsigned& checks) {
    auto x = evaluate([&] { return uint64_t(ki::load<T>(old,a,0x88000000u)); });
    auto y = evaluate([&] { return uint64_t(ki::load_recompiled<T>(fast,a,0x88000000u)); });
    ++checks;
    if (!same(x,y)) {
        std::fprintf(stderr,"load width=%zu at %08x mismatch old=%llx new=%llx fault=%d/%d\n",
                     sizeof(T),a,(unsigned long long)x.value,
                     (unsigned long long)y.value,x.fault,y.fault);
        return false;
    }
    return true;
}

template<class T> bool store_test(ki::Context& old, ki::Context& fast,
                                 uint32_t a,T value,unsigned& checks) {
    auto x = evaluate([&] { ki::store<T>(old,a,value,0x88000000u);return 0ull; });
    auto y = evaluate([&] { ki::store_recompiled<T>(fast,a,value,0x88000000u);return 0ull; });
    ++checks;
    if (!same(x,y)) {
        std::fprintf(stderr,"store width=%zu at %08x mismatch fault=%d/%d\n",
                     sizeof(T),a,x.fault,y.fault);
        return false;
    }
    return true;
}

int main() {
    ki::Context old, fast;
    StubBoard oa, fa;
    old.io=&oa;
    fast.io=&fa;
    fast.fast_mips_memory=true;
    std::mt19937_64 rng(0x3615cafeull);
    for (size_t i=0;i<old.sram.size();++i)
        old.sram[i] = fast.sram[i] = uint8_t(rng());
    for (size_t i=0;i<old.dram.size();++i)
        old.dram[i] = fast.dram[i] = uint8_t(rng());

    constexpr std::array<uint32_t,31> anchor{
        0x0u,0x1u,0x3u,0x7u,0x0ff8u,0x0ffcu,0x0fffu,0x1000u,
        0x80000000u,0x80000003u,0x80000007u,0x8007fff0u,
        0x8007fff8u,0x8007fffeu,0x8007ffffu,0x80080000u,
        0x88000000u,0x88000001u,0x88000007u,0x887ffff0u,
        0x887ffff8u,0x887fffffu,0x88800000u,0xa8000000u,
        0xa87ffff8u,0xa8800000u,0x90000080u,0xb0000080u,
        0xc0000000u,0xffffffffu,0x7ffffffeu
    };
    unsigned checks=0;
    for (uint32_t base:anchor) for (int shift=-2;shift<=9;++shift) {
        uint32_t a=base+uint32_t(shift);
        if (!load_test<uint8_t>(old,fast,a,checks) ||
            !load_test<uint16_t>(old,fast,a,checks) ||
            !load_test<uint32_t>(old,fast,a,checks) ||
            !load_test<uint64_t>(old,fast,a,checks)) return 1;
        if (!store_test<uint8_t>(old,fast,a,0xdau,checks) ||
            !store_test<uint16_t>(old,fast,a,0xbeefu,checks) ||
            !store_test<uint32_t>(old,fast,a,0x12345678u,checks) ||
            !store_test<uint64_t>(old,fast,a,0xdac0face12345678ull,checks)) return 2;
        const uint64_t src=rng();
        const Result ld_a=evaluate([&] { return ki::ldl(old,src,a,0x88000000u); });
        const Result ld_b=evaluate([&] { return ki::ldl_recompiled(fast,src,a,0x88000000u); });
        const Result dr_a=evaluate([&] { return ki::ldr(old,src,a,0x88000000u); });
        const Result dr_b=evaluate([&] { return ki::ldr_recompiled(fast,src,a,0x88000000u); });
        const Result wl_a=evaluate([&] { return ki::lwl(old,src,a,0x88000000u); });
        const Result wl_b=evaluate([&] { return ki::lwl_recompiled(fast,src,a,0x88000000u); });
        const Result wr_a=evaluate([&] { return ki::lwr(old,src,a,0x88000000u); });
        const Result wr_b=evaluate([&] { return ki::lwr_recompiled(fast,src,a,0x88000000u); });
        checks+=4;
        if (!same(ld_a,ld_b)||!same(dr_a,dr_b)||!same(wl_a,wl_b)||!same(wr_a,wr_b)) {
            std::fprintf(stderr,"unaligned at %08x mismatch\n",a);return 3;
        }
    }
    if (old.sram != fast.sram || old.dram != fast.dram ||
        oa.reads != fa.reads || oa.writes != fa.writes ||
        oa.last_write != fa.last_write) {
        std::fprintf(stderr,"R36S fast memory side effect mismatch RAM/IO reads=%llu/%llu writes=%llu/%llu\n",
          (unsigned long long)oa.reads,(unsigned long long)fa.reads,
          (unsigned long long)oa.writes,(unsigned long long)fa.writes);
        return 4;
    }
    std::printf("MIPS inlined memory: %u loads/stores incl LDL/LDR + IO and faults match baseline\n",checks);
    return 0;
}
