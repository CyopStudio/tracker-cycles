// Device tick test build (see tidal_tick.h and README.md).
//
// v11: Program::clear() is O(1) (high-water mark; it zeroed the 70 KB
// SDRAM program on every parse): compare the "evaluate: parse" lines.
// v10: load-driven cap (max 16, down to 8 as the smoothed audio load goes
// 70 -> 85 %; over it at a noteOn the oldest fade); sets 6-8 = chords,
// everyday and heavy with voices 3 on every track, governed, solo.
// v9: the scheduler supersedes by each start's own `voices` (v8 let 3
// starts per block through for voices-1 tracks: heavy set late), the
// streamed-instrument path respects the cap (v8: 14 sounding at cap 12),
// default cap 14; sets 6-8 caps 12 / 14 / 16, sets 9-10 everyday with
// voices 3 at caps 12 / 14.
// v8: `voices` (default 1: each note cuts the last), cap fix (just-started
// players count as sounding), steals fade out (5 ms) instead of stopping;
// sets 6-8 measure the load at caps 8 / 12 / 16, set 9 everyday with
// voices 3; the 8 single-control listening sets are dropped (v5-v7).
// v7: polyphony (docs/tidal_voices.md): 3 sample players per track into the
// track's chain, at most 16 sounding; set 6 overfills the cap (8 tracks of
// ringing chords), sets 15-17 are polyphony listening sets.
// v6: audio floor work (idle voices, muted sends, silent delay / reverb
// send no blocks); set 1 = 10 s of silence to measure the floor.
// v5: listening sets one track at a time (solo, with a hint popup);
// checker in SDRAM, 32 KB stack reserve (v4 left 1.9 KB of stack).
// v4: the real control mapping (plan 4c): tidal/core/mapping.* on the main
// loop, playerEngine::tidalNoteOn and MIDI out (tidal_device.*) in the
// interrupt; sets 5-6 are listening tests (sample controls, MIDI).
// v2-v3: scheduler object in internal RAM, mono sample slots, per-slot
// query timing, update() early exit; waits for a loaded project.
//
// Once a project with instruments is loaded (15 s after boot at the
// earliest, 60 s at the latest) it plays a list of test sets (built in, or
// the one in /tidal_tick.tidal if the card has it), 20 s each, on the
// project's instruments: `s` picks the instrument whose file name matches
// (case-insensitive, without extension); for this test a name with no
// match falls back to its position in the prototype's default list (bd =
// 1, sd = 2, ...), wrapped onto the active instruments, so any project
// makes sound. MIDI (d9-d16) goes out on the configured MIDI ports.
//
// After each set it appends to /tidal_tick.txt: interrupt cost of
// tidalTick per block and per noteOn (with its overrides), note starts per
// block, lateness, the scheduler's and the mapper's counters, the
// main-loop time spent in update() and the audio load.
#ifdef TIDAL_TICK
#include "tidal_tick.h"

#include <ctype.h>
#include <malloc.h>
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
#include "tidal_device.h"

extern "C" {
extern char* __brkval;        // heap end (teensy core)
extern unsigned long _estack;  // top of RAM
}

namespace {

using tc::Entry;

#define CYCLES() (*(volatile uint32_t*)0xE0001004u)  // DWT->CYCCNT

constexpr uint32_t kStartMs = 15000, kStartLatestMs = 60000, kFileSetMs = 60000;
constexpr uint32_t kUpdateBudgetUs = 4000;
#ifdef TC_ENGINE_O2
#define TC_TICK_VARIANT "engine -O2"
const char* const kPath = "/tidal_tick_o2.txt";
#else
#define TC_TICK_VARIANT "engine -Os"
const char* const kPath = "/tidal_tick.txt";
#endif
const char* const kUserSet = "/tidal_tick.tidal";

// Built-in sets. Names resolve onto any project (see the top). `solo`:
// hush everything first (listening sets: only this set's tracks sound; it
// starts at the next cycle, up to ~1.7 s later at cps 0.575).
struct TestSet {
    const char* text;
    uint16_t seconds;
    bool solo;
    const char* hint;  // popup line 2: what to listen for
    uint8_t cap = 0;   // polyphony cap for this set (0 = DeviceVoice::kCap)
};
const TestSet kSets[] = {
    {"", 10, true, "silence: audio floor, nothing plays"},
    // timing, default voices 1 (each note cuts the last, as v6)
    {"d1 $ s \"bd*4\"\n"
     "d2 $ s \"~ sd\"\n"
     "d3 $ s \"hh*8\" # gain 0.8\n",
     20, false, "timing: light, 3 tracks"},
    {"d1 $ s \"bd*2 [~ bd] bd\"\n"
     "d2 $ s \"~ sd\" # n \"<0 1>\"\n"
     "d3 $ every 2 (fast 2) $ s \"hh*8\"\n"
     "d4 $ n \"0 .. 7\" # s \"arpy\"\n"
     "d5 $ off 0.125 (|+ n 12) $ n \"0 3 7\" # s \"bass\"\n"
     "d6 $ s \"cp*2\" # pan sine\n"
     "d7 $ sometimes (# speed 2) $ s \"drum*4\"\n"
     "d8 $ s \"<lt mt ht>*3\"\n",
     20, false, "timing: everyday, 8 tracks"},
    {"setcps 0.7\n"
     "d1 $ stut 8 0.5 0.0625 $ s \"hh*16\" # n \"0 .. 15\"\n"
     "d2 $ jux rev $ chop 8 $ s \"bd*8 sd*8\" # speed \"1 2\"\n"
     "d3 $ echo 8 0.125 0.7 $ s \"bd(5,8) sd(3,8,2)\" # n (irand 8)\n"
     "d4 $ off 0.125 (|+ n 12) $ off 0.25 (|+ n 7) $ n (scale \"major\" \"0 .. 7\") # s \"arpy\"\n"
     "d5 $ chunk 4 (hurry 2) $ iter 4 $ s \"bd sd hh cp hh sd bd cp\" # n \"0 .. 7\"\n"
     "d6 $ s \"hh*16\" # n \"0 .. 15\" # pan sine # gain (range 0.8 1 saw)\n"
     "d7 $ sometimes (# speed 2) $ often (# crush 4) $ s \"[bd*4, hh*8, ~ sd]\"\n"
     "d8 $ n (arp \"up\" \"<c'maj e'min7 g'dom7 a'min>*2\") # s \"bass\" # legato 0.5\n",
     20, false, "timing: heavy, 8 tracks"},
    {"setcps 0.575\n"
     "d1 $ s \"bd*16\"\n"
     "d2 $ s \"sd*16\"\n"
     "d3 $ s \"hh*16\"\n"
     "d4 $ s \"cp*16\"\n"
     "d5 $ s \"ho*16\"\n"
     "d6 $ s \"cr*16\"\n"
     "d7 $ s \"lt*16\"\n"
     "d8 $ s \"mt*16\"\n",
     20, false, "timing: all 8 tracks together"},
    // timing: polyphony under the load-driven cap (max 16), each hushed
    // first (solo) so the last set's tails don't carry over
    {"setcps 0.575\n"
     "d1 $ n \"<c'maj e'min>*2\" # s \"arpy\" # legato 3 # voices 3\n"
     "d2 $ n \"<a'min f'maj>*2\" # s \"bass\" # legato 3 # voices 3\n"
     "d3 $ n \"<e'min c'maj>*2\" # s \"pluck\" # legato 3 # voices 3\n"
     "d4 $ n \"<g'maj d'min>*2\" # s \"casio\" # legato 3 # voices 3\n"
     "d5 $ n \"<c'maj7 a'min7>*2\" # s \"jvbass\" # legato 3 # voices 3\n"
     "d6 $ n \"<f'maj g'maj>*2\" # s \"bev\" # legato 3 # voices 3\n"
     "d7 $ n \"<d'min e'min>*2\" # s \"bass1\" # legato 3 # voices 3\n"
     "d8 $ n \"<a'min c'maj>*2\" # s \"bass2\" # legato 3 # voices 3\n",
     20, true, "timing: governed cap, 8 tracks of ringing chords (voices 3)"},
    {"d1 $ s \"bd*2 [~ bd] bd\" # voices 3\n"
     "d2 $ s \"~ sd\" # n \"<0 1>\" # voices 3\n"
     "d3 $ every 2 (fast 2) $ s \"hh*8\" # voices 3\n"
     "d4 $ n \"0 .. 7\" # s \"arpy\" # voices 3\n"
     "d5 $ off 0.125 (|+ n 12) $ n \"0 3 7\" # s \"bass\" # voices 3\n"
     "d6 $ s \"cp*2\" # pan sine # voices 3\n"
     "d7 $ sometimes (# speed 2) $ s \"drum*4\" # voices 3\n"
     "d8 $ s \"<lt mt ht>*3\" # voices 3\n",
     20, true, "timing: governed cap, everyday with voices 3 on every track"},
    {"setcps 0.7\n"
     "d1 $ stut 8 0.5 0.0625 $ s \"hh*16\" # n \"0 .. 15\" # voices 3\n"
     "d2 $ jux rev $ chop 8 $ s \"bd*8 sd*8\" # speed \"1 2\" # voices 3\n"
     "d3 $ echo 8 0.125 0.7 $ s \"bd(5,8) sd(3,8,2)\" # n (irand 8) # voices 3\n"
     "d4 $ off 0.125 (|+ n 12) $ off 0.25 (|+ n 7) $ n (scale \"major\" \"0 .. 7\") # s \"arpy\" # voices 3\n"
     "d5 $ chunk 4 (hurry 2) $ iter 4 $ s \"bd sd hh cp hh sd bd cp\" # n \"0 .. 7\" # voices 3\n"
     "d6 $ s \"hh*16\" # n \"0 .. 15\" # pan sine # gain (range 0.8 1 saw) # voices 3\n"
     "d7 $ sometimes (# speed 2) $ often (# crush 4) $ s \"[bd*4, hh*8, ~ sd]\" # voices 3\n"
     "d8 $ n (arp \"up\" \"<c'maj e'min7 g'dom7 a'min>*2\") # s \"bass\" # legato 0.5 # voices 3\n",
     20, true, "timing: governed cap, heavy with voices 3 on every track"},
    // listening: polyphony
    {"d4 $ n \"<c'maj e'min a'min f'maj>\" # s \"arpy\" # legato 2 # voices 3\n", 10, true,
     "poly d4: whole 3-note chords, each rings into the next"},
    {"d5 $ n \"0 3 7 10\" # s \"bass\" # legato 2 # voices 3\n", 10, true,
     "poly d5: bass notes overlap, tails under the next"},
    {"d3 $ echo 3 0.2 0.6 $ n \"<0 7>\" # s \"pluck\" # legato 1 # voices 3\n", 10, true,
     "poly d3: echoes ring over each other"},
    {"d4 $ n \"<c'maj e'min>\" # s \"arpy\" # legato 2 # voices \"<1 3>\"\n", 10, true,
     "voices d4: one note (voices 1), then a whole chord (voices 3)"},
    // listening: MIDI (copy docs/tidal_tick_midi.tidal to the card to run this alone)
    {"d1 $ s \"bd*4\"\n"
     "d9 $ n \"0 4 7 12\" # velocity \"0.5 1\" # legato 0.5\n"
     "d10 $ n \"<c'maj e'min>\" # midichan 1 # ccn 74 # ccv (range 0 127 sine) # legato 1\n",
     20, true, "MIDI: ch1 C E G C, ch2 chords + CC74"},
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
// The scheduler object in internal RAM (the interrupt reads its ring heads
// / tails and noteOff tables every block), its entries in SDRAM.
unsigned char schedRaw[sizeof(tc::Scheduler)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char storageRaw[sizeof(tc::Entry) * tc::Scheduler::kStorageEntries]
    __attribute__((aligned(8)));
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
    uint32_t idleCycMax;   // blocks without note starts
    uint32_t notes;
    uint64_t noteOnCyc;    // inside instrumentPlayer.noteOn only
    uint32_t noteOnCycMax;
    uint32_t offs;
    uint32_t maxStarts;    // note starts in one block
    uint32_t late[4];      // 0, 1, 2, 3+ blocks late
    uint32_t mods;         // overrides carried by the notes played
};
IsrStats isr;
volatile bool isrReset = false;
volatile bool running = false;
uint32_t blockStarts = 0;

// DeviceVoice, timed
struct TickVoice : DeviceVoice {
    uint16_t noteOn(int slot, const Entry& e, uint32_t late) override {
        const uint32_t t0 = CYCLES();
        const uint16_t tag = DeviceVoice::noteOn(slot, e, late);
        const uint32_t c = CYCLES() - t0;
        isr.noteOnCyc += c;
        if (c > isr.noteOnCycMax) isr.noteOnCycMax = c;
        isr.notes++;
        isr.mods += e.nmods;
        isr.late[late < 3 ? late : 3]++;
        if (slot < audioEngine::kVoices) blockStarts++;
        return tag;
    }
    void noteOff(int slot, uint8_t chan, uint8_t note, uint8_t offset, uint16_t tag) override {
        DeviceVoice::noteOff(slot, chan, note, offset, tag);
        isr.offs++;
    }
};
TickVoice voice;

// Test only: names no instrument file matches fall back to the prototype's
// default list (bd = 1, ...) wrapped onto the active instruments.
struct TestHost : DeviceHost {
    int fallbackInstrument(const char* name) const override {
        int k = -1;
        for (int i = 0; i < 48 && k < 0; i++)
            if (!strcmp(name, kDefaultNames[i])) k = i;
        if (k < 0) {
            unsigned h = 0;
            for (const char* c = name; *c; c++) h = h * 31 + (unsigned char)*c;
            k = (int)(h % 48);
        }
        if (mtProject.instrument[k].isActive == 1) return k;
        int active[kSampleInstrumentsCount], n = 0;
        for (int i = 0; i < kSampleInstrumentsCount; i++)
            if (mtProject.instrument[i].isActive == 1) active[n++] = i;
        return n ? active[k % n] : -1;
    }
};
TestHost host;

// TrackerMapper, timed (cycles on the main loop, interrupts included)
struct TimedMapper : tc::TrackerMapper {
    uint32_t maps = 0;
    uint64_t mapCyc = 0;
    bool map(const tc::Program& p, int slot, const tc::Event& e, float spc, float lenSamples, Entry& out) override {
        const uint32_t t0 = CYCLES();
        const bool ok = tc::TrackerMapper::map(p, slot, e, spc, lenSamples, out);
        mapCyc += CYCLES() - t0;
        maps++;
        return ok;
    }
};
TimedMapper mapper;

// ---- main-loop side -------------------------------------------------------------
int gSet = -1;           // current set (-1: not started)
int gSetCount = kSetCount;
bool gUserSet = false;
uint32_t gSetStart = 0, gLastUpdate = 0;
uint32_t updCalls = 0, updUs = 0, updMaxUs = 0, loopGapMaxUs = 0;
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

const char* setText(int i) { return gUserSet ? userText : kSets[i].text; }
uint32_t setMs(int i) { return gUserSet ? kFileSetMs : kSets[i].seconds * 1000u; }
bool setSolo(int i) { return gUserSet || kSets[i].solo; }
const char* setHint(int i) { return gUserSet ? "your /tidal_tick.tidal" : kSets[i].hint; }

// Evaluates the set's blocks (`dN $`: switch at the next cycle); slots it
// doesn't use are silenced. Errors go into the report.
int loadSet(int set, int n) {
    const char* text = setText(set);
    tc::Error e, ce;
    if (setSolo(set)) sched->hush();
    const uint32_t t0 = clockUs();
    edit->clear();
    if (!tc::parseFile(*edit, text, (int)strlen(text), e)) {
        int line, col;
        tc::errorLineCol(text, e.pos, line, col);
        return n + snprintf(report + n, sizeof report - n, "parse error line %d: %s\n", line, e.msg);
    }
    const uint32_t t1 = clockUs();
    if (!tc::checkProgram(*edit, ce)) return n + snprintf(report + n, sizeof report - n, "type error: %s\n", ce.msg);
    const uint32_t t2 = clockUs();
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
    const uint32_t t3 = clockUs();
    return n + snprintf(report + n, sizeof report - n,
                        "evaluate: parse %lu us, check %lu us (checker in SDRAM), scheduler %lu us\n",
                        (unsigned long)(t1 - t0), (unsigned long)(t2 - t1), (unsigned long)(t3 - t2));
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
    mapper.init(&host, 750000.0f / 17.0f);
    tc::engineResetMinStack();
    sched->init(live, buf, kQueryCap, &mapper, reinterpret_cast<tc::Entry*>(storageRaw),
                tc::Scheduler::Hooks{irqOff, irqOn});
    sched->setSampleRate(750000, 17);  // 180 MHz * 16 / 255 / 256
    sched->setSlotPlayers(DeviceVoice::kPlayers);  // players per sample track

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

int activeInstruments() {
    int active = 0;
    for (int i = 0; i < kSampleInstrumentsCount; i++) active += mtProject.instrument[i].isActive ? 1 : 0;
    return active;
}

int header(int n) {
    return n + snprintf(report + n, sizeof report - n,
                        "Tracker Cycles device tick test v11 (" TC_TICK_VARIANT "), %s\n"
                        "%d active instruments; scheduler object %u B (internal RAM), entries %u B, live program "
                        "%u B (SDRAM); engine working set in internal RAM: %s; %d sample players per track, cap max %d (load-driven), voices default 1\n"
                        "cycles at 180 MHz (1 us = 180); audio block = 2902 us = 522,353 cycles\n\n",
                        gUserSet ? kUserSet : "built-in sets",
                        activeInstruments(), (unsigned)sizeof(tc::Scheduler), (unsigned)sizeof storageRaw,
                        (unsigned)sizeof(tc::Program),
                        tc::engineMemoryBytes(kFastEvents, kFastCtls, kFastParams) <= (int)sizeof fastMem ? "yes"
                                                                                                         : "no",
                        DeviceVoice::kPlayers, DeviceVoice::kCap);
}

uint32_t unmapped() {
    uint32_t u = 0;
    for (int i = 0; i < tc::kSlots; i++) u += mapper.warnings(i).unknownName + mapper.warnings(i).noSound;
    return u;
}

int setReport(int n, uint32_t ms) {
    IsrStats s;
    DeviceVoice::Stats vs;
    irqOff();
    s = isr;
    vs = voice.vstats;
    irqOn();
    Profiler::AudioProcessorUsage u{};
    Profiler::getInstance()->getAudioProcessingUsagePercent(u, true);
    const tc::SchedulerStats& st = sched->stats();
    auto avg = [](uint64_t sum, uint32_t cnt) { return (unsigned long)(cnt ? sum / cnt : 0); };
    const uint32_t idle = s.blocks - s.noteBlocks;
    n += snprintf(report + n, sizeof report - n,
                  "tick: %lu blocks, avg %lu cycles, max %lu; idle blocks (no start) %lu: avg %lu, max %lu; "
                  "%lu blocks with starts: avg %lu, max %lu\n"
                  "noteOn: %lu (avg %lu.%lu overrides), avg %lu cycles, max %lu; noteOffs %lu; most starts in one block %lu\n"
                  "late (blocks): 0: %lu, 1: %lu, 2: %lu, 3+: %lu; dropped %lu, capped %lu, superseded %lu, "
                  "unmapped %lu\n"
                  "main loop: update() %lu calls, %lu ms in %lu ms (%lu.%lu %%), longest call %lu us; "
                  "longest gap between main-loop passes %lu us\n"
                  "scheduler: queries %lu, idle update() calls %lu, window splits %lu, horizon skips %lu, "
                  "window %ld/%ld cycle; "
                  "mapping %lu events, avg %lu cycles\n"
                  "audio load: avg %d %%, max %d %%\n",
                  (unsigned long)s.blocks, avg(s.cyc, s.blocks), (unsigned long)s.cycMax, (unsigned long)idle,
                  avg(s.cyc - s.noteBlockCyc, idle), (unsigned long)s.idleCycMax, (unsigned long)s.noteBlocks,
                  avg(s.noteBlockCyc, s.noteBlocks), (unsigned long)s.noteBlockCycMax, (unsigned long)s.notes,
                  avg(s.mods, s.notes), avg((uint64_t)s.mods * 10, s.notes) % 10, avg(s.noteOnCyc, s.notes), (unsigned long)s.noteOnCycMax, (unsigned long)s.offs,
                  (unsigned long)s.maxStarts, (unsigned long)s.late[0], (unsigned long)s.late[1],
                  (unsigned long)s.late[2], (unsigned long)s.late[3], (unsigned long)st.dropped,
                  (unsigned long)st.capped, (unsigned long)st.superseded, (unsigned long)unmapped(),
                  (unsigned long)updCalls, (unsigned long)(updUs / 1000), (unsigned long)ms,
                  (unsigned long)(ms ? updUs / 10 / ms : 0), (unsigned long)(ms ? updUs / ms % 10 : 0),
                  (unsigned long)updMaxUs, (unsigned long)loopGapMaxUs, (unsigned long)st.queries,
                  (unsigned long)st.idleUpdates,
                  (unsigned long)st.splits, (unsigned long)st.skips, (long)sched->window().n, (long)sched->window().d,
                  (unsigned long)mapper.maps, avg(mapper.mapCyc, mapper.maps), u.average, u.tempMaximum);
    {
        n += snprintf(report + n, sizeof report - n,
                      "players: most sounding at a noteOn %lu (cap %d), avg %lu.%lu; retriggered own track's oldest "
                      "%lu, faded by voices %lu, faded for the cap %lu; stale noteOffs %lu; audio blocks most in use "
                      "%u of 70\n",
                      (unsigned long)vs.maxSounding, voice.cap(), avg(vs.soundingSum, vs.picks),
                      avg((uint64_t)vs.soundingSum * 10, vs.picks) % 10, (unsigned long)vs.trackSteals,
                      (unsigned long)vs.voicesCuts, (unsigned long)vs.globalSteals, (unsigned long)vs.staleOffs, (unsigned)AudioMemoryUsageMax());
        n += snprintf(report + n, sizeof report - n,
                      "governor: smoothed load max %lu %% (now %d %%); cap at noteOns min %lu, avg %lu (max %d); "
                      "faded by the governor %lu\n",
                      (unsigned long)vs.loadMax, voice.loadPercent(), (unsigned long)vs.capMin,
                      avg(vs.capSum, vs.picks), voice.cap(), (unsigned long)vs.governorCuts);
    }
    for (int i = 0; i < tc::kSlots; i++) {
        const tc::MapWarnings& w = mapper.warnings(i);
        if (w.unknownName || w.noSound || w.droppedMods || w.filterClash)
            n += snprintf(report + n, sizeof report - n,
                          "d%d mapping: unknown s %lu (last '%s'), no s %lu, too many controls %lu, filter clash %lu\n",
                          i + 1, (unsigned long)w.unknownName, w.lastUnknown, (unsigned long)w.noSound,
                          (unsigned long)w.droppedMods, (unsigned long)w.filterClash);
    }
    n += snprintf(report + n, sizeof report - n, "per slot (wall time, interrupts included): windows, avg us, max us\n");
    for (int i = 0; i < tc::kSlots; i++)
        if (st.slotWindows[i])
            n += snprintf(report + n, sizeof report - n, "  d%d: %lu, %lu, %lu\n", i + 1,
                          (unsigned long)st.slotWindows[i], avg(st.slotUs[i], st.slotWindows[i]),
                          (unsigned long)st.slotUsMax[i]);
    const uintptr_t top = (uintptr_t)&_estack, brk = (uintptr_t)__brkval, minSp = tc::engineMinStack();
    {
        const struct mallinfo mi = mallinfo();
        n += snprintf(report + n, sizeof report - n, "heap: %lu B in use, %lu B free in the arena\n",
                      (unsigned long)mi.uordblks, (unsigned long)mi.fordblks);
    }
    if (minSp == UINTPTR_MAX)  // no query has run yet (silent set)
        n += snprintf(report + n, sizeof report - n,
                      "stack: evaluator not run yet; heap end %lu B below the top of RAM\n",
                      (unsigned long)(top - brk));
    else
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
    mapper.resetWarnings();
    mapper.maps = 0;
    mapper.mapCyc = 0;
    sched->resetStats();
    AudioMemoryUsageMaxReset();
    Profiler::getInstance()->resetMeasuringAudio();
}

void beginSet(int set, int n) {
    gSet = set;
    n += snprintf(report + n, sizeof report - n, "== set %d of %d (%s, %lu s%s)\n%s", set + 1, gSetCount,
                  setHint(set), (unsigned long)(setMs(set) / 1000), setSolo(set) ? ", solo" : "", setText(set));
    if (report[n - 1] != '\n') n += snprintf(report + n, sizeof report - n, "\n");
    voice.setCap(!gUserSet && kSets[set].cap ? kSets[set].cap : DeviceVoice::kCap);
    n = loadSet(set, n);
    n += snprintf(report + n, sizeof report - n, "--\n");
    save(n);
    resetCounters();
    gSetStart = gPlatform->getTimer()->millis();
    snprintf(gLine1, sizeof gLine1, "Tidal tick test: set %d of %d", set + 1, gSetCount);
    mtPopups.showInfoPopup(gLine1, setHint(set));
}

}  // namespace

void tidalTick() {
    if (!running) return;
    const uint32_t t0 = CYCLES();
    if (isrReset) {
        memset(&isr, 0, sizeof isr);
        voice.vstats = DeviceVoice::Stats{};
        isrReset = false;
    }
    blockStarts = 0;
    voice.blockLoad(Profiler::getInstance()->lastAudioUs());  // last callback's time: the governor's load
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
    } else if (c > isr.idleCycMax) {
        isr.idleCycMax = c;
    }
}

void tidalTickUpdate(Platform* platform) {
    gPlatform = platform;
    const uint32_t ms = platform->getTimer()->millis();
    if (gSet < 0) {
        // wait for the project's instruments (loading takes a while)
        if (ms < kStartMs || (ms < kStartLatestMs && !activeInstruments())) return;
        start();
        beginSet(0, header(0));
        gLastUpdate = clockUs();
        return;  // the set's clock starts now; `ms` is stale
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

    if (ms - gSetStart < setMs(gSet)) return;
    int n = setReport(0, ms - gSetStart);
    if (gSet + 1 < gSetCount) {
        beginSet(gSet + 1, n);
    } else {
        sched->hush();
        save(n);
        gSet = gSetCount;
        mtPopups.showInfoPopup("Tidal tick test done", kPath);
    }
    gLastUpdate = clockUs();
}
#endif
