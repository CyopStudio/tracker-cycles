// Engine benchmark shared by the host (`make bench`) and the on-device
// timing build (core/tidal/tidal_bench.cpp, -DTIDAL_BENCH): the same heavy
// patterns, queried like the scheduler will (one cycle, quarter cycles).
#pragma once
#include <stdint.h>

#include "engine.h"
#include "tree.h"

namespace tc {

typedef uint32_t (*BenchClock)();  // microseconds, wrapping is fine

struct BenchResult {
    const char* code;
    int events;          // events in the last one-cycle query
    int peak, peak4;     // arena high water: one-cycle / quarter-cycle queries
    uint32_t avgUs, maxUs;    // per one-cycle query
    uint32_t avgUs4, maxUs4;  // per quarter-cycle query
    bool ok;
    char err[64];
};

int benchCaseCount();
const char* benchCase(int i);

// Queries case i `reps` times one cycle at a time (cycles 0..15 in turn),
// then 4 * reps quarter-cycle windows. `out` receives each query's events.
bool runBenchCase(int i, Program& prog, Event* out, int cap, BenchClock clock, int reps, BenchResult& r);

}  // namespace tc
