#include "ki_r36s_host_profiler.h"
#include <ctime>
#include <cstdio>

// ROM-free check of Linux SIGPROF capture and symbol resolution.
// This is a diagnostic exercise only; no game files are loaded.
int main() {
    if (!ki_r36s::host_profiler::start()) return 1;
    const std::clock_t start = std::clock();
    volatile uint64_t dummy = 0;
    while (double(std::clock() - start) / CLOCKS_PER_SEC < 0.080) {
        dummy = dummy + 3; // intentionally consume CPU time for ITIMER_PROF
    }
    if (!ki_r36s::host_profiler::count.load(std::memory_order_relaxed)) {
        std::fputs("SIGPROF sampler got zero samples\n", stderr);
        ki_r36s::host_profiler::stop();
        return 2;
    }
    ki_r36s::host_profiler::report(3);
    ki_r36s::host_profiler::stop();
    std::fprintf(stderr, "Host CPU sampling profiler smoke test passed (%llu)\n",
                 static_cast<unsigned long long>(dummy));
    return 0;
}
