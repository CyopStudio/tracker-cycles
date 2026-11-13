// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Throwaway on-device timing build (see tidal_bench.h). Blocks the main
// loop while it runs (several seconds): the display and SD streaming
// pause, audio keeps running in its interrupt.
//
// v4: besides the timings, a PC-sampling profile: the SysTick vector (1 kHz,
// above the audio interrupt's priority) is swapped at run time for a
// handler that records the interrupted instruction address in a histogram
// (32-byte bins over the 1 MB flash). core/tidal/bench_profile.py names
// the functions from the build's ELF. The evaluator's deepest stack point
// comes from TC_TRACK_STACK (tidal_config.h).
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

// ---- PC sampling (C linkage: used from the naked handler) ------------------------
extern "C" {
extern void (*_VectorsRam[])(void);  // teensy core: vector table in RAM (SCB_VTOR)
extern char* __brkval;               // heap end
extern unsigned long _estack;        // top of RAM
void (*gTidalOrigSysTick)(void) = nullptr;
volatile uint32_t gTidalPcOn = 0, gTidalPcTotal = 0, gTidalPcOther = 0;
}
constexpr uint32_t kFlashSize = 1024 * 1024, kBinShift = 5;
__NOINIT(EXTERNAL_RAM) static uint16_t pcBins[kFlashSize >> kBinShift];

extern "C" void tidalBenchSample(const uint32_t* frame) {
    if (!gTidalPcOn) return;
    uint32_t pc = frame[6];  // stacked PC (same offset with an FPU frame)
    gTidalPcTotal = gTidalPcTotal + 1;
    if (pc < kFlashSize) {
        uint16_t& b = pcBins[pc >> kBinShift];
        if (b != 0xFFFF) b++;
    } else {
        gTidalPcOther = gTidalPcOther + 1;
    }
}

// Exception entry: r0 = the interrupted context's stack frame; sample, then
// tail-jump to the original handler with LR (EXC_RETURN) intact.
extern "C" __attribute__((naked)) void tidalBenchSysTick(void) {
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "push {r0, lr}\n"
        "bl tidalBenchSample\n"
        "pop {r0, lr}\n"
        "movw r1, #:lower16:gTidalOrigSysTick\n"
        "movt r1, #:upper16:gTidalOrigSysTick\n"
        "ldr r1, [r1]\n"
        "bx r1\n");
}

namespace {

constexpr int kOutCap = 512;
constexpr int kReps = 16;  // one-cycle queries per pattern (and 4x quarter-cycle ones)
constexpr uint32_t kRunAtMs[2] = {10000, 60000};
#ifdef TC_ENGINE_O2
#define TC_BENCH_VARIANT "engine -O2"
const char* const kPath = "/tidal_bench_o2.txt";
#else
#define TC_BENCH_VARIANT "engine -Os"
const char* const kPath = "/tidal_bench.txt";
#endif

// v6: quarter-cycle pass with the whole working set (arena, control pool,
// parameter stack) in internal RAM, sized like a real build could afford.
constexpr int kFastEvents = 256, kFastCtls = 1024, kFastParams = 128;
unsigned char fastMem[26 * 1024] __attribute__((aligned(8)));

// Big buffers in SDRAM, constructed when the bench starts (NOLOAD section).
__NOINIT(EXTERNAL_RAM) unsigned char progRaw[sizeof(tc::Program)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char outRaw[sizeof(tc::Event) * kOutCap] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) char report[8192];

Platform* gPlatform = nullptr;
int gStage = 0;  // 0: wait run 1, 1: run 1 shown, 2: wait run 2, 3: run 2 shown, 4: done
uint32_t gShownAt = 0;
bool gResultShown = false;  // a "done" popup is up; hidden after kPopupMs
constexpr uint32_t kPopupMs = 4000;
char gLine1[64], gLine2[64];

uint32_t clockUs() { return gPlatform->getTimer()->micros(); }

void pcStart() {
    memset(pcBins, 0, sizeof pcBins);
    gTidalPcTotal = 0;
    gTidalPcOther = 0;
    if (!gTidalOrigSysTick) {
        __asm volatile("cpsid i" ::: "memory");
        gTidalOrigSysTick = _VectorsRam[15];
        _VectorsRam[15] = tidalBenchSysTick;
        __asm volatile("dsb\n isb\n cpsie i" ::: "memory");
    }
    gTidalPcOn = 1;
}
void pcStop() { gTidalPcOn = 0; }

bool save(int run, bool first, int n) {
    FileSystem* fs = gPlatform->getFileSystem();
    auto [o, fh] =
        fs->openFile(kPath, run == 1 && first ? FileSystem::kWriteCreateNew : FileSystem::kWriteAppend);
    bool ok = o && fh;
    if (ok) {
        fs->writeFile(report, (size_t)n, fh);
        fs->closeFile(fh);
    }
    return ok;
}

int runAll(int run, int at) {
    tc::Program* prog = new (progRaw) tc::Program();
    tc::Event* out = reinterpret_cast<tc::Event*>(outRaw);
    for (int i = 0; i < kOutCap; i++) new (&out[i]) tc::Event();

    Profiler::AudioProcessorUsage u{};
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
    tc::engineResetMinStack();
    volatile char here = 0;
    const uintptr_t top = (uintptr_t)&_estack, brk = (uintptr_t)__brkval, sp = (uintptr_t)&here;

    int n = 0;
    n += snprintf(report + n, sizeof report - n,
                  "Tracker Cycles engine bench v6 (" TC_BENCH_VARIANT "), run %d (%d s after boot)\n"
                  "event %u B, arena %d events, %d one-cycle + %d quarter-cycle queries per pattern\n"
                  "times in microseconds; peak = arena high water (events); arena + program in SDRAM\n\n"
                  "%-6s %-5s %-6s %-7s %-7s %-6s %-6s  pattern\n",
                  run, at / 1000, (unsigned)tc::engineEventBytes(), tc::kEventArena, kReps, kReps * 4, "events", "peak",
                  "peak/4", "us/cyc", "max", "us/q4", "max");
    uint32_t worstAvg = 0, worstQ4 = 0;
    int worstCtl = 0, worstCtl4 = 0;
    pcStart();
    for (int i = 0; i < tc::benchCaseCount() && n < (int)sizeof report - 200; i++) {
        tc::BenchResult r;
        if (!tc::runBenchCase(i, *prog, out, kOutCap, clockUs, kReps, r)) {
            n += snprintf(report + n, sizeof report - n, "error: %s: %.60s\n", r.err, r.code);
            continue;
        }
        if (r.avgUs > worstAvg) worstAvg = r.avgUs;
        if (r.avgUs4 > worstQ4) worstQ4 = r.avgUs4;
        if (r.ctl > worstCtl) worstCtl = r.ctl;
        if (r.ctl4 > worstCtl4) worstCtl4 = r.ctl4;
        n += snprintf(report + n, sizeof report - n, "%-6d %-5d %-6d %-7lu %-7lu %-6lu %-6lu  %.60s\n", r.events,
                      r.peak, r.peak4, (unsigned long)r.avgUs, (unsigned long)r.maxUs, (unsigned long)r.avgUs4,
                      (unsigned long)r.maxUs4, r.code);
    }
    pcStop();
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
    const uintptr_t minSp = tc::engineMinStack();
    n += snprintf(report + n, sizeof report - n,
                  "\nworst: %lu us/cycle, %lu us/quarter; audio max %d %% during the run (avg since reset %d %%)\n"
                  "stack: bench entry %lu B below the top of RAM, evaluator deepest %lu B (%lu B below entry);\n"
                  "  heap end (brk) %lu B below the top: %ld B left between the deepest point and the heap\n"
                  "control pool peak %d one-cycle, %d quarter-cycle (of %d)\n"
                  "pc samples: %lu (outside flash %lu)\n",
                  (unsigned long)worstAvg, (unsigned long)worstQ4, u.tempMaximum, u.average,
                  (unsigned long)(top - sp), (unsigned long)(top - minSp), (unsigned long)(sp - minSp),
                  (unsigned long)(top - brk), (long)minSp - (long)brk, worstCtl, worstCtl4, tc::kCtlPool,
                  (unsigned long)gTidalPcTotal,
                  (unsigned long)gTidalPcOther);
    bool ok = save(run, true, n);

    // internal-RAM pass (quarter-cycle queries only)
    const int need = tc::engineMemoryBytes(kFastEvents, kFastCtls, kFastParams);
    n = snprintf(report, sizeof report,
                 "\nquarter-cycle queries, working set in internal RAM (%d B: %d events, %d controls, %d params):\n"
                 "%-6s %-6s %-6s %-6s  pattern\n",
                 need, kFastEvents, kFastCtls, kFastParams, "peak/4", "ctl/4", "us/q4", "max");
    uint32_t worstFast = 0;
    if (need <= (int)sizeof fastMem) {
        tc::engineUseMemory(fastMem, kFastEvents, kFastCtls, kFastParams);
        for (int i = 0; i < tc::benchCaseCount() && n < (int)sizeof report - 200; i++) {
            tc::BenchResult r;
            if (!tc::runBenchCase(i, *prog, out, kOutCap, clockUs, kReps, r, false)) {
                n += snprintf(report + n, sizeof report - n, "error: %s: %.60s\n", r.err, r.code);
                continue;
            }
            if (r.avgUs4 > worstFast) worstFast = r.avgUs4;
            n += snprintf(report + n, sizeof report - n, "%-6d %-6d %-6lu %-6lu  %.60s\n", r.peak4, r.ctl4,
                          (unsigned long)r.avgUs4, (unsigned long)r.maxUs4, r.code);
        }
        tc::engineUseMemory(nullptr, 0, 0, 0);
        n += snprintf(report + n, sizeof report - n, "worst %lu us/quarter (%lu%% of the SDRAM pass)\n",
                      (unsigned long)worstFast, (unsigned long)(worstQ4 ? worstFast * 100 / worstQ4 : 0));
    } else {
        n += snprintf(report + n, sizeof report - n, "skipped: needs %d B, buffer %u B\n", need,
                      (unsigned)sizeof fastMem);
    }
    ok = save(run, false, n) && ok;

    // histogram: "pc <run> <bin address> <count>", non-empty bins only
    n = snprintf(report, sizeof report, "pcprofile run %d samples %lu other %lu bin %lu\n", run,
                 (unsigned long)gTidalPcTotal, (unsigned long)gTidalPcOther, (unsigned long)(1u << kBinShift));
    for (uint32_t b = 0; b < (kFlashSize >> kBinShift); b++) {
        if (!pcBins[b]) continue;
        n += snprintf(report + n, sizeof report - n, "pc %d %05lx %u\n", run, (unsigned long)(b << kBinShift),
                      (unsigned)pcBins[b]);
        if (n > (int)sizeof report - 64) {
            ok = save(run, false, n) && ok;
            n = 0;
        }
    }
    n += snprintf(report + n, sizeof report - n, "\n");
    ok = save(run, false, n) && ok;

    prog->~Program();
    snprintf(gLine2, sizeof gLine2, "worst %lu us/quarter%s", (unsigned long)worstQ4, ok ? "" : " - SD write failed");
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
            mtPopups.showInfoPopup(gLine1, run == 0 ? "screen freezes for ~10 s" : "keep playing (~40 s)");
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
