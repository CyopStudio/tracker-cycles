// Device tick test build (see tidal_tick.h and README.md).
//
// 15 s after boot it plays a list of test sets (built in, or the one in
// /tidal_tick.tidal if the card has it), 20 s each, on the loaded
// project's sample instruments: `s` picks the instrument whose file name
// matches (case-insensitive, without extension), else the instrument at
// that name's position in the prototype's default list (bd = 1, sd = 2,
// ...), else wraps onto the active instruments. `n` / `note` add to C5,
// `gain` sets the step volume (x100). d9..d16 (MIDI) are not mapped yet.
//
// After each set it appends to /tidal_tick.txt: interrupt cost of
// tidalTick per block (all blocks / blocks that started notes) and per
// noteOn, note starts per block, lateness, the scheduler's counters, the
// main-loop time spent in update() and the audio load.
#ifdef TIDAL_TICK
#include "tidal_tick.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <new>

#include "Platform.h"
#include "Profiler.h"
#include "core/interfacePopups.h"
#include "cr_section_macros.h"
#include "mtAudioEngine.h"
#include "mtStructs.h"
#include "tidal_config.h"
#include "../../tidal/core/check.h"
#include "../../tidal/core/parse.h"
#include "../../tidal/core/scheduler.h"

extern "C" {
extern char* __brkval;        // heap end (teensy core)
extern unsigned long _estack;  // top of RAM
}

namespace {

using tc::Entry;

#define CYCLES() (*(volatile uint32_t*)0xE0001004u)  // DWT->CYCCNT

constexpr uint32_t kStartMs = 15000, kSetMs = 20000, kFileSetMs = 60000;
constexpr uint32_t kUpdateBudgetUs = 4000;
const char* const kPath = "/tidal_tick.txt";
const char* const kUserSet = "/tidal_tick.tidal";

// Built-in sets. Names resolve onto any project (see the top).
const char* const kSets[] = {
    // 1: light
    "d1 $ s \"bd*4\"\n"
    "d2 $ s \"~ sd\"\n"
    "d3 $ s \"hh*8\" # gain 0.8\n",
    // 2: everyday, 8 slots
    "d1 $ s \"bd*2 [~ bd] bd\"\n"
    "d2 $ s \"~ sd\" # n \"<0 1>\"\n"
    "d3 $ every 2 (fast 2) $ s \"hh*8\"\n"
    "d4 $ n \"0 .. 7\" # s \"arpy\"\n"
    "d5 $ off 0.125 (|+ n 12) $ n \"0 3 7\" # s \"bass\"\n"
    "d6 $ s \"cp*2\" # pan sine\n"
    "d7 $ sometimes (# speed 2) $ s \"drum*4\"\n"
    "d8 $ s \"<lt mt ht>*3\"\n",
    // 3: heavy (bench patterns) at a faster tempo
    "setcps 0.7\n"
    "d1 $ stut 8 0.5 0.0625 $ s \"hh*16\" # n \"0 .. 15\"\n"
    "d2 $ jux rev $ chop 8 $ s \"bd*8 sd*8\" # speed \"1 2\"\n"
    "d3 $ echo 8 0.125 0.7 $ s \"bd(5,8) sd(3,8,2)\" # n (irand 8)\n"
    "d4 $ off 0.125 (|+ n 12) $ off 0.25 (|+ n 7) $ n (scale \"major\" \"0 .. 7\") # s \"arpy\"\n"
    "d5 $ chunk 4 (hurry 2) $ iter 4 $ s \"bd sd hh cp hh sd bd cp\" # n \"0 .. 7\"\n"
    "d6 $ s \"hh*16\" # n \"0 .. 15\" # pan sine # gain (range 0.8 1 saw)\n"
    "d7 $ sometimes (# speed 2) $ often (# crush 4) $ s \"[bd*4, hh*8, ~ sd]\"\n"
    "d8 $ n (arp \"up\" \"<c'maj e'min7 g'dom7 a'min>*2\") # s \"bass\" # legato 0.5\n",
    // 4: every slot hits together (note-start cap: 8 starts per 16th)
    "setcps 0.575\n"
    "d1 $ s \"bd*16\"\n"
    "d2 $ s \"sd*16\"\n"
    "d3 $ s \"hh*16\"\n"
    "d4 $ s \"cp*16\"\n"
    "d5 $ s \"ho*16\"\n"
    "d6 $ s \"cr*16\"\n"
    "d7 $ s \"lt*16\"\n"
    "d8 $ s \"mt*16\"\n",
};
constexpr int kSetCount = sizeof kSets / sizeof kSets[0];

// Same order as the prototype's default instrument list (tidal/proto/editor.cpp).
const char* const kDefaultNames[48] = {
    "bd",    "sd",    "hh",    "cp",    "ho",     "cr",     "lt",    "mt",    "ht",    "drum",
    "sn",    "tink",  "808bd", "808sd", "808hc",  "808oh",  "808cy", "808lt", "808mt", "808ht",
    "808lc", "808mc", "808",   "hh27",  "arpy",   "bass",   "bass1", "bass2", "bass3", "jvbass",
    "pluck", "casio", "blip",  "bend",  "bev",    "bottle", "break", "jazz",  "feel",  "future",
    "gab",   "hand",  "east",  "kurt",  "rave",   "space",  "tabla", "wind"};

// ---- memory -------------------------------------------------------------------
// SDRAM is not initialised when static constructors run: raw storage,
// placement-new at start (CLAUDE.md).
constexpr int kQueryCap = 256;
__NOINIT(EXTERNAL_RAM) unsigned char schedRaw[sizeof(tc::Scheduler)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char liveRaw[sizeof(tc::Program)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char editRaw[sizeof(tc::Program)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char bufRaw[sizeof(tc::Event) * kQueryCap] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) char userText[16384];
__NOINIT(EXTERNAL_RAM) char report[4096];

// The engine's working set in internal RAM (bench v6: up to 2x faster).
constexpr int kFastEvents = 256, kFastCtls = 1024, kFastParams = 128;
unsigned char fastMem[26 * 1024] __attribute__((aligned(8)));

tc::Scheduler* sched = nullptr;
tc::Program* live = nullptr;
tc::Program* edit = nullptr;
Platform* gPlatform = nullptr;

uint32_t clockUs() { return gPlatform->getTimer()->micros(); }
void irqOff() { __asm volatile("cpsid i" ::: "memory"); }
void irqOn() { __asm volatile("cpsie i" ::: "memory"); }

// ---- interrupt-side counters ------------------------------------------------------
struct IsrStats {
    uint32_t blocks;
    uint64_t cyc;          // tidalTick, all blocks
    uint32_t cycMax;
    uint32_t noteBlocks;   // blocks that started >= 1 note
    uint64_t noteBlockCyc;
    uint32_t noteBlockCycMax;
    uint32_t notes;
    uint64_t noteOnCyc;    // inside instrumentPlayer.noteOn only
    uint32_t noteOnCycMax;
    uint32_t offs;
    uint32_t maxStarts;    // note starts in one block
    uint32_t late[4];      // 0, 1, 2, 3+ blocks late
};
IsrStats isr;
volatile bool isrReset = false;
volatile bool running = false;
uint32_t blockStarts = 0;

struct DeviceVoice : tc::Voice {
    void noteOn(int slot, const Entry& e, uint32_t late) override {
        if (slot >= audioEngine::kVoices) return;
        const uint32_t t0 = CYCLES();
        instrumentPlayer[slot].noteOn(e.chan, (int8_t)e.note, e.vel == 255 ? -1 : (int8_t)e.vel, 0, 0, 0, 0);
        const uint32_t c = CYCLES() - t0;
        isr.noteOnCyc += c;
        if (c > isr.noteOnCycMax) isr.noteOnCycMax = c;
        isr.notes++;
        isr.late[late < 3 ? late : 3]++;
        blockStarts++;
    }
    void noteOff(int slot, uint8_t, uint8_t) override {
        if (slot >= audioEngine::kVoices) return;
        instrumentPlayer[slot].noteOff();
        isr.offs++;
    }
    void allOff() override {
        for (int i = 0; i < audioEngine::kVoices; i++) instrumentPlayer[i].noteOff();
    }
};
DeviceVoice voice;

// ---- mapping (a first cut of step 4c) -------------------------------------------
int nameMatches(const char* name, const char* file) {
    int i = 0;
    for (; name[i]; i++)
        if (tolower((unsigned char)name[i]) != tolower((unsigned char)file[i])) return 0;
    return file[i] == '.' || file[i] == 0;
}

int findInstrument(const char* name) {
    for (int i = 0; i < kSampleInstrumentsCount; i++)
        if (mtProject.instrument[i].isActive && nameMatches(name, mtProject.instrument[i].sample.file_name)) return i;
    int k = -1;
    for (int i = 0; i < 48 && k < 0; i++)
        if (!strcmp(name, kDefaultNames[i])) k = i;
    if (k < 0) {  // unknown name: a stable pick from its letters
        unsigned h = 0;
        for (const char* c = name; *c; c++) h = h * 31 + (unsigned char)*c;
        k = (int)(h % 48);
    }
    if (mtProject.instrument[k].isActive) return k;
    int active[kSampleInstrumentsCount], n = 0;
    for (int i = 0; i < kSampleInstrumentsCount; i++)
        if (mtProject.instrument[i].isActive) active[n++] = i;
    return n ? active[k % n] : -1;
}

struct DeviceMapper : tc::Mapper {
    int keyS = -1, keySound = -1, keyN = -1, keyNote = -1, keyGain = -1;
    uint32_t unmapped = 0;
    void initKeys() {
        keyS = tc::controlKey("s");
        keySound = tc::controlKey("sound");
        keyN = tc::controlKey("n");
        keyNote = tc::controlKey("note");
        keyGain = tc::controlKey("gain");
    }
    bool map(const tc::Program& p, int slot, const tc::Event& e, float, float lenSamples, Entry& out) override {
        if (slot >= audioEngine::kVoices || !e.isMap) {
            unmapped++;
            return false;
        }
        int instr = -1;
        float n = 0, gain = -1;
        for (int i = 0; i < e.nctl; i++) {
            const tc::Ctl& c = e.ctl[i];
            if (c.key == keyS || c.key == keySound) {
                char name[32];
                tc::formatCtl(p, c, name, sizeof name);
                instr = findInstrument(name);
            } else if (c.key == keyN || c.key == keyNote) {
                n += c.f;
            } else if (c.key == keyGain) {
                gain = c.f;
            }
        }
        if (instr < 0) {
            unmapped++;
            return false;
        }
        long note = kC5Note + lroundf(n);
        out.chan = (uint8_t)instr;
        out.note = (uint8_t)(note < 0 ? 0 : note > 127 ? 127 : note);
        out.vel = gain < 0 ? 255 : (uint8_t)fminf(fmaxf(gain * 100.0f, 0.0f), 100.0f);
        // looped instruments need a release; one-shots play out
        out.dur = mtProject.instrument[instr].playMode == playModeSingleShot ? 0 : (uint32_t)lenSamples;
        return true;
    }
};
DeviceMapper mapper;

// ---- main-loop side -------------------------------------------------------------
int gSet = -1;           // current set (-1: not started)
int gSetCount = kSetCount;
bool gUserSet = false;
uint32_t gSetStart = 0, gLastUpdate = 0;
uint32_t updCalls = 0, updUs = 0, updMaxUs = 0, loopGapMaxUs = 0, queries = 0;
bool fileStarted = false;
char gLine1[64];

bool save(int n) {
    FileSystem* fs = gPlatform->getFileSystem();
    auto [o, fh] = fs->openFile(kPath, fileStarted ? FileSystem::kWriteAppend : FileSystem::kWriteCreateNew);
    bool ok = o && fh;
    if (ok) {
        fs->writeFile(report, (size_t)n, fh);
        fs->closeFile(fh);
    }
    fileStarted = true;
    return ok;
}

const char* setText(int i) { return gUserSet ? userText : kSets[i]; }

// Evaluates the set's blocks (`dN $`: switch at the next cycle); slots it
// doesn't use are silenced. Errors go into the report.
int loadSet(int set, int n) {
    const char* text = setText(set);
    tc::Error e, ce;
    edit->clear();
    if (!tc::parseFile(*edit, text, (int)strlen(text), e)) {
        int line, col;
        tc::errorLineCol(text, e.pos, line, col);
        return n + snprintf(report + n, sizeof report - n, "parse error line %d: %s\n", line, e.msg);
    }
    if (!tc::checkProgram(*edit, ce)) return n + snprintf(report + n, sizeof report - n, "type error: %s\n", ce.msg);
    if (edit->cpsExpr) {
        tc::Event* buf = reinterpret_cast<tc::Event*>(bufRaw);
        tc::Error qe;
        int q = tc::queryPattern(*edit, edit->cpsExpr, tc::Arc{tc::Q{0, 1}, tc::Q{1, 1}}, buf, kQueryCap, qe);
        if (q > 0 && buf[0].v.t == tc::VT::Num) {
            const tc::Val& v = buf[0].v;
            sched->setCps(v.exact ? v.q : tc::qmake(llroundf(v.fl * 100000.0f), 100000));
        }
    }
    for (int i = 0; i < tc::kSlots; i++) {
        tc::Error err;
        if (edit->block[i]) {
            if (!sched->evaluate(*edit, i, err))
                n += snprintf(report + n, sizeof report - n, "d%d: %s\n", i + 1, err.msg);
        } else if (sched->playing(i)) {
            sched->silence(i);
        }
    }
    return n;
}

void start() {
    sched = new (schedRaw) tc::Scheduler();
    live = new (liveRaw) tc::Program();
    edit = new (editRaw) tc::Program();
    tc::Event* buf = reinterpret_cast<tc::Event*>(bufRaw);
    for (int i = 0; i < kQueryCap; i++) new (&buf[i]) tc::Event();
    live->clear();
    if (tc::engineMemoryBytes(kFastEvents, kFastCtls, kFastParams) <= (int)sizeof fastMem)
        tc::engineUseMemory(fastMem, kFastEvents, kFastCtls, kFastParams);
    mapper.initKeys();
    tc::engineResetMinStack();
    sched->init(live, buf, kQueryCap, &mapper, tc::Scheduler::Hooks{irqOff, irqOn});
    sched->setSampleRate(750000, 17);  // 180 MHz * 16 / 255 / 256

    FileSystem* fs = gPlatform->getFileSystem();
    if (fs->exists(kUserSet)) {
        auto [o, fh] = fs->openFile(kUserSet, FileSystem::kRead);
        if (o && fh) {
            int32_t r = fs->readFile(userText, sizeof userText - 1, fh);
            fs->closeFile(fh);
            if (r > 0) {
                userText[r] = 0;
                gUserSet = true;
                gSetCount = 1;
            }
        }
    }
    memset(&isr, 0, sizeof isr);
    running = true;
}

int header(int n) {
    int active = 0;
    for (int i = 0; i < kSampleInstrumentsCount; i++) active += mtProject.instrument[i].isActive ? 1 : 0;
    return n + snprintf(report + n, sizeof report - n,
                        "Tracker Cycles device tick test v1 (%s), %s, %d s per set\n"
                        "%d active instruments; scheduler object %u B, live program %u B (SDRAM); "
                        "engine working set in internal RAM: %s\n"
                        "cycles at 180 MHz (1 us = 180); audio block = 2902 us = 522,353 cycles\n\n",
#ifdef TC_ENGINE_O2
                        "engine -O2",
#else
                        "engine -Os",
#endif
                        gUserSet ? kUserSet : "built-in sets", (int)((gUserSet ? kFileSetMs : kSetMs) / 1000), active,
                        (unsigned)sizeof(tc::Scheduler), (unsigned)sizeof(tc::Program),
                        tc::engineMemoryBytes(kFastEvents, kFastCtls, kFastParams) <= (int)sizeof fastMem ? "yes"
                                                                                                         : "no");
}

int setReport(int n, uint32_t ms) {
    IsrStats s;
    irqOff();
    s = isr;
    irqOn();
    Profiler::AudioProcessorUsage u{};
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
    const tc::SchedulerStats& st = sched->stats();
    auto avg = [](uint64_t sum, uint32_t cnt) { return (unsigned long)(cnt ? sum / cnt : 0); };
    n += snprintf(report + n, sizeof report - n,
                  "tick: %lu blocks, avg %lu cycles, max %lu; %lu blocks with note starts: avg %lu, max %lu\n"
                  "noteOn: %lu, avg %lu cycles, max %lu; noteOffs %lu; most starts in one block %lu\n"
                  "late (blocks): 0: %lu, 1: %lu, 2: %lu, 3+: %lu; dropped %lu, capped %lu, unmapped %lu\n"
                  "main loop: update() %lu calls, %lu ms in %lu ms (%lu.%lu %%), longest call %lu us; "
                  "longest gap between main-loop passes %lu us\n"
                  "scheduler: queries %lu, window splits %lu, horizon skips %lu, window %ld/%ld cycle\n"
                  "audio load: avg %d %%, max %d %%\n",
                  (unsigned long)s.blocks, avg(s.cyc, s.blocks), (unsigned long)s.cycMax, (unsigned long)s.noteBlocks,
                  avg(s.noteBlockCyc, s.noteBlocks), (unsigned long)s.noteBlockCycMax, (unsigned long)s.notes,
                  avg(s.noteOnCyc, s.notes), (unsigned long)s.noteOnCycMax, (unsigned long)s.offs,
                  (unsigned long)s.maxStarts, (unsigned long)s.late[0], (unsigned long)s.late[1],
                  (unsigned long)s.late[2], (unsigned long)s.late[3], (unsigned long)st.dropped,
                  (unsigned long)st.capped, (unsigned long)mapper.unmapped, (unsigned long)updCalls,
                  (unsigned long)(updUs / 1000), (unsigned long)ms, (unsigned long)(ms ? updUs / 10 / ms : 0),
                  (unsigned long)(ms ? updUs / ms % 10 : 0), (unsigned long)updMaxUs, (unsigned long)loopGapMaxUs,
                  (unsigned long)(st.queries - queries), (unsigned long)st.splits, (unsigned long)st.skips,
                  (long)sched->window().n, (long)sched->window().d, u.average, u.tempMaximum);
    const uintptr_t top = (uintptr_t)&_estack, brk = (uintptr_t)__brkval, minSp = tc::engineMinStack();
    n += snprintf(report + n, sizeof report - n,
                  "stack: evaluator deepest %lu B below the top of RAM; heap end %lu B below: %ld B to spare\n",
                  (unsigned long)(top - minSp), (unsigned long)(top - brk), (long)minSp - (long)brk);
    for (int i = 0; i < tc::kSlots; i++)
        if (const tc::Error* e = sched->error(i)) n += snprintf(report + n, sizeof report - n, "d%d failed: %s\n", i + 1, e->msg);
    return n + snprintf(report + n, sizeof report - n, "\n");
}

void resetCounters() {
    isrReset = true;
    updCalls = updUs = updMaxUs = loopGapMaxUs = 0;
    mapper.unmapped = 0;
    queries = sched->stats().queries;
    Profiler::getInstance()->resetMeasuringAudio();
}

void beginSet(int set, int n) {
    gSet = set;
    n += snprintf(report + n, sizeof report - n, "== set %d of %d\n%s", set + 1, gSetCount, setText(set));
    if (report[n - 1] != '\n') n += snprintf(report + n, sizeof report - n, "\n");
    n = loadSet(set, n);
    n += snprintf(report + n, sizeof report - n, "--\n");
    save(n);
    resetCounters();
    gSetStart = gPlatform->getTimer()->millis();
    snprintf(gLine1, sizeof gLine1, "Tidal tick test: set %d of %d", set + 1, gSetCount);
    mtPopups.showInfoPopup(gLine1, "playing (report: /tidal_tick.txt)");
}

}  // namespace

void tidalTick() {
    if (!running) return;
    const uint32_t t0 = CYCLES();
    if (isrReset) {
        memset(&isr, 0, sizeof isr);
        isrReset = false;
    }
    blockStarts = 0;
    sched->tick(voice);
    const uint32_t c = CYCLES() - t0;
    isr.blocks++;
    isr.cyc += c;
    if (c > isr.cycMax) isr.cycMax = c;
    if (blockStarts) {
        isr.noteBlocks++;
        isr.noteBlockCyc += c;
        if (c > isr.noteBlockCycMax) isr.noteBlockCycMax = c;
        if (blockStarts > isr.maxStarts) isr.maxStarts = blockStarts;
    }
}

void tidalTickUpdate(Platform* platform) {
    gPlatform = platform;
    const uint32_t ms = platform->getTimer()->millis();
    if (gSet < 0) {
        if (ms < kStartMs) return;
        start();
        beginSet(0, header(0));
        gLastUpdate = clockUs();
    }
    if (gSet >= gSetCount) return;  // done

    const uint32_t t0 = clockUs();
    const uint32_t gap = t0 - gLastUpdate;
    if (gap > loopGapMaxUs) loopGapMaxUs = gap;
    sched->update(clockUs, kUpdateBudgetUs);
    const uint32_t t1 = clockUs();
    gLastUpdate = t1;
    updCalls++;
    updUs += t1 - t0;
    if (t1 - t0 > updMaxUs) updMaxUs = t1 - t0;

    const uint32_t setMs = gUserSet ? kFileSetMs : kSetMs;
    if (ms - gSetStart < setMs) return;
    int n = setReport(0, ms - gSetStart);
    if (gSet + 1 < gSetCount) {
        beginSet(gSet + 1, n);
    } else {
        sched->hush();
        save(n);
        gSet = gSetCount;
        mtPopups.showInfoPopup("Tidal tick test done", "report: /tidal_tick.txt");
    }
    gLastUpdate = clockUs();
}
#endif
