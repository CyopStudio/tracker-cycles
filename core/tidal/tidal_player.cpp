// See tidal_player.h.
#include "tidal_player.h"

#ifdef TC_TIDAL_PLAYER
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <new>

#include "Platform.h"
#include "Profiler.h"
#include "fileManager.h"
#include "cr_section_macros.h"
#include "mtAudioEngine.h"
#include "mtMidi.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "tidal_config.h"
#include "../../tidal/core/check.h"
#include "../../tidal/core/parse.h"
#include "../../tidal/core/scheduler.h"
#include "tidal_device.h"

namespace {

// ---- memory (as the tick test: SDRAM isn't up for static constructors) --------
constexpr int kQueryCap = 256;
unsigned char schedRaw[sizeof(tc::Scheduler)] __attribute__((aligned(8)));  // the interrupt reads it
__NOINIT(EXTERNAL_RAM) unsigned char storageRaw[sizeof(tc::Entry) * tc::Scheduler::kStorageEntries]
    __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char liveRaw[sizeof(tc::Program)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char editRaw[sizeof(tc::Program)] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) unsigned char bufRaw[sizeof(tc::Event) * kQueryCap] __attribute__((aligned(8)));
__NOINIT(EXTERNAL_RAM) char fileText[16384];
// the engine's query working set in internal RAM (bench v6: up to 2x faster)
constexpr int kFastEvents = 256, kFastCtls = 1024, kFastParams = 128;
unsigned char fastMem[26 * 1024] __attribute__((aligned(8)));

constexpr uint32_t kUpdateBudgetUs = 4000;  // per main-loop pass
constexpr uint32_t kStartMs = 3000, kStartLatestMs = 60000;
constexpr int kPulsesPerCycle = 96;  // MIDI clock: 24 ppqn x 4 beats per cycle
constexpr float kSampleRate = 750000.0f / 17.0f;

tc::Scheduler* sched = nullptr;
tc::Program* live = nullptr;
tc::Program* edit = nullptr;
Platform* gPlatform = nullptr;
DeviceHost host;
DeviceVoice voice;
tc::TrackerMapper mapper;

bool gStarted = false;
char gMessage[96] = "";
// Tracker-mode problems found at evaluate (the scheduler keeps its own errors)
char gSlotError[tc::kSlots][96];
uint32_t gInstrumentSig = 0;
uint32_t gLastSigCheckMs = 0;
uint32_t gLastSnapMs = 0;

uint32_t clockUs() { return gPlatform->getTimer()->micros(); }
void irqOff() { __asm volatile("cpsid i" ::: "memory"); }
void irqOn() { __asm volatile("cpsie i" ::: "memory"); }

// ---- MIDI clock out --------------------------------------------------------------
// The main loop publishes where Tidal's cycle clock is (snapshot: a sample, the
// clock pulse position there, pulses per sample); the interrupt extrapolates
// and sends a pulse whenever it crosses one, with its offset inside the block.
struct ClockSnap {
    int64_t s0;
    float pulse0;
    float pulsesPerSample;
};
ClockSnap gSnap[2];
volatile uint8_t gSnapIdx = 0;     // the one the interrupt reads
volatile bool gClockArmed = false;  // send Start at gStartPulse
volatile bool gClockRunning = false;
volatile int32_t gStartPulse = 0;
int32_t gLastPulse = 0;             // interrupt only

void publishSnap() {
    const int64_t s0 = (int64_t)(sched->block() + 1) * tc::kBlockSamples;
    const tc::Q c = sched->cycleAt(s0);
    const tc::Q cps = sched->cps();
    ClockSnap& n = gSnap[gSnapIdx ^ 1];
    n.s0 = s0;
    n.pulse0 = (float)((double)c.n * kPulsesPerCycle / c.d);
    n.pulsesPerSample = (float)cps.n / (float)cps.d * kPulsesPerCycle / kSampleRate;
    gSnapIdx = (uint8_t)(gSnapIdx ^ 1);
}

float pulseAt(const ClockSnap& s, int64_t sample) { return s.pulse0 + (float)(sample - s.s0) * s.pulsesPerSample; }

// ---- project / evaluate -------------------------------------------------------------
uint32_t instrumentSignature() {
    uint32_t h = 2166136261u;
    for (int i = 0; i < kSampleInstrumentsCount; i++) {
        const strInstrument& ins = mtProject.instrument[i];
        h = (h ^ (uint32_t)ins.isActive) * 16777619u;
        if (!ins.isActive) continue;
        for (const char* c = ins.sample.file_name; *c; c++) h = (h ^ (uint8_t)*c) * 16777619u;
        h = (h ^ ins.playMode) * 16777619u;
    }
    return h;
}

int activeInstruments() {
    int n = 0;
    for (int i = 0; i < kSampleInstrumentsCount; i++) n += mtProject.instrument[i].isActive == 1;
    return n;
}

void transportPlay() { tidalPlayerReload(); }
void transportStop() { tidalPlayerHush(); }
bool transportPlaying() { return tidalPlayerAnyPlaying(); }

void start() {
    sched = new (schedRaw) tc::Scheduler();
    live = new (liveRaw) tc::Program();
    edit = new (editRaw) tc::Program();
    tc::Event* buf = reinterpret_cast<tc::Event*>(bufRaw);
    for (int i = 0; i < kQueryCap; i++) new (&buf[i]) tc::Event();
    if (tc::engineMemoryBytes(kFastEvents, kFastCtls, kFastParams) <= (int)sizeof fastMem)
        tc::engineUseMemory(fastMem, kFastEvents, kFastCtls, kFastParams);
    mapper.init(&host, kSampleRate);
    sched->init(live, buf, kQueryCap, &mapper, reinterpret_cast<tc::Entry*>(storageRaw),
                tc::Scheduler::Hooks{irqOff, irqOn});
    sched->setSampleRate(750000, 17);  // 180 MHz * 16 / 255 / 256
    sched->setSlotPlayers(DeviceVoice::kPlayers);
    memset(gSlotError, 0, sizeof gSlotError);
    gInstrumentSig = instrumentSignature();
    publishSnap();
    sequencer.setTransport(transportPlay, transportStop, transportPlaying);
    gStarted = true;
}

void setTempoFromCps() {
    const tc::Q cps = sched->cps();
    sequencer.setTempo((float)cps.n / (float)cps.d * 240.0f);  // one cycle = one 4-beat bar
}

}  // namespace

// ---- API ----------------------------------------------------------------------------

bool tidalPlayerReload() {
    if (!gStarted) return false;
    gMessage[0] = 0;
    FileSystem* fs = gPlatform->getFileSystem();
    if (!fs->exists(kTidalPlayerFile)) {
        snprintf(gMessage, sizeof gMessage, "no %s on the card", kTidalPlayerFile);
        return false;
    }
    auto [ok, fh] = fs->openFile(kTidalPlayerFile, FileSystem::kRead);
    int32_t len = (ok && fh) ? fs->readFile(fileText, sizeof fileText - 1, fh) : -1;
    if (ok && fh) fs->closeFile(fh);
    if (len < 0) {
        snprintf(gMessage, sizeof gMessage, "can't read %s", kTidalPlayerFile);
        return false;
    }
    if (len >= (int32_t)sizeof fileText - 1) {
        snprintf(gMessage, sizeof gMessage, "%s is over %u bytes", kTidalPlayerFile, (unsigned)sizeof fileText - 1);
        return false;
    }
    fileText[len] = 0;

    tc::Error e, ce;
    edit->clear();
    if (!tc::parseFile(*edit, fileText, len, e)) {
        int line, col;
        tc::errorLineCol(fileText, e.pos, line, col);
        snprintf(gMessage, sizeof gMessage, "line %d: %s", line, e.msg);
        return false;
    }
    if (!tc::checkProgram(*edit, ce)) {
        snprintf(gMessage, sizeof gMessage, "%s", ce.msg);
        return false;
    }
    if (edit->cpsExpr) {
        tc::Event* buf = reinterpret_cast<tc::Event*>(bufRaw);
        tc::Error qe;
        int q = tc::queryPattern(*edit, edit->cpsExpr, tc::Arc{tc::Q{0, 1}, tc::Q{1, 1}}, buf, kQueryCap, qe);
        if (q > 0 && buf[0].v.t == tc::VT::Num) {
            const tc::Val& v = buf[0].v;
            sched->setCps(v.exact ? v.q : tc::qmake(llroundf(v.fl * 100000.0f), 100000));
        }
    }
    const bool wasPlaying = tidalPlayerAnyPlaying();
    for (int i = 0; i < tc::kSlots; i++) {
        gSlotError[i][0] = 0;
        if (edit->block[i]) {
            tc::Error te;
            if (!tc::trackerCheck(*edit, i, te)) {  // Tracker mode: keeps what played before
                snprintf(gSlotError[i], sizeof gSlotError[i], "%s", te.msg);
                continue;
            }
            tc::Error err;
            if (!sched->evaluate(*edit, i, err)) snprintf(gSlotError[i], sizeof gSlotError[i], "%s", err.msg);
        } else if (sched->playing(i)) {
            sched->silence(i);
        }
    }
    setTempoFromCps();
    if (!wasPlaying && !gClockRunning) {
        // MIDI Start on the next cycle boundary (where `dN $` switches)
        publishSnap();
        const ClockSnap& s = gSnap[gSnapIdx];
        const int32_t now = (int32_t)floorf(s.pulse0);
        gStartPulse = (now / kPulsesPerCycle + 1) * kPulsesPerCycle;
        gClockArmed = true;
    }
    return true;
}

void tidalPlayerHush() {
    if (!gStarted) return;
    sched->hush();
    gClockArmed = false;
    if (gClockRunning) {
        gClockRunning = false;
        sendMidiStop();
    }
}

bool tidalPlayerStarted() { return gStarted; }
bool tidalPlayerAnyPlaying() {
    if (!gStarted) return false;
    for (int i = 0; i < tc::kSlots; i++)
        if (sched->playing(i)) return true;
    return false;
}
const char* tidalPlayerMessage() { return gMessage; }
bool tidalPlayerSlotPlaying(int slot) { return gStarted && sched->playing(slot); }
const char* tidalPlayerSlotError(int slot) {
    if (!gStarted) return nullptr;
    if (gSlotError[slot][0]) return gSlotError[slot];
    const tc::Error* e = sched->error(slot);
    return e ? e->msg : nullptr;
}
const char* tidalPlayerSlotWarning(int slot, char* buf, int len) {
    if (!gStarted) return nullptr;
    const tc::MapWarnings& w = mapper.warnings(slot);
    if (w.unknownName) snprintf(buf, len, "no instrument named '%s'", w.lastUnknown);
    else if (w.noSound) snprintf(buf, len, "events without s / sound");
    else if (w.droppedMods) snprintf(buf, len, "too many controls on one note");
    else if (w.filterClash) snprintf(buf, len, "more than one filter on a note");
    else return nullptr;
    return buf;
}
float tidalPlayerCps() {
    if (!gStarted) return 0;
    const tc::Q c = sched->cps();
    return (float)c.n / (float)c.d;
}
int tidalPlayerAudioLoad() { return voice.loadPercent(); }
int tidalPlayerSounding() {
    int n = 0;
    for (int t = 0; t < DeviceVoice::kTracks; t++)
        for (int k = 0; k < DeviceVoice::kPlayers; k++) n += instrumentPlayer[t].tidalPlaying((uint8_t)k);
    return n;
}
int tidalPlayerCap() { return voice.effectiveCap(); }

void tidalPlayerUpdate(Platform* platform) {
    gPlatform = platform;
    const uint32_t ms = platform->getTimer()->millis();
    if (!gStarted) {
        // wait for the project's instruments (loading takes a while): some
        // loaded and the file manager done, or the latest start time
        const bool loaded = activeInstruments() && newFileManager.getCurrentOperation() == fmNoOperation;
        if (ms < kStartMs || (ms < kStartLatestMs && !loaded)) return;
        start();
        tidalPlayerReload();  // play the card's file at start-up
        return;
    }
    sched->update(clockUs, kUpdateBudgetUs);
    if (ms - gLastSnapMs >= 20) {  // the MIDI clock's view of the cycle clock (soft double: not every pass)
        gLastSnapMs = ms;
        publishSnap();
    }
    if (ms - gLastSigCheckMs > 1000) {  // another project / instrument loaded: drop the name cache
        gLastSigCheckMs = ms;
        const uint32_t sig = instrumentSignature();
        if (sig != gInstrumentSig) {
            gInstrumentSig = sig;
            mapper.instrumentsChanged();
        }
    }
}

void tidalPlayerTick() {
    if (!gStarted) return;
    voice.blockLoad(Profiler::getInstance()->lastAudioUs());
    sched->tick(voice);

    // MIDI clock out
    if (!gClockArmed && !gClockRunning) return;
    const ClockSnap& s = gSnap[gSnapIdx];
    const int64_t b0 = (int64_t)(sched->block() - 1) * tc::kBlockSamples;  // the block tick() just played
    const float p1 = pulseAt(s, b0 + tc::kBlockSamples);
    if (gClockArmed) {
        if (p1 < (float)gStartPulse) return;
        gClockArmed = false;
        gClockRunning = true;
        gLastPulse = gStartPulse - 1;
        sendMidiStart();
    }
    const int32_t last = (int32_t)floorf(p1);
    if (last - gLastPulse > 8) gLastPulse = last - 1;  // a jump (cps change, stall): don't burst
    while (gLastPulse < last) {
        gLastPulse++;
        const float atSample = (float)(s.s0 - b0) + ((float)gLastPulse - s.pulse0) / s.pulsesPerSample;
        const float us = atSample < 0 ? 0 : atSample / kSampleRate * 1e6f;
        midiSetClockSubBlockOffsetUs((uint32_t)us);
        sendMidiClock();
    }
}
#endif
