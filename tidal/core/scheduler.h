// Scheduler (plan step 4b, design docs/tidal_scheduler.md): queries each
// slot's pattern on the main loop a little ahead of time into per-slot
// single-producer / single-consumer rings; the audio interrupt (tick())
// only pops due entries and calls the voice interface.
//
// Portable, no heap. Two sides:
//   main loop: evaluate(), hush(), setCps(), update()   (the producer)
//   interrupt: tick()                                     (the consumer)
// Times on the main loop are samples (int64); entries carry a 32-bit block
// index (block = 128 samples) plus a sample offset inside the block.
#pragma once
#include <stdint.h>

#include "engine.h"
#include "qtime.h"
#include "tree.h"

namespace tc {

constexpr int kBlockSamples = 128;
constexpr int kRingSize = 128;           // entries per slot (power of two)
constexpr int kMaxMods = 10;
constexpr int kMaxPendingOffs = 8;       // per slot (MIDI chords)

enum class EntryKind : uint8_t { Note = 0 };

// One trigger, 48 bytes. The control mapping (step 4c, mapping.h) fills
// everything but the timing; the interrupt applies it as given.
struct Entry {
    uint32_t block;      // absolute block index of the onset
    uint32_t dur;        // samples until the noteOff (0 = none, plays out)
    uint8_t offset;      // sample within the block (0..127)
    EntryKind kind;
    uint8_t chan;        // instrument (sample slots) / MIDI channel
    uint8_t note;
    uint8_t vel;         // sample: volume step 0..100, 255 = instrument's; MIDI: velocity
    uint8_t nmods;
    uint8_t modId[kMaxMods];
    uint16_t modVal[kMaxMods];
    uint8_t voices;      // sample: notes the slot lets ring, this one included (1-3; `voices`, default 1)
    uint8_t pad[1];      // pad[0]: the scheduler's own (lit steps: index while queueing)
};
static_assert(sizeof(Entry) == 48, "Entry must stay 48 bytes");

// Event -> entry (step 4c). Called on the main loop for every onset.
// `lenSamples` is the event's whole length in samples (for legato).
// Returns false to skip the event.
class Mapper {
public:
    virtual bool map(const Program& p, int slot, const Event& e, float samplesPerCycle, float lenSamples,
                     Entry& out) = 0;
    // Optional (host backends that keep more per entry than 32 bytes):
    // begin() before each query attempt's map() calls; commit() with the
    // sorted entries just before they are queued (may rewrite them).
    virtual void begin(int slot) { (void)slot; }
    virtual void commit(int slot, Entry* e, int n) { (void)slot, (void)e, (void)n; }
};

// What the interrupt drives (playerEngine / MIDI on the device, a recorder
// in tests). Called from tick() only.
class Voice {
public:
    // `late` = blocks after the scheduled block (note-start cap, jitter).
    // Returns a tag the scheduler hands back to this note's noteOff (the
    // device: which of the slot's players, and that player's note number,
    // so a noteOff for a stolen voice is ignored).
    virtual uint16_t noteOn(int slot, const Entry& e, uint32_t late) = 0;
    // `chan` = the noteOn entry's chan (MIDI: the channel it went out on)
    virtual void noteOff(int slot, uint8_t chan, uint8_t note, uint8_t offset, uint16_t tag) = 0;
    virtual void allOff() = 0;
};

// Single-producer / single-consumer ring of entries, in time order. The
// entries live in caller storage (SDRAM on the device); head / tail sit in
// the Scheduler object (internal RAM on the device: the interrupt reads
// them every block).
struct Ring {
    Entry* e;            // kRingSize entries
    volatile uint32_t head;  // written by the consumer
    volatile uint32_t tail;  // written by the producer
    uint32_t count() const { return tail - head; }
    uint32_t space() const { return kRingSize - count(); }
};

struct SchedulerStats {
    uint32_t dropped;     // entries too late when due (interrupt)
    uint32_t capped;      // note starts moved to a later block by the cap
    uint32_t skips;       // horizon skipped forward after a stall
    uint32_t queries;     // queries run
    uint32_t splits;      // windows halved (too many events / ring space)
    uint32_t superseded;  // mono slots: due starts skipped for a later due one
    uint32_t muted;       // due starts of muted slots (not played)
    uint32_t idleUpdates; // update() calls that returned at once (nothing due yet)
    // per slot, when update() gets a clock: time in query windows (query,
    // mapping, sort; retries included), windows queued, longest window
    uint32_t slotUs[kSlots];
    uint32_t slotWindows[kSlots];
    uint32_t slotUsMax[kSlots];
};

// Lit steps (editor highlighting on the device, TC_EVENT_SRC): when an entry
// plays, the interrupt records the live program's source steps of its event.
constexpr int kLitSrc = 2;     // steps kept per entry
constexpr int kLitRing = 256;  // records between two takeLit() calls
struct LitRecord {
    uint32_t block;   // onset block
    uint32_t dur;     // samples (0 = plays out)
    uint8_t offset;   // sample in the block
    uint8_t slot;
    uint8_t n;        // src used
    uint8_t pad;
    uint16_t src[kLitSrc];  // live program node ids
};
static_assert(sizeof(LitRecord) == 16, "LitRecord must stay 16 bytes");

// Interrupt hooks (device: disable / enable IRQs; host: none).
struct SchedulerHooks {
    void (*lock)();
    void (*unlock)();
};

class Scheduler {
public:
    using Hooks = SchedulerHooks;
    // Entries of caller storage init() needs: the rings + a sort buffer.
    static constexpr int kStorageEntries = (kSlots + 1) * kRingSize;
    // No constructor work: placement-new it at run time (SDRAM is not
    // ready for static constructors on the device), then call init().
    // `live` is the scheduler's own program (evaluate copies blocks into
    // it); `buf` / `cap` the query output buffer; `storage` holds
    // kStorageEntries entries (main loop + interrupt, may be slow memory).
    // The object itself (~4 KB) holds what the interrupt reads every block.
    void init(Program* live, Event* buf, int cap, Mapper* mapper, Entry* storage,
              Hooks hooks = Hooks{nullptr, nullptr});

    // Sample rate as an exact fraction (MK66: 750000/17 = 44117.647 Hz).
    void setSampleRate(int64_t num, int64_t den);
    void setLookahead(uint32_t samples) {
        lookahead_ = samples;
        idleValid_ = false;
    }
    void setLateLimit(uint32_t blocks) { lateBlocks_ = blocks; }
    void setNoteStartCap(int n) { cap_ = n; }
    // Sample slots play at most n notes at a time (the device: n players
    // per track, docs/tidal_voices.md), and a start keeps at most its
    // Entry::voices - 1 others ringing (0 = n). Starts due in the same
    // block that a later one would cut at once are skipped (counted in
    // `superseded`, not by the cap): with voices 1 only the last plays.
    // n = 1: one note at a time, and a new note also cancels the slot's
    // pending noteOffs (as does n = 0, the default: no skipping). n >= 2:
    // noteOffs stay pending, each goes back with its noteOn's tag.
    void setSlotPlayers(int n) { players_ = n; }
    // Mute (Tidal's mute / solo, editor v2 Play + pad): a muted slot keeps
    // being queried and keeps its timing, but its notes don't play.
    void setMuted(int slot, bool on) {
        const uint16_t b = (uint16_t)(1u << slot);
        muted_ = (uint16_t)(on ? (muted_ | b) : (muted_ & ~b));
    }
    bool muted(int slot) const { return (muted_ >> slot) & 1; }
    void unmuteAll() { muted_ = 0; }
    void setMonoSlots(bool on) { players_ = on ? 1 : 0; }
    // Lit steps (optional; caller storage, may be slow memory): `src` holds
    // kLitSrcWords, `ring` kLitRing records. Without it nothing is kept.
    static constexpr int kLitSrcWords = (kSlots + 1) * kRingSize * kLitSrc;
    void setLitStorage(uint16_t* src, LitRecord* ring) {
        litSrc_ = src;
        litRing_ = ring;
        litHead_ = litTail_ = 0;
    }
    // Main loop: the records since the last call, oldest first (if more than
    // kLitRing came, the oldest are gone). Returns how many were written.
    int takeLit(LitRecord* out, int cap);

    // ---- main loop ----------------------------------------------------------
    // Evaluate block `slot` of `edit` (a checked program): copies its tree
    // into the live program, switching at the time its launch mode gives.
    // False (and `err`) if the live program is full. `liveRoot` (optional)
    // gets the copy's root in the live program (0 for an empty block): the
    // copy has the same shape, so a backend can map its node ids (e.g. the
    // events' source steps) back to `edit`.
    bool evaluate(const Program& edit, int slot, Error& err, uint16_t* liveRoot = nullptr);
    void silence(int slot);           // `dN silence`, switching like evaluate
    void hush();                      // stop everything now
    // Cycles per second (exact). Takes effect at the window boundary after
    // everything already queued (<= lookahead + one window from now).
    void setCps(Q cps);
    Q cps() const { return pendingCps_ ? nextCps_ : cur_.cps; }
    // Fill the rings. `clockUs` / `budgetUs` time-box the work (nullptr =
    // until every slot is queued `lookahead` ahead). Returns queries run.
    // Cheap to call every main-loop pass: after a call that ran out of work
    // it returns at once until the first block at which a slot needs a
    // query (or a cps change takes effect); evaluate / silence / hush /
    // setCps end that early.
    int update(uint32_t (*clockUs)() = nullptr, uint32_t budgetUs = 0);

    // Slot state for the UI.
    bool playing(int slot) const { return slot_[slot].expr || slot_[slot].next; }
    const Error* error(int slot) const { return slot_[slot].failed ? &slot_[slot].err : nullptr; }
    Q horizon(int slot) const { return slot_[slot].h; }
    const SchedulerStats& stats() const { return stats_; }
    void resetStats();  // main loop; the interrupt's counters may lose a tick

    // Time conversion (main loop). Cycle -> absolute sample (floor).
    int64_t sampleAt(Q cycle) const;
    // Sample -> cycle, rounded down to 1/kNowGrid of a cycle.
    Q cycleAt(int64_t sample) const;
    static constexpr int kNowGrid = 4096;
    Q window() const;  // current query window W (cycles)
    // Cycle the latest cps change takes effect at (0 if none yet).
    Q anchorCycle() const { return cur_.c0; }

    // ---- interrupt -----------------------------------------------------------
    // Called once per audio block: plays due entries, then advances the
    // block clock.
    void tick(Voice& v);
    uint32_t block() const { return block_; }

private:
    struct Anchor {
        Q c0;          // cycle
        int64_t s0;    // sample (integer part) at c0
        float s0f;     // fractional sample at c0, [0, 1)
        Q cps;
        int64_t num;   // samples per cycle = num / den
        int64_t den;
    };
    struct Slot {
        uint16_t expr;     // live program root playing now (0 = none)
        uint16_t next;     // pending root from `at` on (0 = silence)
        bool pending;      // a switch at `at` is pending
        Q at;              // switch cycle
        Q h;               // horizon: everything before h is queued
        bool failed;
        Error err;
    };
    struct Off {
        uint32_t block;
        uint8_t offset;
        uint8_t note;
        uint8_t chan;
        bool active;
        uint16_t tag;  // what the noteOn returned
    };

    Anchor makeAnchor(Q c0, int64_t s0, float s0f, Q cps) const;
    const Anchor& anchorFor(Q cycle) const;
    int64_t sampleAtAnchor(const Anchor& a, Q cycle, float* frac) const;
    Q nowCycle() const;
    void retireAnchor();
    void truncate(int slot, int64_t fromSample);
    void switchTo(int slot, uint16_t root, Q at);
    bool queryWindow(int slot, Q s, Q e);
    void fail(int slot, const char* msg);
    void setIdle();

    Program* live_;
    Event* buf_;
    int bufCap_;
    Mapper* mapper_;
    Hooks hooks_;
    int64_t fsNum_, fsDen_;
    uint32_t lookahead_;
    uint32_t lateBlocks_;
    int cap_;
    int players_;
    volatile uint16_t muted_;  // slot bits; written on the main loop, read by tick()
    bool idleValid_;       // update() may return at once before idleUntil_
    uint32_t idleUntil_;   // block

    Anchor cur_;           // valid from cur_.c0 on
    Anchor prev_;          // valid before cur_.c0 while hasPrev_
    bool hasPrev_;
    bool pendingCps_;      // a cps request waiting for prev_ to retire
    Q nextCps_;

    Slot slot_[kSlots];
    Entry* tmp_;           // kRingSize entries of the caller's storage
    SchedulerStats stats_;

    // interrupt side
    Ring ring_[kSlots];
    volatile uint32_t block_;
    volatile uint32_t hushReq_;   // incremented by hush()
    uint32_t hushSeen_;
    uint8_t rr_;                  // round-robin start among sample slots
    Off off_[kSlots][kMaxPendingOffs];
    uint8_t nOff_[kSlots];        // active entries per off_ row

    // lit steps (setLitStorage)
    uint16_t* litSrc_;            // per slot ring position, then the sort buffer's row
    LitRecord* litRing_;
    volatile uint32_t litHead_;   // written by the main loop
    volatile uint32_t litTail_;   // written by the interrupt
};

}  // namespace tc
