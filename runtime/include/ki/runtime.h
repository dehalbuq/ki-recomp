// Guest runtime for statically recompiled Killer Instinct (R4600, little-endian) code.
//
// Model (see docs/TECHNICAL_NOTE.md for the evidence behind each choice):
// - 32 GPRs + HI/LO are 64-bit; 32-bit operations sign-extend their result.
// - Effective addresses are 32-bit (the game runs in 32-bit kernel mode).
// - KSEG0/KSEG1 map to physical by masking with 0x1fffffff. Of the TLB-mapped
//   segments only the single page the game maps at boot is modelled (VA
//   0x0000-0x0fff -> PA 0x08090000); other mapped addresses raise Fault::Tlb.
// - Physical map from MAME src/mame/rare/kinst.cpp (kinst_map):
//     0x00000000-0x0007ffff  SRAM 512 KiB (also holds the two framebuffers)
//     0x08000000-0x087fffff  DRAM 8 MiB (game code decompressed here)
//     0x10000080-0x1000017f  I/O (inputs, DIP, sound latch, IDE)
//     0x1fc00000-0x1fc7ffff  boot EPROM
//   Unmapped reads return all ones (map.unmap_value_high()).
// - Guest memory is stored in guest byte order (little-endian). All supported
//   hosts are little-endian too, so accesses are plain memcpy.
#pragma once

#include <bit>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>

static_assert(std::endian::native == std::endian::little,
              "the runtime stores guest (little-endian) memory in host order");

namespace ki {

constexpr uint32_t kSramBase = 0x00000000, kSramSize = 0x00080000;
constexpr uint32_t kDramBase = 0x08000000, kDramSize = 0x00800000;
constexpr uint32_t kZeroPagePhys = 0x08090000, kZeroPageSize = 0x1000;

struct Context;

// The board around the CPU: I/O space (physical addresses outside SRAM/DRAM;
// size is 1, 2, 4 or 8) and the external interrupt lines.
struct IoHandler {
    virtual ~IoHandler() = default;
    virtual uint64_t read(Context& ctx, uint32_t phys, int size) = 0;
    virtual void write(Context& ctx, uint32_t phys, int size, uint64_t value) = 0;
    // Cause.IP bits (mask 0x7c00 for Int0..Int4) driven by the board at ctx.cycles.
    virtual uint32_t interrupt_lines(Context&) { return 0; }
    // Cycle count at which the board next needs attention (line changes, frames).
    virtual uint64_t next_event(Context&) { return ~0ull; }
    // Earliest observable change to an IRQ line, including end of VBlank.
    // Only consulted by the experimental MIPS busy-loop accelerator.
    virtual uint64_t irq_edge(Context& c) { return next_event(c); }
    // Called from safe points once ctx.cycles reaches next_event().
    virtual void update(Context&) {}
};

enum class Fault { AddressErrorLoad, AddressErrorStore, Overflow, Tlb, Unimplemented, Syscall, Break, Trap };

struct Context {
    uint64_t r[32] = {};
    uint64_t hi = 0, lo = 0;
    // COP1 with Status.FR = 1 (set by the game at boot): 32 64-bit FPRs. Only
    // single precision is used by the game; a single lives in the low 32 bits
    // and writes leave the high half untouched (as MAME's DRC does).
    uint64_t f[32] = {};
    uint32_t fcsr = 0;  // FCR31: rounding mode in bits 0-1, condition bit 23
    // COP0 registers (Count and Cause are derived on read, see mfc0).
    uint64_t cp0[32] = {};
    // Guest time: one cycle per executed instruction (added by generated code).
    uint64_t cycles = 0;
    uint64_t count_base = 0;           // Count = count_base + cycles / 2
    uint32_t last_count = 0;           // Count at the last timer check
    uint64_t next_event = ~0ull;       // poll() calls service() when cycles reach it
    // Host call depth of recompiled functions, with the most recent entries
    // (diagnostics for runaway recursion, e.g. tail-call chains in a loop).
    uint64_t interrupts_taken = 0;
    uint32_t depth = 0;
    // Pending tail call: set by a function that ends by jumping to another
    // function, run by the trampoline (drain) at the nearest real call site.
    void (*tail_fn)(Context&) = nullptr;
    uint32_t recent[64] = {};
    uint32_t recent_pos = 0;
    // Optional log of every function entry (diagnostics): appended while non-null.
    std::vector<uint32_t>* call_log = nullptr;
    // Optional per-instruction hook, called by code generated with --trace.
    void (*trace_fn)(Context&, uint32_t pc) = nullptr;
    // Optional hook when a given function is entered (diagnostics).
    uint32_t watch_entry = 0, watch_entry2 = 0;  // on_watch runs at the entry of either
    void (*on_watch)(Context&, uint32_t entry) = nullptr;
    uint32_t recent_service_pc[64] = {};  // safe points where service() ran
    uint32_t recent_service_pos = 0;
    std::vector<uint8_t> sram = std::vector<uint8_t>(kSramSize);
    std::vector<uint8_t> dram = std::vector<uint8_t>(kDramSize);
    IoHandler* io = nullptr;
    // R36S-only opt-in fast COP0 reads. The original mfc0() remains available
    // for side-by-side hardware comparison and every other platform.
    bool fast_cop0 = false;
    bool fast_mips_idle = false;
    // Guest memory fast path uses proved physical KSEG/zero-page RAM ranges.
    // Kept switchable for on-device A/B performance and regression checks.
    bool fast_mips_memory = false;
    uint64_t mips_idle_skipped_cycles = 0;
    uint64_t mips_idle_skipped_iterations = 0;
};

// Raised for guest exceptions. Exception delivery to the guest's own vector is
// not modelled yet; the default implementation throws ki::GuestFault.
[[noreturn]] void raise_fault(Context& ctx, Fault fault, uint32_t pc, uint32_t addr = 0);
// Calls through a computed address (jalr, jr to non-ra, jal to an
// untranslated function). Looks the address up in the function table.
void call_indirect(Context& ctx, uint32_t target, uint32_t pc);

struct GuestFault {
    Fault fault;
    uint32_t pc;
    uint32_t addr;
};

using RecompFunc = void (*)(Context&);
struct FuncEntry {
    uint32_t vaddr;
    RecompFunc fn;
};
// Defined by generated code.
extern const FuncEntry kFunctionTable[];
extern const std::size_t kFunctionTableSize;
// The boot EPROM's code (generated separately, --table-symbol kBootFunctionTable).
// An executable that starts from the reset vector installs it with
// set_boot_function_table(); lookups also accept its cached alias 0x9fcxxxxx.
extern const FuncEntry kBootFunctionTable[];
extern const std::size_t kBootFunctionTableSize;
void set_boot_function_table(const FuncEntry* table, std::size_t size);
RecompFunc lookup_function(uint32_t vaddr);
// Test20 ROM-free counter checks; reported only with
// KI_R36S_DISPATCH_PROFILE=1. Does not mutate emulator state.
struct DispatchStats { uint64_t hits = 0, misses = 0; };
DispatchStats dispatch_cache_take_stats();

inline void tail(Context& c, RecompFunc fn) { c.tail_fn = fn; }
void tail_indirect(Context& c, uint32_t target, uint32_t pc);
// Trampoline: runs pending tail calls until a function returns normally.
inline void drain(Context& c) {
    while (c.tail_fn) {
        RecompFunc fn = c.tail_fn;
        c.tail_fn = nullptr;
        fn(c);
    }
}

// ---- arithmetic helpers (no undefined behaviour) ---------------------------

inline uint64_t sx32(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
inline uint64_t sx16(uint16_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(v))); }
inline uint64_t sx8(uint8_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(v))); }
inline uint32_t lo32(uint64_t v) { return static_cast<uint32_t>(v); }
inline int64_t s64(uint64_t v) { return static_cast<int64_t>(v); }

// Arithmetic right shifts: C++20 defines >> on negative signed values as arithmetic.
inline uint64_t sra32(uint64_t v, unsigned sa) { return sx32(static_cast<uint32_t>(static_cast<int32_t>(lo32(v)) >> (sa & 31))); }
inline uint64_t dsra(uint64_t v, unsigned sa) { return static_cast<uint64_t>(s64(v) >> (sa & 63)); }

inline bool add32_overflows(uint32_t a, uint32_t b) {
    uint32_t s = a + b;
    return ((~(a ^ b) & (a ^ s)) >> 31) != 0;
}
inline bool sub32_overflows(uint32_t a, uint32_t b) {
    uint32_t d = a - b;
    return (((a ^ b) & (a ^ d)) >> 31) != 0;
}
inline bool add64_overflows(uint64_t a, uint64_t b) {
    uint64_t s = a + b;
    return ((~(a ^ b) & (a ^ s)) >> 63) != 0;
}
inline bool sub64_overflows(uint64_t a, uint64_t b) {
    uint64_t d = a - b;
    return (((a ^ b) & (a ^ d)) >> 63) != 0;
}

// MULT/MULTU/DIV/DIVU operate on the low 32 bits; HI/LO are sign-extended.
// Division by zero leaves HI/LO unchanged and INT_MIN/-1 wraps, matching MAME
// src/devices/cpu/mips/mips3.cpp (architecturally the result is undefined).
inline void mult(Context& c, uint64_t a, uint64_t b) {
    int64_t p = static_cast<int64_t>(static_cast<int32_t>(lo32(a))) * static_cast<int32_t>(lo32(b));
    c.lo = sx32(static_cast<uint32_t>(p));
    c.hi = sx32(static_cast<uint32_t>(static_cast<uint64_t>(p) >> 32));
}
inline void multu(Context& c, uint64_t a, uint64_t b) {
    uint64_t p = static_cast<uint64_t>(lo32(a)) * lo32(b);
    c.lo = sx32(static_cast<uint32_t>(p));
    c.hi = sx32(static_cast<uint32_t>(p >> 32));
}
inline void div32(Context& c, uint64_t a, uint64_t b) {
    int32_t n = static_cast<int32_t>(lo32(a)), d = static_cast<int32_t>(lo32(b));
    if (lo32(a) == 0x80000000u && lo32(b) == 0xffffffffu) { c.lo = sx32(0x80000000u); c.hi = 0; }
    else if (d != 0) { c.lo = sx32(static_cast<uint32_t>(n / d)); c.hi = sx32(static_cast<uint32_t>(n % d)); }
}
inline void divu32(Context& c, uint64_t a, uint64_t b) {
    uint32_t n = lo32(a), d = lo32(b);
    if (d != 0) { c.lo = sx32(n / d); c.hi = sx32(n % d); }
}
void dmult(Context& c, uint64_t a, uint64_t b);
void dmultu(Context& c, uint64_t a, uint64_t b);
inline void ddiv(Context& c, uint64_t a, uint64_t b) {
    if (a == 0x8000000000000000ull && b == ~0ull) { c.lo = a; c.hi = 0; }
    else if (b != 0) { c.lo = static_cast<uint64_t>(s64(a) / s64(b)); c.hi = static_cast<uint64_t>(s64(a) % s64(b)); }
}
inline void ddivu(Context& c, uint64_t a, uint64_t b) {
    if (b != 0) { c.lo = a / b; c.hi = a % b; }
}

// ---- memory ----------------------------------------------------------------

inline uint32_t ea(uint64_t base, int32_t offset) { return lo32(base + static_cast<uint64_t>(static_cast<int64_t>(offset))); }

#ifdef KI_MEM_PROBE
// Debug builds only (-DKI_MEM_PROBE): called for every RAM access.
inline void (*g_mem_probe)(Context& c, uint32_t vaddr, int size, uint32_t pc, bool store) = nullptr;
#endif

// Returns a host pointer for a RAM access, or nullptr for I/O / unmapped.
inline uint8_t* ram_ptr(Context& c, uint32_t vaddr, int size, uint32_t pc, bool store) {
#ifdef KI_MEM_PROBE
    if (g_mem_probe) g_mem_probe(c, vaddr, size, pc, store);
#endif
    if ((vaddr & static_cast<uint32_t>(size - 1)) != 0)
        raise_fault(c, store ? Fault::AddressErrorStore : Fault::AddressErrorLoad, pc, vaddr);
    uint32_t phys;
    if (vaddr - 0x80000000u < 0x40000000u) {
        phys = vaddr & 0x1fffffffu;  // KSEG0/KSEG1
    } else if (vaddr < kZeroPageSize) {
        // The only TLB mapping the game creates (0x8800b2e0, called once at
        // boot): VA 0x0000-0x0fff -> PA 0x08090000, used for $zero-based
        // globals. Verified in MAME after boot; every other mapped address
        // misses, which the game's exception handler treats as fatal.
        phys = kZeroPagePhys + vaddr;
    } else {
        raise_fault(c, Fault::Tlb, pc, vaddr);
    }
    if (phys - kSramBase < kSramSize) return c.sram.data() + (phys - kSramBase);
    if (phys - kDramBase < kDramSize) return c.dram.data() + (phys - kDramBase);
    return nullptr;
}

uint64_t io_read(Context& c, uint32_t vaddr, int size);
void io_write(Context& c, uint32_t vaddr, int size, uint64_t value);

template <typename T>
inline T load(Context& c, uint32_t vaddr, uint32_t pc) {
    if (uint8_t* p = ram_ptr(c, vaddr, sizeof(T), pc, false)) {
        T v;
        std::memcpy(&v, p, sizeof(T));
        return v;
    }
    return static_cast<T>(io_read(c, vaddr, sizeof(T)));
}

template <typename T>
inline void store(Context& c, uint32_t vaddr, T value, uint32_t pc) {
    if (uint8_t* p = ram_ptr(c, vaddr, sizeof(T), pc, true))
        std::memcpy(p, &value, sizeof(T));
    else
        io_write(c, vaddr, sizeof(T), value);
}

// R36S native recompiler memory fast-path. The existing load/store() above
// are the reference semantics for all platforms. This specializes the common
// SRAM, DRAM and zero-page RAM accesses into a constant-size memcpy with no
// dynamic segment translation, virtual I/O call or guest exception path.
// Unaligned or invalid accesses ALWAYS call the reference implementation.
// KI_MEM_PROBE diagnostics remain fully observable by bypassing this path.
template <typename T>
inline T load_recompiled(Context& c, uint32_t vaddr, uint32_t pc) {
#ifdef KI_R36S_FAST_MEMORY
    if (c.fast_mips_memory) {
#ifdef KI_MEM_PROBE
        if (!g_mem_probe)
#endif
        {
            if ((vaddr & (sizeof(T) - 1)) == 0) {
                const uint32_t segment = vaddr & 0xe0000000u;
                if (segment == 0x80000000u || segment == 0xa0000000u) {
                    const uint32_t phys = vaddr & 0x1fffffffu;
                    if (phys <= kSramSize - sizeof(T)) {
                        T v;
                        std::memcpy(&v, c.sram.data() + phys, sizeof(T));
                        return v;
                    }
                    const uint32_t dram_offset = phys - kDramBase;
                    if (dram_offset <= kDramSize - sizeof(T)) {
                        T v;
                        std::memcpy(&v, c.dram.data() + dram_offset, sizeof(T));
                        return v;
                    }
                } else if (vaddr <= kZeroPageSize - sizeof(T)) {
                    T v;
                    std::memcpy(&v, c.dram.data() + kZeroPagePhys - kDramBase + vaddr, sizeof(T));
                    return v;
                }
            }
        }
    }
#endif
    return load<T>(c, vaddr, pc);
}

template <typename T>
inline void store_recompiled(Context& c, uint32_t vaddr, T value, uint32_t pc) {
#ifdef KI_R36S_FAST_MEMORY
    if (c.fast_mips_memory) {
#ifdef KI_MEM_PROBE
        if (!g_mem_probe)
#endif
        {
            if ((vaddr & (sizeof(T) - 1)) == 0) {
                const uint32_t segment = vaddr & 0xe0000000u;
                if (segment == 0x80000000u || segment == 0xa0000000u) {
                    const uint32_t phys = vaddr & 0x1fffffffu;
                    if (phys <= kSramSize - sizeof(T)) {
                        std::memcpy(c.sram.data() + phys, &value, sizeof(T));
                        return;
                    }
                    const uint32_t dram_offset = phys - kDramBase;
                    if (dram_offset <= kDramSize - sizeof(T)) {
                        std::memcpy(c.dram.data() + dram_offset, &value, sizeof(T));
                        return;
                    }
                } else if (vaddr <= kZeroPageSize - sizeof(T)) {
                    std::memcpy(c.dram.data() + kZeroPagePhys - kDramBase + vaddr, &value, sizeof(T));
                    return;
                }
            }
        }
    }
#endif
    store<T>(c, vaddr, value, pc);
}

// Unaligned word/doubleword halves, little-endian semantics (MIPS III manual,
// LWL/LWR/LDL/LDR/SWL/SWR/SDL/SDR with BigEndianCPU = 0).
uint64_t lwl(Context& c, uint64_t rt, uint32_t vaddr, uint32_t pc);
uint64_t lwr(Context& c, uint64_t rt, uint32_t vaddr, uint32_t pc);
uint64_t ldl(Context& c, uint64_t rt, uint32_t vaddr, uint32_t pc);
uint64_t ldr(Context& c, uint64_t rt, uint32_t vaddr, uint32_t pc);
void swl(Context& c, uint64_t rt, uint32_t vaddr, uint32_t pc);
void swr(Context& c, uint64_t rt, uint32_t vaddr, uint32_t pc);
void sdl(Context& c, uint64_t rt, uint32_t vaddr, uint32_t pc);
void sdr(Context& c, uint64_t rt, uint32_t vaddr, uint32_t pc);

// The regular MIPS unaligned helpers are out-of-line and therefore expensive
// on Cortex-A35. Inline their exact little-endian half-access semantics in
// generated code while retaining original functions as the baseline.
inline uint64_t lwl_recompiled(Context& c, uint64_t rt, uint32_t a, uint32_t pc) {
#ifdef KI_R36S_FAST_MEMORY
    if (c.fast_mips_memory) {
        const unsigned sh = 8u * (3u - (a & 3u));
        const uint32_t word = load_recompiled<uint32_t>(c, a & ~3u, pc);
        const uint32_t mask = 0xffffffffu << sh;
        return sx32((lo32(rt) & ~mask) | (word << sh));
    }
#endif
    return lwl(c, rt, a, pc);
}
inline uint64_t lwr_recompiled(Context& c, uint64_t rt, uint32_t a, uint32_t pc) {
#ifdef KI_R36S_FAST_MEMORY
    if (c.fast_mips_memory) {
        const unsigned sh = 8u * (a & 3u);
        const uint32_t word = load_recompiled<uint32_t>(c, a & ~3u, pc);
        const uint32_t mask = 0xffffffffu >> sh;
        return sx32((lo32(rt) & ~mask) | (word >> sh));
    }
#endif
    return lwr(c, rt, a, pc);
}
inline uint64_t ldl_recompiled(Context& c, uint64_t rt, uint32_t a, uint32_t pc) {
#ifdef KI_R36S_FAST_MEMORY
    if (c.fast_mips_memory) {
        const unsigned k = a & 7u;
        const uint64_t word = k <= 3
          ? load_recompiled<uint32_t>(c, a & ~7u, pc)
          : load_recompiled<uint64_t>(c, a & ~7u, pc);
        const unsigned sh = 8u * (7u - k);
        return (rt & ~(~0ull << sh)) | (word << sh);
    }
#endif
    return ldl(c, rt, a, pc);
}
inline uint64_t ldr_recompiled(Context& c, uint64_t rt, uint32_t a, uint32_t pc) {
#ifdef KI_R36S_FAST_MEMORY
    if (c.fast_mips_memory) {
        const unsigned k = a & 7u;
        const uint64_t word = k >= 4
          ? static_cast<uint64_t>(load_recompiled<uint32_t>(c, (a & ~7u) + 4u, pc)) << 32
          : load_recompiled<uint64_t>(c, a & ~7u, pc);
        const unsigned sh = 8u * k;
        return (rt & ~(~0ull >> sh)) | (word >> sh);
    }
#endif
    return ldr(c, rt, a, pc);
}

// ---- coprocessor 1 (single precision) --------------------------------------
// Host float arithmetic is IEEE-754 binary32 with no excess precision on the
// supported targets (FLT_EVAL_METHOD == 0 on arm64 and x86-64 SSE). Do not
// build with -ffast-math. NaN payloads may differ between host ISAs.

inline uint32_t fbits(const Context& c, unsigned n) { return lo32(c.f[n]); }
inline void set_fbits(Context& c, unsigned n, uint32_t v) { c.f[n] = (c.f[n] & 0xffffffff00000000ull) | v; }
inline float fs(const Context& c, unsigned n) {
    uint32_t b = fbits(c, n);
    float v;
    std::memcpy(&v, &b, sizeof v);
    return v;
}
inline void set_fs(Context& c, unsigned n, float v) {
    uint32_t b;
    std::memcpy(&b, &v, sizeof b);
    set_fbits(c, n, b);
}
inline bool fcc(const Context& c) { return (c.fcsr >> 23) & 1; }
inline void set_fcc(Context& c, bool v) { c.fcsr = (c.fcsr & ~(1u << 23)) | (static_cast<uint32_t>(v) << 23); }
// CVT.W.S rounds according to FCSR.RM, like the hardware and MAME's DRC
// (MAME's interpreter truncates instead). NaN and out-of-range inputs, which
// trap on the R4600, saturate here (as arm64 fcvt* does); see the note.
int32_t cvt_w_s(const Context& c, float v);
// C.cond.S: cond is the low 4 bits of the function field (signalling variants
// 8-15 compare like 0-7; FP exceptions are not modelled).
bool fcompare(unsigned cond, float a, float b);
uint64_t cfc1(const Context& c, unsigned reg);
void ctc1(Context& c, unsigned reg, uint64_t value);

// ---- coprocessor 0 / system (not needed by the first proof; trap loudly) ---
// COP0 register numbers used by the game.
enum : unsigned { kCp0Index = 0, kCp0EntryLo0 = 2, kCp0EntryLo1 = 3, kCp0PageMask = 5, kCp0Wired = 6,
                  kCp0BadVAddr = 8, kCp0Count = 9, kCp0EntryHi = 10, kCp0Compare = 11, kCp0Status = 12,
                  kCp0Cause = 13, kCp0Epc = 14, kCp0PRId = 15, kCp0Config = 16 };
// Exception vector handler the game installs at 0x80000180 jumps here (SRAM
// stub: sd sp,0x1f0($zero); sp = 0x880266c8; jr sp).
constexpr uint32_t kExceptionHandler = 0x880266c8;

#define KI_TRACE(c, pc)                                  \
    do {                                                 \
        if ((c).trace_fn) (c).trace_fn((c), (pc));       \
    } while (0)

constexpr uint32_t kMaxDepth = 20000;
[[noreturn]] void raise_fault(Context& ctx, Fault fault, uint32_t pc, uint32_t addr);
// Scope guard placed at the top of every whole-program function.
struct FrameGuard {
    Context& c;
    FrameGuard(Context& ctx, uint32_t vaddr) : c(ctx) {
        c.recent[c.recent_pos++ & 63] = vaddr;
        if (c.call_log) c.call_log->push_back(vaddr);
        if (c.trace_fn) c.trace_fn(c, vaddr | 1);  // odd = function entry marker
        if ((vaddr == c.watch_entry || vaddr == c.watch_entry2) && c.on_watch) c.on_watch(c, vaddr);
        if (++c.depth > kMaxDepth) raise_fault(c, Fault::Unimplemented, vaddr, 0xdeeedeee);
    }
    ~FrameGuard() { --c.depth; }
};

// Safe point: called by generated code at function entry and backward jumps.
void service(Context& c, uint32_t pc);
inline void poll(Context& c, uint32_t pc) {
    if (c.cycles >= c.next_event) service(c, pc);
}

// Hardware event-driven idle loop: invoked ONLY by the recompiler after an
// exact verified 5-instruction MFC0 Cause / NOP / ANDI / BEQ/BNE / delay
// pattern has completed a *taken* backward branch. CPU cycles are conserved.
// We only skip additional repetitions that cannot observe an IRQ change.
// All remaining iterations, interrupts and delay slots are interpreted by
// the original generated MIPS code.
inline void poll_irq_idle(Context& c, uint32_t pc, uint32_t mask,
                          unsigned tested_reg, unsigned counter_reg,
                          int counter_imm, unsigned loop_cycles = 6) {
    poll(c, pc);
    if (!c.fast_mips_idle || !c.io || loop_cycles == 0) return;
    const uint32_t current =
        (lo32(c.cp0[kCp0Cause]) & (mask & ~0x7c00u)) |
        (c.io->interrupt_lines(c) & (mask & 0x7c00u));
    // An IRQ may have changed BETWEEN the mfc0 at loop entry and the
    // branch's delay slot. If so, the very next real loop must inspect it.
    if ((lo32(c.r[tested_reg]) & mask) != (current & mask)) return;
    const uint64_t edge = c.io->irq_edge(c);
    const uint64_t deadline = edge < c.next_event ? edge : c.next_event;
    if (deadline <= c.cycles) return;
    // Stop BEFORE the next IRQ edge. The ordinary code then completes
    // the final iteration and passes through its normal interrupt safepoint.
    uint64_t repeats = (deadline - c.cycles - 1) / loop_cycles;
    if (repeats == 0) return;
    // Bounded guard against malformed firmware event deadlines.
    if (repeats > 300000) repeats = 300000;
    c.cycles += repeats * loop_cycles;
    if (counter_reg != 0) {
        const uint32_t next = lo32(c.r[counter_reg]) +
            static_cast<uint32_t>(counter_imm) * static_cast<uint32_t>(repeats);
        c.r[counter_reg] = sx32(next);
    }
    c.mips_idle_skipped_cycles += repeats * loop_cycles;
    c.mips_idle_skipped_iterations += repeats;
}
void schedule(Context& c);  // recompute next_event after state changes
void eret(Context& c);

uint64_t mfc0(Context& c, unsigned reg, uint32_t pc);
// The statically generated KI translation calls this wrapper. On R36S
// Release, the most frequent COP0 reads can be inlined at their call sites
// (reg is a compile-time constant), removing a cross-translation-unit call
// and reg switch. All semantics match mfc0(); no guest cycles are changed.
inline uint64_t mfc0_recompiled(Context& c, unsigned reg, uint32_t pc) {
#ifdef KI_R36S_FAST_COP0
    if (c.fast_cop0) {
        if (reg == kCp0Cause) {
            uint32_t cause = lo32(c.cp0[kCp0Cause]) & ~0x7c00u;
            if (c.io) cause |= c.io->interrupt_lines(c) & 0x7c00u;
            return cause;
        }
        if (reg == kCp0Count)
            return static_cast<uint32_t>(c.count_base + c.cycles / 2);
        if (reg == kCp0PRId) return 0x2020;
        return c.cp0[reg & 31];
    }
#endif
    return mfc0(c, reg, pc);
}
void mtc0(Context& c, unsigned reg, uint64_t value, uint32_t pc);
void cop0_op(Context& c, uint32_t instr, uint32_t pc);  // tlbwi, tlbr, eret, ...
void cache_op(Context& c, unsigned op, uint32_t vaddr, uint32_t pc);

}  // namespace ki
