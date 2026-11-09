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
__NOINIT(EXTERNAL_RAM) char report[8192];
// SDRAM code-bus alias (bench v3): the MK66 has no system-bus cache, so
// SDRAM at 0x70000000 is uncached; NXP notes (AN4745, K65) describe an
// alias at 0x08000000 cached by the 8 KB code cache, write-through only.
// Probed with bus faults ignored, then used for a second pass.
#define REG32(a) (*(volatile uint32_t*)(a))
constexpr uintptr_t kLmemPcccr = 0xE0082000, kLmemPccrmr = 0xE0082020;
constexpr uintptr_t kScbCcr = 0xE000ED14, kScbCfsr = 0xE000ED28;
constexpr uint32_t kPcccrGo = 0x80000000u, kPcccrInvw0 = 0x01000000u, kPcccrInvw1 = 0x04000000u;
constexpr uint32_t kPccrmrR1Mask = 0x30000000u, kPccrmrR1WriteThrough = 0x20000000u;
constexpr uintptr_t kSdram = 0x70000000u, kAlias = 0x08000000u;
template <class T>
T* viaAlias(T* p) {
    return reinterpret_cast<T*>((uintptr_t)p - kSdram + kAlias);
}
// Read with precise bus faults ignored (CCR.BFHFNMIGN at FAULTMASK
// priority); false if the read faulted.
bool probeRead(uintptr_t addr, uint32_t& v) {
    REG32(kScbCfsr) = 0x0000FF00u;  // clear BusFault status (write 1 to clear)
    uint32_t ccr = REG32(kScbCcr);
    REG32(kScbCcr) = ccr | (1u << 8);
    __asm volatile("dsb\n isb\n cpsid f" ::: "memory");
    v = *(volatile uint32_t*)addr;
    __asm volatile("dsb\n cpsie f\n isb" ::: "memory");
    REG32(kScbCcr) = ccr;
    bool fault = (REG32(kScbCfsr) & 0x0000FF00u) != 0;
    REG32(kScbCfsr) = 0x0000FF00u;
    return !fault;
}
void codeCacheInvalidate() {
    REG32(kLmemPcccr) = REG32(kLmemPcccr) | kPcccrGo | kPcccrInvw0 | kPcccrInvw1;
    while (REG32(kLmemPcccr) & kPcccrGo) {
    }
}
// Write a word through SDRAM, read it back through the alias.
bool aliasWorks(void* sdramWord) {
    volatile uint32_t* w = (volatile uint32_t*)sdramWord;
    uint32_t saved = *w, v = 0;
    bool ok = true;
    for (uint32_t pat : {0x1234ABCDu, 0xEDCB5432u}) {
        *w = pat;
        __asm volatile("dsb" ::: "memory");
        codeCacheInvalidate();
        if (!probeRead((uintptr_t)viaAlias((void*)sdramWord), v) || v != pat) ok = false;
    }
    *w = saved;
    codeCacheInvalidate();
    return ok;
}

// Stack high water (MK66 Profiler::maxStackBytes() returns 0): paint the
// gap between heap and stack before a run, find the deepest overwrite after.
extern "C" char* __brkval;
extern "C" unsigned long _estack;
constexpr uint32_t kPaint = 0x5AC3A55Cu;
uintptr_t gPaintLo = 0, gPaintHi = 0;

__attribute__((noinline)) void paintStack() {
    volatile char here = 0;
    uintptr_t sp = (uintptr_t)&here;
    gPaintLo = ((uintptr_t)__brkval + 1024 + 3) & ~(uintptr_t)3;  // room for heap growth
    gPaintHi = (sp - 512) & ~(uintptr_t)3;
    for (volatile uint32_t* p = (uint32_t*)gPaintLo; p < (uint32_t*)gPaintHi; p++) *p = kPaint;
}
// deepest stack use in bytes below the top of RAM (main loop + interrupts)
uint32_t stackDepth() {
    const uint32_t* p = (const uint32_t*)gPaintLo;
    while (p < (const uint32_t*)gPaintHi && *p == kPaint) p++;
    return (uint32_t)((uintptr_t)&_estack - (uintptr_t)p);
}

Platform* gPlatform = nullptr;
int gStage = 0;  // 0: wait run 1, 1: run 1 shown, 2: wait run 2, 3: run 2 shown, 4: done
uint32_t gShownAt = 0;
bool gResultShown = false;  // a "done" popup is up; hidden after kPopupMs
constexpr uint32_t kPopupMs = 4000;
char gLine1[64], gLine2[64];

uint32_t clockUs() { return gPlatform->getTimer()->micros(); }

void save(int run, bool first, int n, bool& ok) {
    FileSystem* fs = gPlatform->getFileSystem();
    auto [o, fh] =
        fs->openFile(kPath, run == 1 && first ? FileSystem::kWriteCreateNew : FileSystem::kWriteAppend);
    ok = o && fh;
    if (ok) {
        fs->writeFile(report, (size_t)n, fh);
        fs->closeFile(fh);
    }
}

// One table of all bench cases; returns the new report length.
int table(int n, tc::Program& prog, tc::Event* out, uint32_t& worstAvg, uint32_t& worstQ4) {
    n += snprintf(report + n, sizeof report - n, "%-6s %-5s %-6s %-7s %-7s %-6s %-6s  pattern\n", "events", "peak",
                  "peak/4", "us/cyc", "max", "us/q4", "max");
    worstAvg = worstQ4 = 0;
    for (int i = 0; i < tc::benchCaseCount() && n < (int)sizeof report - 200; i++) {
        tc::BenchResult r;
        if (!tc::runBenchCase(i, prog, out, kOutCap, clockUs, kReps, r)) {
            n += snprintf(report + n, sizeof report - n, "error: %s: %.60s\n", r.err, r.code);
            continue;
        }
        if (r.avgUs > worstAvg) worstAvg = r.avgUs;
        if (r.avgUs4 > worstQ4) worstQ4 = r.avgUs4;
        n += snprintf(report + n, sizeof report - n, "%-6d %-5d %-6d %-7lu %-7lu %-6lu %-6lu  %.60s\n", r.events,
                      r.peak, r.peak4, (unsigned long)r.avgUs, (unsigned long)r.maxUs, (unsigned long)r.avgUs4,
                      (unsigned long)r.maxUs4, r.code);
    }
    return n;
}

int runAll(int run, int at) {
    tc::Program* prog = new (progRaw) tc::Program();
    tc::Event* out = reinterpret_cast<tc::Event*>(outRaw);
    for (int i = 0; i < kOutCap; i++) new (&out[i]) tc::Event();

    Profiler::AudioProcessorUsage u{};
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
    paintStack();
    const uint32_t gap = (uint32_t)(gPaintHi - gPaintLo);

    int n = 0;
    n += snprintf(report + n, sizeof report - n,
                  "Tracker Cycles engine bench v3, run %d (%d s after boot)\n"
                  "event %u B, arena %d events, %d one-cycle + %d quarter-cycle queries per pattern\n"
                  "times in microseconds; peak = arena high water (events)\n"
                  "free heap/stack gap at start %lu B\n\n"
                  "-- arena + program in SDRAM (0x70000000, uncached)\n",
                  run, at / 1000, (unsigned)sizeof(tc::Event), tc::kEventArena, kReps, kReps * 4,
                  (unsigned long)gap);
    uint32_t worstAvg, worstQ4;
    n = table(n, *prog, out, worstAvg, worstQ4);
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
    n += snprintf(report + n, sizeof report - n,
                  "worst: %lu us/cycle, %lu us/quarter; audio max %d %% during the pass (avg since reset %d %%)\n"
                  "deepest stack: %lu B below the top of RAM%s\n",
                  (unsigned long)worstAvg, (unsigned long)worstQ4, u.tempMaximum, u.average,
                  (unsigned long)stackDepth(),
                  stackDepth() >= gap ? " (SATURATED: used the whole gap)" : "");
    bool ok = false, ok2 = true;
    save(run, true, n, ok);  // on the card before the riskier alias pass
    uint32_t q4Sdram = worstQ4;

    n = 0;
    const uint32_t rmr = REG32(kLmemPccrmr), ccr = REG32(kLmemPcccr);
    bool aliasArena = aliasWorks(tc::engineDefaultArena() + tc::kEventArena - 1);
    bool aliasProg = aliasWorks(progRaw + sizeof(tc::Program) - 8);
    n += snprintf(report + n, sizeof report - n,
                  "\n-- SDRAM through the code-bus alias (0x08000000, code cache)\n"
                  "PCCCR %08lx, PCCRMR %08lx; alias readable at arena end: %s, program end: %s\n",
                  (unsigned long)ccr, (unsigned long)rmr, aliasArena ? "yes" : "no", aliasProg ? "yes" : "no");
    if (aliasArena && aliasProg && (ccr & 1u)) {
        REG32(kLmemPccrmr) = (rmr & ~kPccrmrR1Mask) | kPccrmrR1WriteThrough;
        codeCacheInvalidate();
        tc::engineUseArena(viaAlias(tc::engineDefaultArena()), tc::kEventArena);
        tc::Program* progA = viaAlias(prog);
        n = table(n, *progA, out, worstAvg, worstQ4);
        tc::engineUseArena(nullptr, 0);
        codeCacheInvalidate();
        REG32(kLmemPccrmr) = rmr;
        Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
        n += snprintf(report + n, sizeof report - n,
                      "worst: %lu us/cycle, %lu us/quarter (%lu%% of uncached); audio max %d %% during the pass\n",
                      (unsigned long)worstAvg, (unsigned long)worstQ4,
                      (unsigned long)(q4Sdram ? worstQ4 * 100 / q4Sdram : 0), u.tempMaximum);
    } else {
        n += snprintf(report + n, sizeof report - n, "skipped (no alias, or code cache off)\n");
    }
    n += snprintf(report + n, sizeof report - n, "deepest stack: %lu B\n\n", (unsigned long)stackDepth());
    save(run, false, n, ok2);
    prog->~Program();
    snprintf(gLine2, sizeof gLine2, "worst %lu us/quarter%s", (unsigned long)q4Sdram,
             ok && ok2 ? "" : " - SD write failed");
    return n;
}

}  // namespace

void tidalBenchUpdate(Platform* platform) {
    gPlatform = platform;
    uint32_t now = platform->getTimer()->millis();
    // info popups don't time out by themselves: hide "done" so the screen
    // is usable again (e.g. to load a project before run 2)
    if (gResultShown && now - gShownAt >= kPopupMs) {
        mtPopups.hideInfoPopup();
        gResultShown = false;
    }
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
            gShownAt = platform->getTimer()->millis();  // after the run, not before
            gResultShown = true;
            gStage++;
            return;
        }
        default: return;
    }
}
#endif
