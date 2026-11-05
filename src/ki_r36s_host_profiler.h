// Test10: diagnostic-only host instruction-pointer sampling.
// This is NOT guest emulation: it samples the native AArch64 program counter
// on process CPU time, then groups symbols outside the signal handler.
// Nothing starts unless KI_R36S_HOST_PROFILE=1 is explicitly set.
#pragma once

#if defined(__linux__)
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <map>
#include <signal.h>
#include <string>
#include <sys/time.h>
#include <ucontext.h>
#include <vector>
#include <cxxabi.h>

namespace ki_r36s::host_profiler {
constexpr unsigned kSlots = 8192;
static_assert(std::atomic<uint32_t>::is_always_lock_free, "Signal sampling requires lock-free indices");
static_assert(std::atomic<uintptr_t>::is_always_lock_free, "Signal sampling requires lock-free PC storage");
inline std::atomic<uint32_t> count{0};
inline std::atomic<uintptr_t> pc_ring[kSlots]{};
inline struct sigaction previous_handler {};
inline bool started = false;

inline void handler(int, siginfo_t*, void* ptr) noexcept {
    auto* uc = static_cast<ucontext_t*>(ptr);
    uintptr_t pc = 0;
#if defined(__aarch64__)
    pc = static_cast<uintptr_t>(uc->uc_mcontext.pc);
#elif defined(__x86_64__) && defined(REG_RIP)
    pc = static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_RIP]);
#endif
    if (!pc) return;
    uint32_t slot = count.fetch_add(1, std::memory_order_relaxed);
    pc_ring[slot % kSlots].store(pc, std::memory_order_relaxed);
}

inline bool timer(bool on) {
    struct itimerval value{};
    if (on) {
        value.it_interval.tv_usec = 5000; // one sample per 5 ms of PROCESS CPU
        value.it_value = value.it_interval;
    }
    return setitimer(ITIMER_PROF, &value, nullptr) == 0;
}

inline bool start() {
    if (started) return true;
    struct sigaction action{};
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = handler;
    if (sigaction(SIGPROF, &action, &previous_handler) != 0) {
        std::perror("[r36s] sigaction(SIGPROF)");
        return false;
    }
    if (!timer(true)) {
        std::perror("[r36s] setitimer");
        sigaction(SIGPROF, &previous_handler, nullptr);
        return false;
    }
    started = true;
    std::fprintf(stderr,
        "[r36s] HOST_PROFILE ON: SIGPROF sampling 200Hz CPU, symbolized with dladdr; diagnostic only\n");
    return true;
}

inline void report(unsigned frames) {
    if (!started) return;
    timer(false); // No signals while reading the capture or resolving symbols.
    const uint32_t total = count.exchange(0, std::memory_order_relaxed);
    const uint32_t n = std::min<uint32_t>(total, kSlots);
    std::map<std::string, uint32_t> histogram;
    for (uint32_t i = 0; i < n; ++i) {
        uintptr_t site = pc_ring[i].load(std::memory_order_relaxed);
        if (!site) continue;
        Dl_info info{};
        if (!dladdr(reinterpret_cast<void*>(site), &info)) {
            ++histogram["<unmapped>"];
            continue;
        }
        if (!info.dli_sname) {
            // Many SDL/GLES driver libraries are stripped: still report
            // their module name instead of an opaque 20% <unknown> bucket.
            std::string module = info.dli_fname ? info.dli_fname : "<anonymous>";
            const auto slash = module.find_last_of('/');
            if (slash != std::string::npos) module.erase(0, slash + 1);
            ++histogram["<no-symbol>:" + module];
            continue;
        }
        int status = 0;
        char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
        std::string name = status == 0 && demangled ? demangled : info.dli_sname;
        std::free(demangled);
        if (name.size() > 100) name.resize(100);
        ++histogram[name];
    }
    std::vector<std::pair<std::string,uint32_t>> ranking(histogram.begin(), histogram.end());
    std::sort(ranking.begin(), ranking.end(),
              [](const auto& a, const auto& b) {
                  if (a.second != b.second) return a.second > b.second;
                  return a.first < b.first;
              });
    std::fprintf(stderr,
        "[r36s] HOST_PROFILE frames=%u cpu_pc_samples=%u retained=%u unique_symbols=%zu\n",
        frames, total, n, ranking.size());
    for (size_t i = 0; i < std::min<size_t>(ranking.size(), 15); ++i) {
        const auto& [name, hits] = ranking[i];
        std::fprintf(stderr, "[r36s] HOST_TOP rank=%zu pct=%.2f%% hits=%u sym=%s\n",
                     i+1, n ? 100.0 * hits / n : 0.0, hits, name.c_str());
    }
    timer(true);
}

inline void stop() {
    if (!started) return;
    timer(false);
    sigaction(SIGPROF, &previous_handler, nullptr);
    started = false;
}
}  // namespace ki_r36s::host_profiler
#else
namespace ki_r36s::host_profiler {
inline bool start() { return false; }
inline void report(unsigned) {}
inline void stop() {}
}
#endif
