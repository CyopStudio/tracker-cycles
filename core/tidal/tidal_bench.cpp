// Throwaway on-device timing build (see tidal_bench.h). Blocks the main
// loop while it runs (a few seconds): the display and SD streaming pause,
// audio keeps running in its interrupt.
#ifdef TIDAL_BENCH
#include "tidal_bench.h"

#include <stdio.h>
#include <string.h>

#include <new>

#include "Platform.h"
#include "Profiler.h"
#include "core/interfacePopups.h"
#include "cr_section_macros.h"
#include "tidal_config.h"
#include "../../tidal/core/bench.h"

namespace {

constexpr int kOutCap = 512;
constexpr int kReps = 16;  // one-cycle queries per pattern (and 4x quarter-cycle ones)
constexpr uint32_t kRunAtMs[2] = {10000, 60000};
const char* const kPath = "/tidal_bench.txt";

// Big buffers in SDRAM, constructed when the bench starts (NOLOAD section).
__NOINIT(EXTERNAL_RAM) unsigned char progRaw[sizeof(tc::Program)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char outRaw[sizeof(tc::Event) * kOutCap] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) char report[6144];

Platform* gPlatform = nullptr;
int gStage = 0;  // 0: wait run 1, 1: run 1 shown, 2: wait run 2, 3: run 2 shown, 4: done
uint32_t gShownAt = 0;
char gLine1[64], gLine2[64];

uint32_t clockUs() { return gPlatform->getTimer()->micros(); }

int runAll(int run, int at) {
    tc::Program* prog = new (progRaw) tc::Program();
    tc::Event* out = reinterpret_cast<tc::Event*>(outRaw);
    for (int i = 0; i < kOutCap; i++) new (&out[i]) tc::Event();

    Profiler::AudioProcessorUsage before{}, after{};
    Profiler::getInstance()->getAudioProcessingUsagePercent(before, true);

    int n = 0;
    n += snprintf(report + n, sizeof report - n,
                  "Tracker Cycles engine bench, run %d (%d s after boot)\n"
                  "event %u B, arena %d events, %d one-cycle + %d quarter-cycle queries per pattern\n"
                  "times in microseconds; peak = arena high water (events)\n\n"
                  "%-6s %-5s %-6s %-7s %-7s %-6s %-6s  pattern\n",
                  run, at / 1000, (unsigned)sizeof(tc::Event), tc::kEventArena, kReps, kReps * 4, "events",
                  "peak", "peak/4", "us/cyc", "max", "us/q4", "max");
    uint32_t worstAvg = 0, worstMax = 0;
    for (int i = 0; i < tc::benchCaseCount() && n < (int)sizeof report - 200; i++) {
        tc::BenchResult r;
        if (!tc::runBenchCase(i, *prog, out, kOutCap, clockUs, kReps, r)) {
            n += snprintf(report + n, sizeof report - n, "error: %s: %.60s\n", r.err, r.code);
            continue;
        }
        if (r.avgUs > worstAvg) worstAvg = r.avgUs;
        if (r.maxUs > worstMax) worstMax = r.maxUs;
        n += snprintf(report + n, sizeof report - n, "%-6d %-5d %-6d %-7lu %-7lu %-6lu %-6lu  %.60s\n", r.events,
                      r.peak, r.peak4, (unsigned long)r.avgUs, (unsigned long)r.maxUs, (unsigned long)r.avgUs4,
                      (unsigned long)r.maxUs4, r.code);
    }
    Profiler::getInstance()->getAudioProcessingUsagePercent(after, true);
    n += snprintf(report + n, sizeof report - n,
                  "\nworst one-cycle query: avg %lu us, max %lu us\n"
                  "audio interrupt load during the run: avg %d %%, max %d %%\n\n",
                  (unsigned long)worstAvg, (unsigned long)worstMax, after.average, after.maximum);

    FileSystem* fs = gPlatform->getFileSystem();
    auto [ok, fh] = fs->openFile(kPath, run == 1 ? FileSystem::kWriteCreateNew : FileSystem::kWriteAppend);
    if (ok && fh) {
        fs->writeFile(report, (size_t)n, fh);
        fs->closeFile(fh);
    }
    prog->~Program();
    snprintf(gLine2, sizeof gLine2, "worst %lu us/cycle (max %lu)%s", (unsigned long)worstAvg,
             (unsigned long)worstMax, ok ? "" : " - SD write failed");
    return n;
}

}  // namespace

void tidalBenchUpdate(Platform* platform) {
    gPlatform = platform;
    uint32_t now = platform->getTimer()->millis();
    switch (gStage) {
        case 0:
        case 2: {
            int run = gStage / 2;  // 0 or 1
            if (now < kRunAtMs[run]) return;
            snprintf(gLine1, sizeof gLine1, "Tidal engine bench %d running...", run + 1);
            mtPopups.showInfoPopup(gLine1, run == 0 ? "screen freezes for a few seconds" : "keep playing");
            gShownAt = now;
            gStage++;
            return;
        }
        case 1:
        case 3: {
            if (now - gShownAt < 500) return;  // let the popup draw first
            int run = gStage == 1 ? 1 : 2;
            runAll(run, (int)kRunAtMs[run - 1]);
            snprintf(gLine1, sizeof gLine1, run == 1 ? "Bench 1 done. Start a project playing;"
                                                     : "Bench 2 done: /tidal_bench.txt");
            if (run == 1) snprintf(gLine2, sizeof gLine2, "bench 2 runs at 60 s.");
            mtPopups.showInfoPopup(gLine1, gLine2);
            gShownAt = now;
            gStage++;
            return;
        }
        default: return;
    }
}
#endif
