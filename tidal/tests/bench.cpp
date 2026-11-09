// Host engine benchmark (`make bench`): the shared cases in core/bench.cpp,
// the same ones the on-device timing build runs.
#include <stdio.h>

#include <chrono>

#include "bench.h"

using namespace tc;

static Program gProg;
static Event gOut[1024];

static uint32_t clockUs() {
    static const auto start = std::chrono::steady_clock::now();
    return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start)
        .count();
}

int main() {
    printf("%-6s %-5s %-6s %-7s %-7s %-6s %-6s  pattern\n", "events", "peak", "peak/4", "us/cyc", "max", "us/q4",
           "max");
    int worst = 0, worstCtl = 0;
    for (int i = 0; i < benchCaseCount(); i++) {
        BenchResult r;
        if (!runBenchCase(i, gProg, gOut, 1024, clockUs, 200, r)) {
            printf("error: %s: %s\n", r.err, r.code);
            continue;
        }
        if (r.peak > worst) worst = r.peak;
        if (r.ctl > worstCtl) worstCtl = r.ctl;
        printf("%-6d %-5d %-6d %-7u %-7u %-6u %-6u  %.60s\n", r.events, r.peak, r.peak4, r.avgUs, r.maxUs, r.avgUs4,
               r.maxUs4, r.code);
    }
    printf("worst arena peak %d events x %d B = %d KB (arena %d); control pool peak %d of %d\n", worst,
           engineEventBytes(), worst * engineEventBytes() / 1024, kEventArena, worstCtl, kCtlPool);
    return 0;
}
