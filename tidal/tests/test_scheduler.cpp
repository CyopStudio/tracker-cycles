// Scheduler tests (docs/tidal_scheduler.md section 9): a fake interrupt
// ticks the block clock, a fake main loop calls update() at irregular
// intervals, a recording voice logs what would play. Expected onsets come
// from querying the engine directly over the whole span, timed with exact
// 128-bit arithmetic.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "check.h"
#include "engine.h"
#include "parse.h"
#include "scheduler.h"

using namespace tc;

namespace {

int* gPassP;
int* gFailP;

#define SEXPECT(cond, ...)                              \
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

constexpr int64_t kFsNum = 750000, kFsDen = 17;  // MK66 44117.647 Hz

Program gEdit, gLive;
Event gBuf[1024], gRefBuf[4096];
Scheduler gS;

uint8_t hashNote(const char* s, float n) {
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return (uint8_t)((h + (int)n * 7) & 127);
}

// Test mapping: note = hash of (s, n), dur from legato.
bool mapEvent(const Program& p, int slot, const Event& e, float lenSamples, Entry& out) {
    const char* s = "";
    float n = 0, legato = 0;
    if (e.isMap)
        for (int i = 0; i < e.nctl; i++) {
            const Ctl& c = e.ctl[i];
            const char* name = controlDef(c.key).name;
            if (c.tag == 's' && c.str < kEngineStr) s = p.str(c.str);
            else if (!strcmp(name, "n") || !strcmp(name, "note")) n = c.f;
            else if (!strcmp(name, "legato")) legato = c.f;
        }
    out.kind = EntryKind::Note;
    out.chan = (uint8_t)slot;
    out.note = hashNote(s, n);
    out.vel = 100;
    out.dur = legato > 0 ? (uint32_t)(lenSamples * legato) : 0;
    return true;
}

struct TMapper : Mapper {
    bool map(const Program& p, int slot, const Event& e, float, float lenSamples, Entry& out) override {
        return mapEvent(p, slot, e, lenSamples, out);
    }
} gMapper;

struct Rec {
    uint32_t at;   // block it played in
    int slot;
    uint32_t block;
    uint8_t offset;
    uint8_t note;
    uint32_t late;
    int64_t sample() const { return (int64_t)block * kBlockSamples + offset; }
};
struct RecVoice : Voice {
    std::vector<Rec> on;
    std::vector<Rec> off;
    int allOffs = 0;
    uint32_t now = 0;
    void noteOn(int slot, const Entry& e, uint32_t late) override {
        on.push_back(Rec{now, slot, e.block, e.offset, e.note, late});
    }
    void noteOff(int slot, uint8_t note, uint8_t offset) override {
        off.push_back(Rec{now, slot, now, offset, note, 0});
    }
    void allOff() override { allOffs++; }
} gVoice;

// Exact piecewise-linear cycle -> sample reference.
struct Seg {
    Q c0;
    __int128 sN, sD;  // sample at c0 = sN / sD
    Q cps;
};
std::vector<Seg> gSegs;

int64_t refSample(Q q) {
    const Seg* s = &gSegs[0];
    for (const Seg& g : gSegs)
        if (!(q < g.c0)) s = &g;
    Q d = q - s->c0;
    // s->sN/s->sD + d.n/d.d * fsNum*cps.d / (fsDen*cps.n)
    __int128 n2 = (__int128)d.n * kFsNum * s->cps.d, d2 = (__int128)d.d * kFsDen * s->cps.n;
    __int128 num = s->sN * d2 + n2 * s->sD, den = s->sD * d2;
    __int128 f = num / den;
    if (num % den != 0 && num < 0) f--;
    return (int64_t)f;
}
void refReset(Q cps) {
    gSegs.clear();
    gSegs.push_back(Seg{Q{0, 1}, 0, 1, cps});
}
void refCps(Q at, Q cps) {
    const Seg& s = gSegs.back();
    Q d = at - s.c0;
    __int128 n2 = (__int128)d.n * kFsNum * s.cps.d, d2 = (__int128)d.d * kFsDen * s.cps.n;
    gSegs.push_back(Seg{at, s.sN * d2 + n2 * s.sD, s.sD * d2, cps});
}

void reset(int bufCap = 1024) {
    gLive.clear();
    gS.init(&gLive, gBuf, bufCap, &gMapper);
    gVoice = RecVoice();
    refReset(Q{23, 40});
}

bool load(const char* text) {
    Error e, ce;
    gEdit.clear();
    bool ok = parseFile(gEdit, text, (int)strlen(text), e) && checkProgram(gEdit, ce);
    SEXPECT(ok, "parse %s: %s%s", text, e.msg, ce.msg);
    return ok;
}

bool evalSlot(int slot) {
    Error e;
    bool ok = gS.evaluate(gEdit, slot, e);
    SEXPECT(ok, "evaluate d%d: %s", slot + 1, e.msg);
    return ok;
}

// Run the fake interrupt to block `until`, the fake main loop every 1..maxGap
// blocks (random). `hook(block)` runs on the main loop before update().
template <class F>
void run(uint32_t until, int maxGap, F hook) {
    uint32_t next = gS.block();
    while (gS.block() < until) {
        uint32_t b = gS.block();
        if (b >= next) {
            hook(b);
            gS.update();
            next = b + 1 + (uint32_t)(rand() % maxGap);
        }
        gVoice.now = b;
        gS.tick(gVoice);
    }
}
void run(uint32_t until, int maxGap) {
    run(until, maxGap, [](uint32_t) {});
}

struct Onset {
    int slot;
    int64_t sample;
    uint8_t note;
    bool operator<(const Onset& o) const {
        return slot != o.slot ? slot < o.slot : sample != o.sample ? sample < o.sample : note < o.note;
    }
};

void refOnsetsPart(int slot, Q a, Q b, std::vector<Onset>& out);

// Onsets of slot `slot` of gEdit over [a, b), timed by the reference.
void refOnsets(int slot, Q a, Q b, std::vector<Onset>& out) {
    // quarter-cycle pieces (the arena holds 1024 events)
    for (Q s = a; s < b;) {
        Q e = qmin(b, qint(qfloor(s * qint(4)) + 1) / qint(4));
        refOnsetsPart(slot, s, e, out);
        s = e;
    }
}

void refOnsetsPart(int slot, Q a, Q b, std::vector<Onset>& out) {
    Error err;
    uint16_t expr = gEdit.at(gEdit.block[slot]).first;
    int n = queryPattern(gEdit, expr, Arc{a, b}, gRefBuf, 4096, err);
    SEXPECT(n >= 0, "reference query: %s", err.msg);
    int keyNudge = controlKey("nudge");
    for (int i = 0; i < n; i++) {
        const Event& e = gRefBuf[i];
        if (!e.hasWhole || !(e.whole.s == e.part.s)) continue;
        Entry en;
        memset(&en, 0, sizeof en);
        mapEvent(gEdit, slot, e, 0, en);
        int64_t s = refSample(e.whole.s);
        for (int c = 0; c < e.nctl; c++)
            if (e.ctl[c].key == keyNudge) s += (int64_t)(e.ctl[c].f * kFsNum / kFsDen);
        out.push_back(Onset{slot, s, en.note});
    }
}

// Compare two onset lists (sample within +-1).
bool sameOnsets(std::vector<Onset> got, std::vector<Onset> want, const char* what) {
    std::sort(got.begin(), got.end());
    std::sort(want.begin(), want.end());
    bool ok = got.size() == want.size();
    for (size_t i = 0; ok && i < got.size(); i++)
        ok = got[i].slot == want[i].slot && got[i].note == want[i].note && llabs(got[i].sample - want[i].sample) <= 1;
    if (!ok) {
        printf("  %s: got %zu onsets, want %zu\n", what, got.size(), want.size());
        for (size_t i = 0; i < std::max(got.size(), want.size()) && i < 400; i++) {
            bool g = i < got.size(), w = i < want.size();
            bool same = g && w && got[i].slot == want[i].slot && got[i].note == want[i].note &&
                        llabs(got[i].sample - want[i].sample) <= 1;
            if (same) continue;
            printf("    [%zu] got %d/%lld/%d  want %d/%lld/%d\n", i, g ? got[i].slot : -1,
                   g ? (long long)got[i].sample : -1LL, g ? got[i].note : -1, w ? want[i].slot : -1,
                   w ? (long long)want[i].sample : -1LL, w ? want[i].note : -1);
            break;
        }
    }
    SEXPECT(ok, "%s: onsets differ", what);
    return ok;
}

std::vector<Onset> played(int64_t from, int64_t to) {
    std::vector<Onset> v;
    for (const Rec& r : gVoice.on)
        if (r.sample() >= from && r.sample() < to) v.push_back(Onset{r.slot, r.sample(), r.note});
    return v;
}

// ---- tests ---------------------------------------------------------------------

void testConversion() {
    reset();
    int bad = 0, badInv = 0;
    for (int i = 0; i < 20000; i++) {
        static const int dens[] = {1, 2, 3, 4, 5, 7, 8, 12, 16, 24, 48, 64, 96, 256, 1024};
        int32_t d = dens[rand() % 15];
        Q q = qmake(rand() % (100000 * d), d);  // up to 100k cycles (~2 days)
        int64_t s = gS.sampleAt(q), r = refSample(q);
        if (llabs(s - r) > 1) {
            if (bad++ < 5) printf("  sampleAt %d/%d: %lld want %lld\n", q.n, q.d, (long long)s, (long long)r);
        }
        Q back = gS.cycleAt(r);
        Q err = q - back;  // cycleAt rounds down to 1/4096 cycle (from the exact sample, floored)
        if (err < Q{-1, 4096} || Q{2, 4096} < err) {
            if (badInv++ < 5) printf("  cycleAt(%lld) = %d/%d, want ~%d/%d\n", (long long)r, back.n, back.d, q.n, q.d);
        }
    }
    SEXPECT(bad == 0, "sampleAt off by more than one sample in %d of 20000", bad);
    SEXPECT(badInv == 0, "cycleAt off in %d of 20000", badInv);
    SEXPECT(gS.window() == (Q{1, 16}), "window at cps 0.575 = %d/%d, want 1/16", gS.window().n, gS.window().d);
}

// Several slots, irregular main loop: everything plays exactly as queried.
void testPlayback() {
    reset();
    if (!load("d1 $ s \"bd*4 [sd cp]\" # n \"<0 2>\"\n"
              "d2 $ s \"hh*8\" # pan sine\n"
              "d3 $ every 3 (fast 2) $ s \"arpy*3\" # n \"0 .. 2\"\n"
              "d4 $ s \"bd? sd*3 ~ [~ cp]\" # nudge 0.01\n"
              "d9 $ n \"c'maj e4\" # s \"midi\" # legato 1\n"))
        return;
    const int slots[] = {0, 1, 2, 3, 8};
    for (int s : slots) evalSlot(s);
    const Q from = Q{1, 1}, to = Q{9, 1};
    uint32_t end = (uint32_t)(refSample(to) / kBlockSamples) + 20;
    run(end, 40);
    std::vector<Onset> want;
    for (int s : slots) refOnsets(s, from, to, want);
    sameOnsets(played(refSample(from) - 1, refSample(to) - 1), want, "5 slots, 8 cycles");
    SEXPECT(played(0, refSample(from) - 1).empty(), "notes before the launch cycle");
    int late = 0;
    std::map<int, int64_t> last;
    bool mono = true;
    for (const Rec& r : gVoice.on) {
        late += r.late != 0;
        if (last.count(r.slot) && r.sample() < last[r.slot]) mono = false;
        last[r.slot] = r.sample();
    }
    SEXPECT(late == 0, "%d notes late (4 sample slots never hit the cap)", late);
    SEXPECT(mono, "a slot's notes played out of order");
    SEXPECT(gS.stats().dropped == 0 && gS.stats().skips == 0, "dropped %u skips %u", gS.stats().dropped,
            gS.stats().skips);
    // MIDI legato 1: each note gets its noteOff after one event length
    int offs9 = 0;
    for (const Rec& r : gVoice.off) offs9 += r.slot == 8;
    int ons9 = 0;
    for (const Rec& r : gVoice.on) ons9 += r.slot == 8;
    SEXPECT(offs9 >= ons9 - 4 && offs9 <= ons9, "d9: %d noteOns, %d noteOffs", ons9, offs9);
}

// 8 sample slots hitting together: at most 4 starts per block, the rest one
// block later, fairly; MIDI never waits.
void testNoteStartCap() {
    reset();
    std::string text;
    for (int i = 1; i <= 12; i++) text += "d" + std::to_string(i) + " $ s \"bd*4\"\n";
    if (!load(text.c_str())) return;
    for (int i = 0; i < 12; i++) evalSlot(i < 8 ? i : i);
    uint32_t end = (uint32_t)(refSample(Q{17, 1}) / kBlockSamples);
    run(end, 30);
    std::map<uint32_t, int> perBlock;
    int lateCount[16] = {0}, total[16] = {0};
    uint32_t maxLate = 0;
    bool midiLate = false;
    for (const Rec& r : gVoice.on) {
        if (r.slot < kSampleSlots) {
            perBlock[r.at]++;
            lateCount[r.slot] += r.late > 0;
            total[r.slot]++;
            maxLate = std::max(maxLate, r.late);
        } else if (r.late) {
            midiLate = true;
        }
    }
    int maxPer = 0;
    for (auto& kv : perBlock) maxPer = std::max(maxPer, kv.second);
    SEXPECT(maxPer <= 4, "%d sample note starts in one block", maxPer);
    SEXPECT(maxLate == 1, "max lateness %u blocks, want 1", maxLate);
    SEXPECT(!midiLate, "MIDI notes were held back");
    SEXPECT(total[0] == 64, "slot 1 played %d notes, want 64", total[0]);
    for (int i = 0; i < kSampleSlots; i++)
        SEXPECT(lateCount[i] > total[i] / 5 && lateCount[i] < total[i] * 4 / 5,
                "slot %d late %d of %d times (round robin)", i + 1, lateCount[i], total[i]);
}

// Re-evaluating switches exactly at the launch time; nothing lost or doubled.
void testSwitch() {
    struct Mode {
        const char* code;
        int k;  // 0 = d1 $, >0 jumpMod k, <0 jumpIn -k
    } modes[] = {{"d1 $ s \"sd*16\"\n", 0},
                 {"jumpMod 1 4 $ s \"sd*16\"\n", 4},
                 {"jumpIn 1 2 $ s \"sd*16\"\n", -2}};
    for (const Mode& m : modes) {
        for (int rep = 0; rep < 12; rep++) {
            reset();
            if (!load("d1 $ s \"bd*16\"\n")) return;
            evalSlot(0);
            // evaluate at a random time in cycle [2, 4)
            int64_t s0 = refSample(Q{2, 1}), s1 = refSample(Q{4, 1});
            uint32_t evalBlock = (uint32_t)((s0 + rand() % (s1 - s0)) / kBlockSamples);
            if (rep == 0) evalBlock = (uint32_t)(refSample(Q{3, 1}) / kBlockSamples) - 1;  // just before a cycle
            Q sw{0, 1};
            bool done = false;
            run(evalBlock + 1, 25, [&](uint32_t b) {
                if (done || b < evalBlock) return;
                done = true;
                // the launch time, from the exact block time
                Q nowC = gS.cycleAt((int64_t)b * kBlockSamples);
                int64_t f = qfloor(nowC);
                if (m.k == 0) sw = qint(f + 1);
                else if (m.k > 0) sw = qint((f / m.k + 1) * m.k);
                else sw = qint((qint(f) == nowC ? f : f + 1) - m.k);
                load(m.code);
                evalSlot(0);
            });
            if (!done) {  // the main loop skipped past evalBlock
                Q nowC = gS.cycleAt((int64_t)gS.block() * kBlockSamples);
                int64_t f = qfloor(nowC);
                sw = m.k == 0 ? qint(f + 1) : m.k > 0 ? qint((f / m.k + 1) * m.k)
                                                     : qint((qint(f) == nowC ? f : f + 1) - m.k);
                load(m.code);
                evalSlot(0);
            }
            Q endC = sw + qint(2);
            run((uint32_t)(refSample(endC) / kBlockSamples) + 10, 25);
            std::vector<Onset> want;
            if (!load("d1 $ s \"bd*16\"\n")) return;
            refOnsets(0, Q{1, 1}, sw, want);
            load(m.code);
            refOnsets(0, sw, endC, want);
            char what[64];
            snprintf(what, sizeof what, "switch k=%d at %d/%d", m.k, sw.n, sw.d);
            if (!sameOnsets(played(0, refSample(endC) - 1), want, what)) return;
        }
    }
}

// cps change mid-pattern: no gap, no duplicate, new tempo from the anchor.
void testCps() {
    reset();
    if (!load("d1 $ s \"bd*8\"\nd2 $ s \"hh*3\"\n")) return;
    evalSlot(0);
    evalSlot(1);
    uint32_t at = (uint32_t)(refSample(Q{33, 10}) / kBlockSamples);
    run(at, 20);
    gS.setCps(Q{1, 1});
    Q t = gS.anchorCycle();
    SEXPECT(t > gS.cycleAt((int64_t)at * kBlockSamples), "cps change at %d/%d is in the past", t.n, t.d);
    refCps(t, Q{1, 1});
    Q endC{10, 1};
    run((uint32_t)(refSample(endC) / kBlockSamples) + 10, 20);
    std::vector<Onset> want;
    refOnsets(0, Q{1, 1}, endC, want);
    refOnsets(1, Q{1, 1}, endC, want);
    sameOnsets(played(0, refSample(endC) - 1), want, "cps 0.575 -> 1");
    SEXPECT(gS.window() == (Q{1, 8}), "window at cps 1 = %d/%d, want 1/8", gS.window().n, gS.window().d);

    // two quick changes: the second waits for the first
    gS.setCps(Q{1, 2});
    Q t1 = gS.anchorCycle();
    refCps(t1, Q{1, 2});
    gS.setCps(Q{3, 4});
    run((uint32_t)(refSample(t1) / kBlockSamples) + 2, 20);
    gS.update();
    Q t2 = gS.anchorCycle();
    SEXPECT(t1 < t2, "second cps change at %d/%d, first at %d/%d", t2.n, t2.d, t1.n, t1.d);
    refCps(t2, Q{3, 4});
    Q end2 = t2 + qint(3);
    run((uint32_t)(refSample(end2) / kBlockSamples) + 10, 20);
    want.clear();
    refOnsets(0, Q{1, 1}, end2, want);
    refOnsets(1, Q{1, 1}, end2, want);
    sameOnsets(played(0, refSample(end2) - 1), want, "cps 1 -> 0.5 -> 0.75");
}

// Main loop stalls: short ones are covered by the lookahead; long ones drop
// what can't play in time and resume on the grid.
void testStall() {
    reset();
    if (!load("d1 $ s \"hh*16\"\n")) return;
    evalSlot(0);
    const uint32_t blocksPerMs100 = (uint32_t)(0.1 * kFsNum / kFsDen / kBlockSamples);  // ~34
    uint32_t a = (uint32_t)(refSample(Q{2, 1}) / kBlockSamples);
    run(a, 20);
    // 100 ms stall: nothing lost
    for (uint32_t i = 0; i < blocksPerMs100; i++) {
        gVoice.now = gS.block();
        gS.tick(gVoice);
    }
    Q mid{4, 1};
    run((uint32_t)(refSample(mid) / kBlockSamples), 20);
    std::vector<Onset> want;
    refOnsets(0, Q{1, 1}, mid, want);
    sameOnsets(played(0, refSample(mid) - 1), want, "100 ms stall");
    SEXPECT(gS.stats().dropped == 0 && gS.stats().skips == 0, "100 ms stall: dropped %u skips %u",
            gS.stats().dropped, gS.stats().skips);
    // 2 s stall
    uint32_t stallFrom = gS.block();
    for (uint32_t i = 0; i < blocksPerMs100 * 20; i++) {
        gVoice.now = gS.block();
        gS.tick(gVoice);
    }
    uint32_t stallTo = gS.block();
    Q endC{9, 1};
    run((uint32_t)(refSample(endC) / kBlockSamples), 20);
    SEXPECT(gS.stats().skips >= 1, "2 s stall: no horizon skip");
    // every note played is on the grid and on time, once
    std::vector<Onset> grid;
    refOnsets(0, Q{1, 1}, endC, grid);
    std::map<int64_t, int> onGrid;
    for (const Onset& o : grid) onGrid[o.sample] = 0;
    int off = 0, dup = 0, tooLate = 0;
    for (const Rec& r : gVoice.on) {
        auto it = onGrid.lower_bound(r.sample() - 1);
        if (it == onGrid.end() || llabs(it->first - r.sample()) > 1) off++;
        else if (it->second++) dup++;
        if (r.late > 7) tooLate++;
    }
    SEXPECT(off == 0 && dup == 0 && tooLate == 0, "2 s stall: %d off-grid, %d doubled, %d too late", off, dup,
            tooLate);
    // nothing missing outside the stall (+ lookahead + window)
    int missing = 0;
    int64_t gapFrom = (int64_t)stallFrom * kBlockSamples, gapTo = (int64_t)(stallTo + 2) * kBlockSamples;
    for (auto& kv : onGrid)
        if (!kv.second && (kv.first < gapFrom || kv.first >= gapTo)) missing++;
    SEXPECT(missing == 0, "2 s stall: %d notes missing outside the stall", missing);
}

// Denser than a ring holds per window: windows split, the slot waits for
// room, nothing is lost or overwritten.
void testDense() {
    reset();
    if (!load("setcps 1\nd1 $ s \"bd*1024\"\n")) return;
    gS.setCps(Q{1, 1});
    refCps(gS.anchorCycle(), Q{1, 1});
    evalSlot(0);
    Q endC{4, 1};
    run((uint32_t)(refSample(endC) / kBlockSamples) + 10, 5);
    std::vector<Onset> want;
    refOnsets(0, Q{1, 1}, endC, want);
    sameOnsets(played(0, refSample(endC) - 1), want, "bd*1024 at cps 1");
    SEXPECT(gS.stats().splits > 0, "no window splits");
    SEXPECT(gS.stats().dropped == 0, "dense: %u dropped", gS.stats().dropped);
}

void testHushAndErrors() {
    reset();
    if (!load("d1 $ s \"bd*8\"\nd9 $ n \"0 3\" # s \"midi\" # legato 2\n")) return;
    evalSlot(0);
    evalSlot(8);
    uint32_t h = (uint32_t)(refSample(Q{5, 2}) / kBlockSamples);
    run(h, 10);
    gS.hush();
    size_t before = gVoice.on.size();
    run(h + 2000, 10);
    SEXPECT(gVoice.on.size() == before, "%zu notes after hush", gVoice.on.size() - before);
    SEXPECT(gVoice.allOffs == 1, "allOff called %d times", gVoice.allOffs);
    SEXPECT(!gS.playing(0) && !gS.playing(8), "slots still playing after hush");

    // silence switches like an evaluation: notes until the next cycle
    reset();
    if (!load("d1 $ s \"bd*8\"\n")) return;
    evalSlot(0);
    uint32_t sb = (uint32_t)(refSample(Q{21, 10}) / kBlockSamples);
    run(sb, 10);
    gS.silence(0);
    run(sb + 3000, 10);
    std::vector<Onset> want;
    refOnsets(0, Q{1, 1}, Q{3, 1}, want);
    sameOnsets(played(0, 1LL << 40), want, "silence at 2.1");

    // a query that can't fit even the smallest window fails the slot only
    reset(8);
    if (!load("d1 $ s \"bd*16384\"\nd2 $ s \"sd\"\n")) return;
    evalSlot(0);
    evalSlot(1);
    run((uint32_t)(refSample(Q{3, 1}) / kBlockSamples), 10);
    SEXPECT(gS.error(0) != nullptr, "dense slot with a tiny buffer did not fail");
    SEXPECT(gS.error(1) == nullptr, "d2 failed too");
    int sd = 0;
    for (const Rec& r : gVoice.on) sd += r.slot == 1;
    SEXPECT(sd == 2, "d2 played %d notes, want 2", sd);
}

}  // namespace

void testScheduler(int& pass, int& fail) {
    gPassP = &pass;
    gFailP = &fail;
    int f0 = fail;
    srand(1234);
    testConversion();
    testPlayback();
    testNoteStartCap();
    testSwitch();
    testCps();
    testStall();
    testDense();
    testHushAndErrors();
    printf("scheduler: %s\n", fail == f0 ? "ok" : "FAILED");
}
