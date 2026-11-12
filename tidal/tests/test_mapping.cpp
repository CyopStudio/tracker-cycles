// Control mapping tests (docs/tidal_control_mapping.md): events from real
// patterns through TrackerMapper against a fake project; every conversion
// checked against values computed here by hand from the documented
// semantics.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "check.h"
#include "engine.h"
#include "library.h"
#include "mapping.h"
#include "parse.h"
#include "scheduler.h"

using namespace tc;

namespace {

int* gPassP;
int* gFailP;

#define MEXPECT(cond, ...)                              \
    do {                                                \
        if (cond) {                                     \
            (*gPassP)++;                                \
        } else {                                        \
            (*gFailP)++;                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

constexpr float kFs = 44100.0f;
constexpr float kSpc = 44100.0f;  // samples per cycle (cps 1)

struct FakeHost : TrackerHost {
    struct I {
        bool active;
        InstrKind kind;
        uint8_t slices;
        std::string name;
    };
    std::vector<I> ins = {
        {true, InstrKind::OneShot, 0, "BD.wav"},     // 0
        {true, InstrKind::OneShot, 0, "sd.WAV"},     // 1
        {true, InstrKind::Slice, 8, "breaks.wav"},   // 2
        {true, InstrKind::Loop, 0, "pad.wav"},       // 3
        {false, InstrKind::OneShot, 0, "hh.wav"},    // 4 (inactive)
        {true, InstrKind::OneShot, 0, "arpy.1.wav"}, // 5 (dot in the name)
        {true, InstrKind::OneShot, 0, "606-Kick (2).wav"},          // 6 -> 606-kick__2_
        {true, InstrKind::OneShot, 0, "a very long sample name.wav"}, // 7 -> a_very_long_sam
    };
    int fallback = -1;
    int instrumentCount() const override { return (int)ins.size(); }
    InstrumentInfo instrument(int i) const override {
        return InstrumentInfo{ins[i].active, ins[i].kind, ins[i].slices, ins[i].name.c_str()};
    }
    // fake curve: linear, instrument volume 50
    uint8_t volumeStep(int, float ratio) const override {
        float v = 50.0f * ratio;
        return (uint8_t)(v > 100 ? 100 : v < 0 ? 0 : lroundf(v));
    }
    int fallbackInstrument(const char*) const override { return fallback; }
} gHost;

Program gProg;
Event gEv[256];
TrackerMapper gMap;

struct Out {
    bool ok;
    Entry e;
    int mod(TMod m) const {  // value, or -1 if absent
        for (int i = 0; i < e.nmods; i++)
            if (e.modId[i] == (uint8_t)m) return e.modVal[i];
        return -1;
    }
};

// Map the onsets of `slot` (0-based) running `code` over cycle 0.
std::vector<Out> mapCode(int slot, const char* code) {
    std::vector<Out> r;
    std::string text = "d" + std::to_string(slot + 1) + " $ " + code + "\n";
    Error e, ce;
    gProg.clear();
    if (!parseFile(gProg, text.c_str(), (int)text.size(), e) || !checkProgram(gProg, ce)) {
        MEXPECT(false, "%s: %s%s", code, e.msg, ce.msg);
        return r;
    }
    Error qe;
    int n = queryPattern(gProg, gProg.at(gProg.block[slot]).first, Arc{Q{0, 1}, Q{1, 1}}, gEv, 256, qe);
    MEXPECT(n >= 0, "%s: %s", code, qe.msg);
    for (int i = 0; i < n; i++) {
        const Event& ev = gEv[i];
        if (!ev.hasWhole || !(ev.whole.s == ev.part.s)) continue;
        Q len = ev.whole.e - ev.whole.s;
        Out o;
        memset(&o.e, 0, sizeof o.e);
        o.ok = gMap.map(gProg, slot, ev, kSpc, (float)len.n / (float)len.d * kSpc, o.e);
        r.push_back(o);
    }
    return r;
}
Out one(int slot, const char* code) {
    std::vector<Out> v = mapCode(slot, code);
    MEXPECT(v.size() == 1, "%s: %zu onsets, want 1", code, v.size());
    return v.empty() ? Out{false, {}} : v[0];
}

uint16_t u16(float v) { return (uint16_t)lroundf(v * 65535.0f); }
uint16_t cutoffRef(float hz) {
    // the Tracker filter's table: entry i = 20 Hz x 2^(i/50), cutoff = i / 477
    float i = 50.0f * log2f(hz / 20.0f);
    float c = i / 477.0f;
    return u16(c < 0 ? 0 : c > 1 ? 1 : c);
}

void testInstruments() {
    gHost.fallback = -1;
    gMap.init(&gHost, kFs);
    Out o = one(0, "s \"bd\"");
    MEXPECT(o.ok && o.e.chan == 0 && o.e.note == 60 && o.e.vel == 255 && o.e.nmods == 0 && o.e.dur == 0,
            "bd: ok %d chan %d note %d vel %d mods %d dur %u", o.ok, o.e.chan, o.e.note, o.e.vel, o.e.nmods,
            o.e.dur);
    o = one(0, "s \"sd\"");
    MEXPECT(o.ok && o.e.chan == 1, "sd -> sd.WAV (case): chan %d", o.e.chan);
    {
        char t[16];
        sampleToken("606-Kick (2).wav", t, sizeof t);
        MEXPECT(!strcmp(t, "606-kick__2_"), "token %s", t);
        sampleToken("noext", t, sizeof t);
        MEXPECT(!strcmp(t, "noext"), "token %s", t);
    }
    o = one(0, "s \"606-kick__2_\"");
    MEXPECT(o.ok && o.e.chan == 6, "606-kick__2_ -> 606-Kick (2).wav: chan %d", o.e.chan);
    o = one(0, "s \"a_very_long_sam\"");
    MEXPECT(o.ok && o.e.chan == 7, "15-char token of a long name: chan %d", o.e.chan);
    o = one(0, "s \"arpy.1\"");
    MEXPECT(o.ok && o.e.chan == 5, "arpy.1 -> arpy.1.wav: chan %d", o.e.chan);
    o = one(0, "s \"arpy\"");
    MEXPECT(!o.ok, "arpy must not match arpy.1.wav");
    o = one(0, "s \"hh\"");
    MEXPECT(!o.ok, "inactive instrument matched");
    MEXPECT(gMap.warnings(0).unknownName == 2 && !strcmp(gMap.warnings(0).lastUnknown, "hh"),
            "unknown names: %u, last '%s'", gMap.warnings(0).unknownName, gMap.warnings(0).lastUnknown);
    o = one(0, "n 3");
    MEXPECT(!o.ok && gMap.warnings(0).noSound == 1, "event without s on a sample slot");
    // `n` on a plain sample = pitch; on a sliced one = slice (mod count)
    o = one(0, "s \"bd:3\"");
    MEXPECT(o.ok && o.e.note == 63 && o.mod(TMod::Slice) < 0, "bd:3: note %d", o.e.note);
    o = one(0, "s \"breaks:10\"");
    MEXPECT(o.ok && o.e.chan == 2 && o.e.note == 60 && o.mod(TMod::Slice) == 2, "breaks:10: note %d slice %d",
            o.e.note, o.mod(TMod::Slice));
    o = one(0, "s \"breaks\" # n (-1)");
    MEXPECT(o.mod(TMod::Slice) == 7, "slice -1 -> %d, want 7", o.mod(TMod::Slice));
    // cache follows the project
    gHost.ins[0].name = "kick.wav";
    o = one(0, "s \"bd\"");
    MEXPECT(o.ok && o.e.chan == 0, "cached until instrumentsChanged()");
    gMap.instrumentsChanged();
    o = one(0, "s \"bd\"");
    MEXPECT(!o.ok, "rename not seen after instrumentsChanged()");
    gHost.fallback = 3;
    gMap.instrumentsChanged();
    o = one(0, "s \"bd\"");
    MEXPECT(o.ok && o.e.chan == 3, "fallback instrument: chan %d", o.e.chan);
    gHost.ins[0].name = "BD.wav";
    gHost.fallback = -1;
    gMap.instrumentsChanged();
}

void testPitchAndVolume() {
    gMap.init(&gHost, kFs);
    Out o = one(0, "s \"bd\" # note 7 # n 2");
    MEXPECT(o.e.note == 69 && o.mod(TMod::FineTune) < 0, "note 7 + n 2: %d", o.e.note);
    o = one(0, "s \"bd\" # speed 2");
    MEXPECT(o.e.note == 72 && o.mod(TMod::FineTune) < 0 && o.mod(TMod::Reverse) < 0, "speed 2: %d", o.e.note);
    o = one(0, "s \"bd\" # speed 1.5");  // 7.02 semitones
    MEXPECT(o.e.note == 67 && o.mod(TMod::FineTune) == 2, "speed 1.5: note %d cents %d", o.e.note,
            o.mod(TMod::FineTune));
    o = one(0, "s \"bd\" # speed (-1)");
    MEXPECT(o.e.note == 60 && o.mod(TMod::Reverse) == 1, "speed -1: note %d reverse %d", o.e.note,
            o.mod(TMod::Reverse));
    o = one(0, "s \"bd\" # speed 0");
    MEXPECT(!o.ok, "speed 0 played");
    o = one(0, "s \"bd\" # note 0.5");
    MEXPECT(o.e.note == 61 && (int16_t)o.mod(TMod::FineTune) == -50, "note 0.5: note %d cents %d", o.e.note,
            (int16_t)o.mod(TMod::FineTune));
    o = one(0, "s \"bd\" # note 100");
    MEXPECT(o.e.note == 120, "note clamp: %d", o.e.note);
    // amplitude = amp / 0.4 x min(gain, 2)^4 x velocity, relative to the instrument (fake: 50 x ratio)
    struct V {
        const char* code;
        int want;
    } vols[] = {{"s \"bd\" # gain 1", 50},   {"s \"bd\" # gain 0.5", 3},   {"s \"bd\" # gain 1.1", 73},
                {"s \"bd\" # gain 3", 100},  {"s \"bd\" # amp 0.2", 25},   {"s \"bd\" # velocity 0.5", 25},
                {"s \"bd\" # amp 0.8 # gain 0.9", 66}};
    for (const V& v : vols) {
        o = one(0, v.code);
        MEXPECT(o.e.vel == v.want, "%s: vol %d, want %d", v.code, o.e.vel, v.want);
    }
}

void testEffects() {
    gMap.init(&gHost, kFs);
    Out o = one(0, "s \"bd\" # cutoff 1000");
    MEXPECT(o.mod(TMod::CutoffLP) == cutoffRef(1000) && o.mod(TMod::Resonance) < 0, "cutoff 1000: %d want %d",
            o.mod(TMod::CutoffLP), cutoffRef(1000));
    MEXPECT(TrackerMapper::cutoffValue(20) == 0 && TrackerMapper::cutoffValue(5) == 0 &&
                TrackerMapper::cutoffValue(30000) == 65535 && TrackerMapper::cutoffValue(4000) == cutoffRef(4000),
            "cutoff ends");
    o = one(0, "s \"bd\" # lpf 500 # lpq 0.5");
    MEXPECT(o.mod(TMod::CutoffLP) == cutoffRef(500) && o.mod(TMod::Resonance) == u16(0.5f), "lpf + lpq");
    o = one(0, "s \"bd\" # hpf 200 # hresonance 0.25 # resonance 0.9");
    MEXPECT(o.mod(TMod::CutoffHP) == cutoffRef(200) && o.mod(TMod::Resonance) == u16(0.25f), "hpf + hresonance");
    o = one(0, "s \"bd\" # lpf 500 # hpq 0.5");
    MEXPECT(o.mod(TMod::Resonance) < 0, "hpq applied to a low-pass");
    o = one(0, "s \"bd\" # bpf 800 # resonance 0.2");
    MEXPECT(o.mod(TMod::CutoffBP) == cutoffRef(800) && o.mod(TMod::Resonance) == u16(0.2f), "bandf");
    uint32_t clash = gMap.warnings(0).filterClash;
    o = one(0, "s \"bd\" # cutoff 300 # hpf 900");
    MEXPECT(o.mod(TMod::CutoffHP) == cutoffRef(900) && o.mod(TMod::CutoffLP) < 0 &&
                gMap.warnings(0).filterClash == clash + 1,
            "two filters: last wins + warning");
    o = one(0, "s \"bd\" # resonance 0.5");
    MEXPECT(o.e.nmods == 0, "resonance without a filter");

    o = one(0, "s \"bd\" # pan 0.25 # shape 0.5 # crush 8 # room 0.3 # delay 1.2");
    MEXPECT(o.mod(TMod::Pan) == 25 && o.mod(TMod::Overdrive) == 50 && o.mod(TMod::BitDepth) == 8 &&
                o.mod(TMod::Reverb) == 30 && o.mod(TMod::Delay) == 100,
            "pan %d drive %d bits %d room %d delay %d", o.mod(TMod::Pan), o.mod(TMod::Overdrive),
            o.mod(TMod::BitDepth), o.mod(TMod::Reverb), o.mod(TMod::Delay));
    // batch 3: global reverb size / delay time / feedback, the DJ filter
    o = one(0, "s \"bd\" # size 0.5 # delaytime 0.25 # delayfeedback 0.6");
    MEXPECT(o.mod(TMod::RevSize) == 32768 && o.mod(TMod::DelayTime) == 250 && o.mod(TMod::DelayFb) == 60,
            "size %d delaytime %d delayfeedback %d", o.mod(TMod::RevSize), o.mod(TMod::DelayTime),
            o.mod(TMod::DelayFb));
    o = one(0, "s \"bd\" # sz 2 # delayt 9 # delayfb 2");  // aliases, clamped
    MEXPECT(o.mod(TMod::RevSize) == 65535 && o.mod(TMod::DelayTime) == 3500 && o.mod(TMod::DelayFb) == 99,
            "clamps: %d %d %d", o.mod(TMod::RevSize), o.mod(TMod::DelayTime), o.mod(TMod::DelayFb));
    o = one(0, "s \"bd\" # djf 0.25");
    MEXPECT(o.mod(TMod::CutoffLP) == 32768 && o.mod(TMod::CutoffHP) < 0, "djf 0.25: low-pass half open");
    o = one(0, "s \"bd\" # djf 0.75");
    MEXPECT(o.mod(TMod::CutoffHP) == 32768 && o.mod(TMod::CutoffLP) < 0, "djf 0.75: high-pass");
    o = one(0, "s \"bd\" # djf 0.5");
    MEXPECT(o.e.nmods == 0, "djf 0.5: no filter");
    o = one(0, "s \"bd\" # cutoff 300 # djf 0.2");
    MEXPECT(o.mod(TMod::CutoffLP) == cutoffRef(300), "an explicit filter wins over djf");
    o = one(0, "s \"bd\" # crush 2 # distort 0.7 # shape 0.2");
    MEXPECT(o.mod(TMod::BitDepth) == 4 && o.mod(TMod::Overdrive) == 70, "crush 2 -> %d, drive %d",
            o.mod(TMod::BitDepth), o.mod(TMod::Overdrive));
    o = one(0, "s \"bd\" # begin 0.25 # end 0.75");
    MEXPECT(o.mod(TMod::Begin) == u16(0.25f) && o.mod(TMod::End) == u16(0.75f), "begin / end");
    o = one(0, "s \"breaks\" # begin 0.25");
    MEXPECT(o.mod(TMod::Begin) < 0, "begin on a sliced instrument");
    o = one(0, "s \"bd\" # attack 0.01 # release 20");
    MEXPECT(o.mod(TMod::Attack) == 10 && o.mod(TMod::Release) == 10000, "attack %d release %d",
            o.mod(TMod::Attack), o.mod(TMod::Release));
}

void testLengths() {
    gMap.init(&gHost, kFs);
    Out o = one(0, "s \"bd\"");
    MEXPECT(o.e.dur == 0, "one-shot plays out: dur %u", o.e.dur);
    o = one(0, "s \"breaks\"");
    MEXPECT(o.e.dur == 0, "slice plays out: dur %u", o.e.dur);
    o = one(0, "s \"pad\"");
    MEXPECT(o.e.dur == (uint32_t)kSpc, "loop releases at the event's end: dur %u", o.e.dur);
    std::vector<Out> v = mapCode(0, "s \"bd*4\" # legato 0.5");
    MEXPECT(v.size() == 4 && v[0].e.dur == (uint32_t)(kSpc / 8), "legato 0.5 of a quarter: %u",
            v.empty() ? 0 : v[0].e.dur);
    o = one(0, "s \"pad\" # sustain 0.1");
    MEXPECT(o.e.dur == (uint32_t)(kFs * 0.1f), "sustain 0.1 s: %u", o.e.dur);
}

// `voices` (Tracker Cycles' own control): default 1, clamped to 1-3
void testVoices() {
    gMap.init(&gHost, kFs);
    Out o = one(0, "s \"bd\"");
    MEXPECT(o.ok && o.e.voices == 1, "default voices %u, want 1", o.e.voices);
    o = one(0, "s \"bd\" # voices 3");
    MEXPECT(o.e.voices == 3, "voices 3: %u", o.e.voices);
    o = one(0, "s \"bd\" # voices 7");
    MEXPECT(o.e.voices == 3, "voices 7 clamps to 3: %u", o.e.voices);
    o = one(0, "s \"bd\" # voices 0");
    MEXPECT(o.e.voices == 1, "voices 0 clamps to 1: %u", o.e.voices);
    std::vector<Out> v = mapCode(0, "n \"0 1\" # s \"arpy\" # voices \"2 3\"");
    MEXPECT(v.size() == 2 && v[0].e.voices == 2 && v[1].e.voices == 3, "patterned voices");
}

void testOverflow() {
    gMap.init(&gHost, kFs);
    Out o = one(0, "s \"bd\" # note 0.3 # cutoff 500 # resonance 0.2 # pan 0.1 # begin 0.1 # end 0.9 "
                   "# speed (-1) # attack 0.1 # release 0.2 # shape 0.3 # crush 6 # room 0.5 # delay 0.2");
    MEXPECT(o.ok && o.e.nmods == kMaxMods && gMap.warnings(0).droppedMods == 1, "nmods %d, dropped %u",
            o.e.nmods, gMap.warnings(0).droppedMods);
    bool ordered = true;
    for (int i = 1; i < o.e.nmods; i++) ordered &= o.e.modId[i - 1] < o.e.modId[i];
    MEXPECT(ordered, "mods not in priority order");
    // priority: fine tune, filter, pan, begin, end, reverse, resonance, release, attack, overdrive kept;
    // bit depth, reverb, delay dropped
    MEXPECT(o.mod(TMod::FineTune) >= 0 && o.mod(TMod::Overdrive) >= 0 && o.mod(TMod::BitDepth) < 0 &&
                o.mod(TMod::Delay) < 0,
            "kept the wrong overrides");
}

void testMidi() {
    gMap.init(&gHost, kFs);
    std::vector<Out> v = mapCode(8, "n \"0 7\" # s \"midi\"");
    MEXPECT(v.size() == 2 && v[0].ok && v[0].e.chan == 0 && v[0].e.note == 60 && v[1].e.note == 67 &&
                v[0].e.vel == 100 && v[0].e.dur == (uint32_t)(kSpc / 2),
            "d9 basic");
    Out o = one(11, "note 3 # midichan 9 # velocity 0.5 # legato 0.5");
    MEXPECT(o.ok && o.e.chan == 9 && o.e.note == 63 && o.e.vel == 50 && o.e.dur == (uint32_t)(kSpc / 2),
            "d12: chan %d note %d vel %d dur %u", o.e.chan, o.e.note, o.e.vel, o.e.dur);
    o = one(9, "n 0 # ccn 7 # ccv 64 # midibend 100");
    MEXPECT(o.e.chan == 1 && o.mod(TMod::Cc) == (7 << 8 | 64) && o.mod(TMod::Bend) == 8292, "cc / bend");
    o = one(8, "n 0 # progNum 5 # miditouch 90");
    MEXPECT(o.mod(TMod::Program) == 5 && o.mod(TMod::Touch) == 90, "program / aftertouch");
    o = one(8, "n 0 # ccn 7");
    MEXPECT(o.mod(TMod::Cc) < 0, "ccn without ccv");
    o = one(8, "n 0 # gain 2");
    MEXPECT(o.e.vel == 127, "midi vel clamp: %d", o.e.vel);
}

// The scheduler carries the 48-byte entries through unchanged.
struct RecVoice : Voice {
    std::vector<Entry> on;
    std::vector<std::pair<int, int>> off;  // chan, note
    uint16_t noteOn(int, const Entry& e, uint32_t) override {
        on.push_back(e);
        return 0;
    }
    void noteOff(int, uint8_t chan, uint8_t note, uint8_t, uint16_t) override { off.push_back({chan, note}); }
    void allOff() override {}
};
Entry gStorage[Scheduler::kStorageEntries];
Event gBuf[256];
Program gLive;
Scheduler gS;

void testThroughScheduler() {
    gMap.init(&gHost, kFs);
    gLive.clear();
    gS.init(&gLive, gBuf, 256, &gMap, gStorage);
    const char* text = "d1 $ s \"bd sd\" # cutoff 1000 # pan \"0 1\" # crush 6\n";
    Error e, ce;
    gProg.clear();
    if (!parseFile(gProg, text, (int)strlen(text), e) || !checkProgram(gProg, ce)) {
        MEXPECT(false, "parse");
        return;
    }
    Error err;
    gS.evaluate(gProg, 0, err);
    RecVoice voice;
    for (int b = 0; b < 2000; b++) {  // ~5.8 s at the default cps 0.575
        if (b % 8 == 0) gS.update();
        gS.tick(voice);
    }
    bool ok = voice.on.size() >= 4;
    for (size_t i = 0; i < voice.on.size() && ok; i++) {
        const Entry& x = voice.on[i];
        Out o{true, x};
        ok = x.chan == (i % 2 ? 1 : 0) && o.mod(TMod::CutoffLP) == cutoffRef(1000) &&
             o.mod(TMod::Pan) == (i % 2 ? 100 : 0) && o.mod(TMod::BitDepth) == 6;
    }
    MEXPECT(ok, "entries through the scheduler: %zu notes", voice.on.size());

    // MIDI noteOffs go to the channel their noteOn used (midichan varies)
    gLive.clear();
    gS.init(&gLive, gBuf, 256, &gMap, gStorage);
    const char* midi = "d9 $ n \"0 1\" # midichan \"1 5\" # legato 0.5\n";
    gProg.clear();
    if (!parseFile(gProg, midi, (int)strlen(midi), e) || !checkProgram(gProg, ce)) {
        MEXPECT(false, "parse midi");
        return;
    }
    gS.evaluate(gProg, 8, err);
    RecVoice mv;
    for (int b = 0; b < 2000; b++) {
        if (b % 8 == 0) gS.update();
        gS.tick(mv);
    }
    bool chans = mv.off.size() >= 4;
    for (const auto& o : mv.off) chans &= (o.second == 60 && o.first == 1) || (o.second == 61 && o.first == 5);
    MEXPECT(chans, "MIDI noteOff channels: %zu offs", mv.off.size());
}

// Tracker mode: which functions / controls the device takes, per slot kind.
void testTrackerCheck() {
    // every library control has an explicit rule (a new control must decide)
    for (int id = 0; id < libCount(); id++) {
        if (lib(id).cat != Cat::Control) continue;
        const char* a = trackerLibIssue(id, 0);
        const char* b = trackerLibIssue(id, 8);
        bool unmapped = (a && !strcmp(a, "isn't mapped on the Tracker")) || (b && !strcmp(b, "isn't mapped on the Tracker"));
        MEXPECT(!unmapped, "control '%s' has no Tracker rule", lib(id).name);
        MEXPECT(!a || !b || strstr(a, "isn't available"), "control '%s' works on no slot kind", lib(id).name);
    }
    struct C {
        int slot;
        const char* code;
        const char* err;  // nullptr = fine
    } cases[] = {
        {0, "s \"bd*2\" # cutoff 300 # pan sine # speed 2 # legato 0.5", nullptr},
        {0, "every 2 (# crush 4) $ n \"0 3\" # s \"arpy\" # lpf 800 # lpq 0.2", nullptr},
        {8, "n \"0 7\" # velocity 0.8 # midichan 3 # ccn 74 # ccv sine # legato 0.9", nullptr},
        {8, "n \"0\" # cutoff 300", "tracker mode: 'cutoff' has no effect on MIDI slots (d9-d16)"},
        {8, "n \"0\" |* speed 2", "tracker mode: 'speed' has no effect on MIDI slots (d9-d16)"},
        {9, "s \"bd\"", "tracker mode: 's' has no effect on MIDI slots (d9-d16)"},
        {0, "s \"bd\" # midichan 2", "tracker mode: 'midichan' only works on MIDI slots (d9-d16)"},
        {2, "n \"c'maj\" # s \"arpy\" # voices 3", nullptr},
        {8, "n \"0\" # voices 3", "tracker mode: 'voices' has no effect on MIDI slots (d9-d16)"},
        {0, "s \"bd\" # orbit 1", "tracker mode: 'orbit' isn't available on the Tracker (one effects bus)"},
        {3, "s \"bd\" # unit \"c\"", "tracker mode: 'unit' isn't available on the Tracker (no cycle-relative speed)"},
        {0, "chop 4 $ s \"break\"", "tracker mode: 'chop' isn't available on the Tracker"},
    };
    for (const C& c : cases) {
        std::string text = "d" + std::to_string(c.slot + 1) + " $ " + c.code + "\n";
        Error e, ce;
        gProg.clear();
        if (!parseFile(gProg, text.c_str(), (int)text.size(), e) || !checkProgram(gProg, ce)) {
            MEXPECT(false, "%s: %s%s", c.code, e.msg, ce.msg);
            continue;
        }
        Error te;
        bool ok = trackerCheck(gProg, c.slot, te);
        if (!c.err) {
            MEXPECT(ok, "%s: %s", c.code, te.msg);
        } else {
            MEXPECT(!ok && !strcmp(te.msg, c.err), "%s: got '%s'", c.code, ok ? "(ok)" : te.msg);
            MEXPECT(ok || (te.node && gProg.at(te.node).kind == Kind::Fn), "%s: error node not the function",
                    c.code);
        }
    }
    // a wide block (long sequence) is no problem
    std::string wide = "d1 $ s \"";  // kMaxSeqSteps steps: more children than any fixed stack would hold
    for (int i = 0; i < kMaxSeqSteps; i++) wide += "bd ";
    wide += "\" # pan 0.5\n";
    Error e, ce, te;
    gProg.clear();
    bool parsed = parseFile(gProg, wide.c_str(), (int)wide.size(), e) && checkProgram(gProg, ce);
    MEXPECT(parsed && trackerCheck(gProg, 0, te), "wide block: %s%s%s", e.msg, ce.msg, te.msg);
}

}  // namespace

void testMapping(int& pass, int& fail) {
    gPassP = &pass;
    gFailP = &fail;
    int f0 = fail;
    testInstruments();
    testPitchAndVolume();
    testEffects();
    testLengths();
    testVoices();
    testOverflow();
    testMidi();
    testThroughScheduler();
    testTrackerCheck();
    printf("mapping: %s\n", fail == f0 ? "ok" : "FAILED");
}
