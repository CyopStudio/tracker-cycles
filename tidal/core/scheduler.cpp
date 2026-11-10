// Scheduler, see scheduler.h and docs/tidal_scheduler.md.
#include "scheduler.h"

#include <string.h>

namespace tc {

namespace {

inline int64_t floorDiv(int64_t a, int64_t b) {  // b > 0
    int64_t q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}

inline void fence() { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

// Smallest multiple of w = 1/2^j that is > c.
Q gridAfter(Q c, Q w) {
    int64_t m = qfloor(c / w);
    return qint(m + 1) * w;
}

Q launchTime(Launch l, uint32_t k, Q now) {
    switch (l) {
        case Launch::JumpMod: {
            Q kq = qint(k ? k : 1);
            return qint(qfloor(now / kq) + 1) * kq;
        }
        case Launch::JumpIn: {
            int64_t c = qfloor(now);
            if (!(qint(c) == now)) c++;
            return qint(c + k);
        }
        default: return qint(qfloor(now) + 1);
    }
}

// Keep cps denominators small so samples-per-cycle stays exact in 64 bits.
Q tameCps(Q c) {
    if (c.d <= 65536) return qnorm(c);
    int64_t n = ((int64_t)c.n * 65536 + c.d / 2) / c.d;
    return qmake(n > 0 ? n : 1, 65536);
}

}  // namespace

void Scheduler::init(Program* live, Event* buf, int cap, Mapper* mapper, Entry* storage, Hooks hooks) {
    live_ = live;
    buf_ = buf;
    bufCap_ = cap;
    mapper_ = mapper;
    hooks_ = hooks;
    cap_ = 4;
    players_ = 0;
    muted_ = 0;
    idleValid_ = false;
    idleUntil_ = 0;
    tmp_ = storage + kSlots * kRingSize;
    hasPrev_ = false;
    pendingCps_ = false;
    nextCps_ = Q{0, 1};
    memset(&stats_, 0, sizeof stats_);
    for (int i = 0; i < kSlots; i++) {
        Slot& s = slot_[i];
        s.expr = s.next = 0;
        s.pending = false;
        s.at = s.h = Q{0, 1};
        s.failed = false;
        s.err = Error();
        ring_[i].e = storage + i * kRingSize;
        ring_[i].head = 0;
        ring_[i].tail = 0;
    }
    memset(off_, 0, sizeof off_);
    memset(nOff_, 0, sizeof nOff_);
    block_ = 0;
    hushReq_ = 0;
    hushSeen_ = 0;
    rr_ = 0;
    setSampleRate(750000, 17);
}

void Scheduler::resetStats() { memset(&stats_, 0, sizeof stats_); }

void Scheduler::setSampleRate(int64_t num, int64_t den) {
    fsNum_ = num;
    fsDen_ = den;
    idleValid_ = false;
    float fs = (float)num / (float)den;
    lookahead_ = (uint32_t)(fs * 0.25f);
    lateBlocks_ = (uint32_t)(fs * 0.02f / kBlockSamples + 0.5f);
    cur_ = makeAnchor(Q{0, 1}, 0, 0.0f, Q{23, 40});  // Tidal 1.10's default 0.575
    hasPrev_ = false;
}

Scheduler::Anchor Scheduler::makeAnchor(Q c0, int64_t s0, float s0f, Q cps) const {
    Anchor a;
    a.c0 = c0;
    a.s0 = s0;
    a.s0f = s0f;
    a.cps = cps;
    a.num = fsNum_ * cps.d;
    a.den = fsDen_ * cps.n;
    return a;
}

const Scheduler::Anchor& Scheduler::anchorFor(Q cycle) const {
    return hasPrev_ && cycle < cur_.c0 ? prev_ : cur_;
}

int64_t Scheduler::sampleAtAnchor(const Anchor& a, Q cycle, float* frac) const {
    Q d = cycle - a.c0;
    int64_t k = qfloor(d);
    Q f = d - qint(k);
    // integer cycles exactly, the fraction of a cycle in float
    int64_t kn = k * a.num;
    int64_t whole = floorDiv(kn, a.den);
    int64_t rem = kn - whole * a.den;
    float x = a.s0f + (float)rem / (float)a.den + ((float)f.n / (float)f.d) * ((float)a.num / (float)a.den);
    int64_t ix = (int64_t)x;
    if ((float)ix > x) ix--;
    if (frac) *frac = x - (float)ix;
    return a.s0 + whole + ix;
}

int64_t Scheduler::sampleAt(Q cycle) const { return sampleAtAnchor(anchorFor(cycle), cycle, nullptr); }

Q Scheduler::cycleAt(int64_t sample) const {
    const Anchor& a = hasPrev_ && sample < cur_.s0 ? prev_ : cur_;
    int64_t ds = (sample - a.s0) * a.den;
    int64_t k = floorDiv(ds, a.num);
    int64_t rem = ds - k * a.num;
    int64_t g = (int64_t)((float)rem / (float)a.num * kNowGrid);
    if (g >= kNowGrid) g = kNowGrid - 1;
    return a.c0 + qint(k) + qmake(g, kNowGrid);
}

Q Scheduler::window() const {
    // largest 1/2^j cycle that lasts <= 125 ms
    const Anchor& a = cur_;
    float maxCycles = 0.125f * (float)a.cps.n / (float)a.cps.d;
    int32_t den = 1;
    while ((float)1 / (float)den > maxCycles && den < 1024) den *= 2;
    return Q{1, den};
}

Q Scheduler::nowCycle() const { return cycleAt((int64_t)block_ * kBlockSamples); }

// ---- main loop ---------------------------------------------------------------

void Scheduler::truncate(int slot, int64_t fromSample) {
    Ring& r = ring_[slot];
    if (hooks_.lock) hooks_.lock();
    uint32_t t = r.tail;
    while (t != r.head) {
        const Entry& e = r.e[(t - 1) & (kRingSize - 1)];
        if ((int64_t)e.block * kBlockSamples + e.offset < fromSample) break;
        t--;
    }
    r.tail = t;
    if (hooks_.unlock) hooks_.unlock();
}

void Scheduler::switchTo(int i, uint16_t root, Q at) {
    idleValid_ = false;
    Slot& s = slot_[i];
    if (s.pending && s.next) live_->freeTree(s.next);
    if (!s.expr && !s.pending) s.h = at;  // idle slot: starts at `at`
    if (s.h > at) {
        truncate(i, sampleAt(at));
        s.h = at;
    }
    s.next = root;
    s.pending = true;
    s.at = at;
    s.failed = false;
}

bool Scheduler::evaluate(const Program& edit, int i, Error& err, uint16_t* liveRoot) {
    uint16_t b = edit.block[i];
    uint16_t root = b ? edit.at(b).first : 0;
    Launch l = b ? blockLaunch(edit.at(b)) : Launch::Now;
    uint32_t k = b ? edit.at(b).b : 0;
    uint16_t copy = 0;
    if (root) {
        copy = live_->copyTree(edit, root);
        if (!copy) {
            err.set(-1, "live program full (too many nodes or strings)");
            return false;
        }
    }
    if (liveRoot) *liveRoot = copy;
    switchTo(i, copy, launchTime(l, k, nowCycle()));
    return true;
}

void Scheduler::silence(int i) { switchTo(i, 0, launchTime(Launch::Now, 0, nowCycle())); }

void Scheduler::hush() {
    idleValid_ = false;
    int64_t now = (int64_t)block_ * kBlockSamples;
    Q nowC = cycleAt(now);
    for (int i = 0; i < kSlots; i++) {
        Slot& s = slot_[i];
        truncate(i, now);
        if (s.expr) live_->freeTree(s.expr);
        if (s.pending && s.next) live_->freeTree(s.next);
        s.expr = s.next = 0;
        s.pending = false;
        s.h = nowC;
    }
    hushReq_ = hushReq_ + 1;
}

void Scheduler::setCps(Q c) {
    if (c.n <= 0 || c.d <= 0) return;
    idleValid_ = false;
    c = tameCps(c);
    if (hasPrev_) {  // one change at a time: apply after the pending one
        pendingCps_ = true;
        nextCps_ = c;
        return;
    }
    // Switch at the first window boundary after everything already queued,
    // so nothing in the rings has to be re-timed.
    Q t = nowCycle();
    for (int i = 0; i < kSlots; i++)
        if ((slot_[i].expr || slot_[i].pending) && slot_[i].h > t) t = slot_[i].h;
    t = gridAfter(t, window());
    float f;
    int64_t s = sampleAtAnchor(cur_, t, &f);
    prev_ = cur_;
    cur_ = makeAnchor(t, s, f, c);
    hasPrev_ = true;
}

void Scheduler::retireAnchor() {
    if (!hasPrev_ || (int64_t)block_ * kBlockSamples < cur_.s0) return;
    hasPrev_ = false;
    if (pendingCps_) {
        pendingCps_ = false;
        setCps(nextCps_);
    }
}

// After update() queued every slot up to the lookahead: the first block at
// which one needs a query again (its horizon - lookahead; cycleAt() rounds
// down, so update() might still find nothing then, never miss work), or a
// cps anchor retires.
void Scheduler::setIdle() {
    const int64_t now = (int64_t)block_ * kBlockSamples;
    int64_t until = now + (int64_t)lookahead_ + (int64_t)kBlockSamples * 4096;  // re-check now and then
    for (int i = 0; i < kSlots; i++) {
        const Slot& s = slot_[i];
        if (!s.expr && !s.pending) continue;
        int64_t need = sampleAt(s.h) - (int64_t)lookahead_;
        if (need < until) until = need;
    }
    if (hasPrev_ && cur_.s0 < until) until = cur_.s0;
    if (until <= now) return;
    idleUntil_ = (uint32_t)(until / kBlockSamples);
    idleValid_ = true;
}

void Scheduler::fail(int i, const char* msg) {
    Slot& s = slot_[i];
    s.err = Error();
    s.err.set(-1, "%s", msg);
    s.failed = true;
    if (s.expr) live_->freeTree(s.expr);
    s.expr = 0;
}

// Query [s, e) of slot i into its ring. False: the ring has no room yet
// (try later) or the slot failed.
bool Scheduler::queryWindow(int i, Q s, Q e) {
    Slot& sl = slot_[i];
    Ring& r = ring_[i];
    Q minSpan = window() / qint(64);
    int keyNudge = controlKey("nudge");
    float fs = (float)fsNum_ / (float)fsDen_;
    for (;;) {
        Error err;
        stats_.queries++;
        mapper_->begin(i);
        int n = queryPattern(*live_, sl.expr, Arc{s, e}, buf_, bufCap_, err);
        bool tooMany = n < 0;
        int cnt = 0;
        const Anchor& a = anchorFor(s);
        float spc = (float)a.num / (float)a.den;
        for (int j = 0; j < n && !tooMany; j++) {
            const Event& ev = buf_[j];
            if (!ev.hasWhole || !(ev.whole.s == ev.part.s)) continue;
            if (cnt == kRingSize) {
                tooMany = true;
                break;
            }
            float frac;
            int64_t smp = sampleAtAnchor(a, ev.whole.s, &frac);
            if (ev.isMap)
                for (int c = 0; c < ev.nctl; c++)
                    if (ev.ctl[c].key == keyNudge) {
                        float sh = frac + ev.ctl[c].f * fs;
                        int64_t ish = (int64_t)sh;
                        if ((float)ish > sh) ish--;
                        smp += ish;
                    }
            Q len = ev.whole.e - ev.whole.s;
            Entry& out = tmp_[cnt];
            memset(&out, 0, sizeof out);
            if (!mapper_->map(*live_, i, ev, spc, (float)len.n / (float)len.d * spc, out)) continue;
            out.block = (uint32_t)(smp / kBlockSamples);
            out.offset = (uint8_t)(smp % kBlockSamples);
            // stable insertion sort by time (pattern order for ties)
            int p = cnt++;
            Entry tmp = out;
            while (p > 0 && (tmp_[p - 1].block > tmp.block ||
                             (tmp_[p - 1].block == tmp.block && tmp_[p - 1].offset > tmp.offset))) {
                tmp_[p] = tmp_[p - 1];
                p--;
            }
            tmp_[p] = tmp;
        }
        if (tooMany || (uint32_t)cnt > r.space()) {
            if (tooMany || cnt > kRingSize / 2) {
                Q half = (e - s) / qint(2);
                if (!(half < minSpan)) {  // retry with half the window
                    e = s + half;
                    stats_.splits++;
                    continue;
                }
                fail(i, n < 0 ? err.msg : "too many events in one window");
                return false;
            }
            return false;  // wait for the interrupt to drain the ring
        }
        mapper_->commit(i, tmp_, cnt);
        uint32_t t = r.tail;
        for (int j = 0; j < cnt; j++) r.e[(t + j) & (kRingSize - 1)] = tmp_[j];
        fence();  // entries before the new tail
        r.tail = t + cnt;
        sl.h = e;
        return true;
    }
}

int Scheduler::update(uint32_t (*clockUs)(), uint32_t budgetUs) {
    if (idleValid_ && (int32_t)(block_ - idleUntil_) < 0) {
        stats_.idleUpdates++;
        return 0;
    }
    idleValid_ = false;
    retireAnchor();
    int64_t now = (int64_t)block_ * kBlockSamples;
    Q nowC = cycleAt(now);
    Q target = cycleAt(now + lookahead_);
    uint32_t t0 = clockUs ? clockUs() : 0;
    uint32_t waiting = 0;  // slots whose ring is full this round
    int queries = 0;
    for (;;) {
        if (clockUs && clockUs() - t0 >= budgetUs) break;
        int i = -1;
        for (int j = 0; j < kSlots; j++) {
            const Slot& s = slot_[j];
            if ((!s.expr && !s.pending) || (waiting >> j & 1) || !(s.h < target)) continue;
            if (i < 0 || s.h < slot_[i].h) i = j;
        }
        if (i < 0) {
            if (!waiting) setIdle();  // all queued: nothing to do for a while
            break;
        }
        Slot& s = slot_[i];
        // stalled (e.g. project load): skip what can no longer play in time
        if (sampleAt(s.h) + (int64_t)lateBlocks_ * kBlockSamples < now) {
            s.h = nowC;
            stats_.skips++;
        }
        if (s.pending && !(s.h < s.at)) {
            if (s.expr) live_->freeTree(s.expr);
            s.expr = s.next;
            s.next = 0;
            s.pending = false;
            if (s.h > s.at) truncate(i, sampleAt(s.at));  // after a skip
        }
        if (!s.expr) {  // silent until the switch (or for good)
            if (s.pending) s.h = s.at;
            continue;
        }
        Q w = window();
        Q e = gridAfter(s.h, w);
        if (s.pending && s.at < e) e = s.at;
        if (hasPrev_ && s.h < cur_.c0 && cur_.c0 < e) e = cur_.c0;
        const uint32_t q0 = clockUs ? clockUs() : 0;
        const bool queued = queryWindow(i, s.h, e);
        if (!queued) waiting |= 1u << i;
        if (clockUs) {
            const uint32_t us = clockUs() - q0;
            stats_.slotUs[i] += us;
            if (us > stats_.slotUsMax[i]) stats_.slotUsMax[i] = us;
        }
        if (queued) stats_.slotWindows[i]++;
        queries++;
    }
    return queries;
}

// ---- interrupt ---------------------------------------------------------------

void Scheduler::tick(Voice& v) {
    const uint32_t b = block_;
    if (hushReq_ != hushSeen_) {
        hushSeen_ = hushReq_;
        memset(off_, 0, sizeof off_);
        memset(nOff_, 0, sizeof nOff_);
        v.allOff();
    }
    for (int i = 0; i < kSlots; i++) {
        if (!nOff_[i]) continue;
        for (Off& o : off_[i])
            if (o.active && (int32_t)(o.block - b) <= 0) {
                o.active = false;
                nOff_[i]--;
                v.noteOff(i, o.chan, o.note, o.offset, o.tag);
            }
    }
    int starts = 0;
    for (int k = 0; k < kSlots; k++) {
        const bool sample = k < kSampleSlots;
        const int i = sample ? (rr_ + k) % kSampleSlots : k;
        Ring& r = ring_[i];
        uint32_t h = r.head;
        const uint32_t t = r.tail;
        if (h == t) continue;
        fence();  // read the entries after seeing the tail
        while (h != t) {
            const Entry& e = r.e[h & (kRingSize - 1)];
            int32_t late = (int32_t)(b - e.block);
            if (late < 0) break;
            if ((uint32_t)late > lateBlocks_) {
                stats_.dropped++;
                h++;
                continue;
            }
            // a later start due now that keeps fewer than k others ringing
            // (its own `voices`, at most players_) would cut this one anyway:
            // the k-th start after it cuts it if k >= that start's voices
            if (sample && players_ > 0) {
                bool cut = false;
                for (int k = 1; h + k != t && k <= players_ && !cut; k++) {
                    const Entry& later = r.e[(h + k) & (kRingSize - 1)];
                    if ((int32_t)(b - later.block) < 0) break;
                    const int v = later.voices && later.voices < players_ ? later.voices : players_;
                    cut = k >= v;
                }
                if (cut) {
                    stats_.superseded++;
                    h++;
                    continue;
                }
            }
            if (muted_ & (1u << i)) {  // muted: consumed on time, not played
                stats_.muted++;
                h++;
                continue;
            }
            if (sample && cap_ > 0 && starts >= cap_) {
                stats_.capped++;
                break;
            }
            Off* offs = off_[i];
            if (sample && players_ <= 1 && nOff_[i]) {  // one voice per sample slot: a new note cuts the old one
                for (int j = 0; j < kMaxPendingOffs; j++) offs[j].active = false;
                nOff_[i] = 0;
            }
            const uint16_t tag = v.noteOn(i, e, (uint32_t)late);
            if (sample) starts++;
            if (e.dur) {
                uint64_t at = (uint64_t)e.block * kBlockSamples + e.offset + e.dur;
                Off* slotFor = nullptr;
                for (int j = 0; j < kMaxPendingOffs && !slotFor; j++)
                    if (!offs[j].active) slotFor = &offs[j];
                if (!slotFor) {  // full: release the oldest now
                    slotFor = &offs[0];
                    for (int j = 1; j < kMaxPendingOffs; j++)
                        if ((int32_t)(offs[j].block - slotFor->block) < 0) slotFor = &offs[j];
                    v.noteOff(i, slotFor->chan, slotFor->note, 0, slotFor->tag);
                } else {
                    nOff_[i]++;
                }
                slotFor->block = (uint32_t)(at / kBlockSamples);
                slotFor->offset = (uint8_t)(at % kBlockSamples);
                slotFor->note = e.note;
                slotFor->chan = e.chan;
                slotFor->tag = tag;
                slotFor->active = true;
            }
            h++;
        }
        fence();  // done with the entries before publishing the head
        r.head = h;
    }
    rr_ = (uint8_t)((rr_ + 1) % kSampleSlots);
    block_ = b + 1;
}

}  // namespace tc
