// Tidal controls -> Tracker note entries (plan step 4c, design
// docs/tidal_control_mapping.md). Runs on the main loop as the scheduler's
// Mapper: resolves `s` to an instrument, `n` to a slice or pitch, and
// converts every control to Tracker units, so the interrupt only copies
// values. Portable (host-tested); the device describes its instruments
// through TrackerHost.
#pragma once
#include <stdint.h>

#include "scheduler.h"

namespace tc {

// Per-note overrides carried in Entry::modId / modVal.
enum class TMod : uint8_t {
    None = 0,
    Slice,      // slice index (Slice / Beat Slice instruments)
    FineTune,   // cents, int16 (-50..50 from fractional pitch)
    CutoffLP,   // filter on, low-pass; value 0..65535 = the filter's cutoff 0..1
    CutoffHP,   // high-pass
    CutoffBP,   // band-pass
    Pan,        // 0..100 (50 = centre)
    Begin,      // start point 0..65535 of the sample
    End,        // end point 0..65535
    Reverse,    // 1 = play backwards (speed < 0)
    Resonance,  // 0..65535 = 0..1 of the Tracker's resonance range
    Release,    // amp envelope release, ms
    Attack,     // amp envelope attack, ms
    Overdrive,  // 0..100
    BitDepth,   // 4..16
    Reverb,     // send 0..100
    Delay,      // send 0..100
    Cc,         // MIDI: number << 8 | value
    Bend,       // MIDI: pitch bend 0..16383 (8192 = none)
    Count
};

// `voices` (Tracker Cycles' own control): notes one sample track lets ring,
// the new one included; the device has 3 players per track.
constexpr int kMaxVoices = 3;

enum class InstrKind : uint8_t { OneShot, Loop, Slice, Granular, Wavetable };

struct InstrumentInfo {
    bool active;
    InstrKind kind;
    uint8_t slices;   // Slice kinds: number of slices
    const char* name; // file name, e.g. "BD 01.wav" (matched without extension)
};

// What the mapper needs to know about the project (device: mtProject).
class TrackerHost {
public:
    virtual int instrumentCount() const = 0;
    virtual InstrumentInfo instrument(int i) const = 0;
    // Volume step 0..100 that plays `ratio` x the amplitude of instrument
    // i's own volume (ratio 1 = the instrument's volume).
    virtual uint8_t volumeStep(int i, float ratio) const = 0;
    // An `s` name no instrument file matches: an instrument to use instead,
    // or -1 to skip the event (default). Test builds use it for aliases.
    virtual int fallbackInstrument(const char* name) const {
        (void)name;
        return -1;
    }
};

// Counters per slot, for the UI (the editor shows them as slot warnings).
struct MapWarnings {
    uint32_t unknownName;   // events skipped: no instrument for `s`
    uint32_t noSound;       // events skipped: no `s` (sample slots)
    uint32_t droppedMods;   // more than kMaxMods overrides: lowest priority dropped
    uint32_t filterClash;   // more than one of cutoff / hcutoff / bandf
    char lastUnknown[24];   // the last unknown `s` name
};

// ---- Tracker mode (design section 5) ------------------------------------------
// Can the device play library entry `libId` (a function or control) in
// block `slot` (0..7 sample tracks, 8..15 MIDI)? nullptr if yes, else why
// not, e.g. "has no effect on MIDI slots (d9-d16)". Host-only functions,
// controls the Tracker doesn't map, and controls of the other slot kind
// are refused; unknown controls are refused by default.
const char* trackerLibIssue(int libId, int slot);
// First issue in block `slot` of `p`: false with err.msg ("tracker mode:
// 'cutoff' has no effect on MIDI slots (d9-d16)") and err.node.
bool trackerCheck(const Program& p, int slot, Error& err);

// The `s` name of a sample file: the name without its extension, lower
// case, characters other than letters, digits and - _ . as _ ("BD 01" -> "bd_01")
// as '_', at most 15 characters (the code tree's word limit). The pads
// enter these names and the mapper matches `s` against them.
int sampleToken(const char* file, char* out, int cap);

class TrackerMapper : public Mapper {
public:
    void init(const TrackerHost* host, float sampleRate);
    // Call when the project's instruments change (load, rename, ...).
    void instrumentsChanged();
    bool map(const Program& p, int slot, const Event& e, float samplesPerCycle, float lenSamples,
             Entry& out) override;
    const MapWarnings& warnings(int slot) const { return warn_[slot]; }
    void resetWarnings();

    // Conversions (exposed for tests).
    static uint16_t cutoffValue(float hz);  // Hz -> the Tracker filter's 0..1 as 0..65535
    int findInstrument(const char* name);   // -1 if none (fallback included)

private:
    struct CacheSlot {
        char name[24];
        int8_t instr;  // -2 = empty
    };
    static constexpr int kCache = 64;

    const TrackerHost* host_ = nullptr;
    float fs_ = 44100.0f;
    CacheSlot cache_[kCache] = {};
    MapWarnings warn_[kSlots] = {};
    // control keys (engine.h controlKey), resolved once
    // (all initialised: a static TrackerMapper is constant-initialised, no
    // static constructor; CLAUDE.md SDRAM gotcha)
    int kS_ = -1, kN_ = -1, kNote_ = -1, kSpeed_ = -1, kGain_ = -1, kAmp_ = -1, kVelocity_ = -1, kPan_ = -1,
        kCutoff_ = -1, kHcutoff_ = -1, kBandf_ = -1, kRes_ = -1, kHres_ = -1, kShape_ = -1, kDistort_ = -1,
        kCrush_ = -1, kRoom_ = -1, kDelay_ = -1, kBegin_ = -1, kEnd_ = -1, kAttack_ = -1, kRelease_ = -1,
        kLegato_ = -1, kSustain_ = -1, kMidichan_ = -1, kCcn_ = -1, kCcv_ = -1, kBend_ = -1, kVoices_ = -1;
};

}  // namespace tc
