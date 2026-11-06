// ROM-free Test20: exact MIPS indirect dispatch to same C++ function as the
// baseline binary search, cache hits, slot collisions and ROM alias resets.
#include "ki/runtime.h"
#include <cstdio>

namespace {
void hit_a(ki::Context& c) { ++c.r[2]; }
void hit_b(ki::Context& c) { c.r[2] += 10; }
void hit_c(ki::Context& c) { c.r[2] += 100; }
}

// Definitions used by the non-game runtime lookup, sorted like real translator.
namespace ki {
const FuncEntry kFunctionTable[] = {
    {0x88000100u, hit_a},
    {0x88000200u, hit_b},
    {0x88000300u, hit_c},
};
const std::size_t kFunctionTableSize = 3;
}

int main() {
    using namespace ki;
    Context c;
    for (int n=0; n<500; ++n) {
        if (lookup_function(0x88000100u) != hit_a) return 1;
        if (lookup_function(0x88000200u) != hit_b) return 2;
        if (lookup_function(0x88000300u) != hit_c) return 3;
    }
    if (lookup_function(0x88000000u) != nullptr) return 4;
    if (lookup_function(0x88000104u) != nullptr) return 5;
    call_indirect(c, 0x88000100u, 0);
    call_indirect(c, 0x88000200u, 0);
    call_indirect(c, 0x88000300u, 0);
    if (c.r[2] != 111) return 6;

    // Change the boot lookup table for exactly the same guest address.
    // Entries cached before the change MUST NOT survive. Boot also has
    // uncached address aliases (0x9fcxxxxx -> 0xbfcxxxxx).
    const FuncEntry rom_a[] = {{0xbfc00000u, hit_a}};
    const FuncEntry rom_b[] = {{0xbfc00000u, hit_b}};
    set_boot_function_table(rom_a, 1);
    for (int i=0; i<100; ++i) {
        if (lookup_function(0xbfc00000u) != hit_a) return 7;
        if (lookup_function(0x9fc00000u) != hit_a) return 8;
    }
    set_boot_function_table(rom_b, 1);
    if (lookup_function(0xbfc00000u) != hit_b) return 9;
    if (lookup_function(0x9fc00000u) != hit_b) return 10;
    set_boot_function_table(nullptr, 0);
    if (lookup_function(0xbfc00000u) || lookup_function(0x9fc00000u)) return 11;

    // Verify addresses with hash collisions: both must resolve exactly,
    // even when overwriting the same direct-mapped cache slot.
    for (uint32_t first=0x88000100u; first<=0x88000300u; first+=0x100u) {
        for (uint32_t second=0x88000100u; second<=0x88000300u; second+=0x100u) {
            const RecompFunc ref_a = first==0x88000100u ? hit_a : first==0x88000200u ? hit_b : hit_c;
            const RecompFunc ref_b = second==0x88000100u ? hit_a : second==0x88000200u ? hit_b : hit_c;
            if (lookup_function(first) != ref_a ||
                lookup_function(second) != ref_b ||
                lookup_function(first) != ref_a) return 12;
        }
    }
    const auto stats = dispatch_cache_take_stats();
    std::printf("MIPS indirect cache equivalence PASS (hits=%llu misses=%llu)\n",
        static_cast<unsigned long long>(stats.hits),
        static_cast<unsigned long long>(stats.misses));
    return 0;
}
