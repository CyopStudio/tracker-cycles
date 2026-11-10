// The Tidal scheduler's device side (plan 4c step 3): the project's
// instruments for the mapper (DeviceHost) and the voices the audio
// interrupt drives (DeviceVoice: playerEngine::tidalNoteOn on tracks 1-8,
// MIDI out for d9-d16). Only with -DTIDAL_TICK for now.
#pragma once
#ifdef TIDAL_TICK
#include "../../tidal/core/mapping.h"

// mtProject's 48 sample instruments.
class DeviceHost : public tc::TrackerHost {
public:
    int instrumentCount() const override;
    tc::InstrumentInfo instrument(int i) const override;
    // nearest step of the Tracker's volume curve (ampLogValues) to ratio x
    // the instrument's own volume
    uint8_t volumeStep(int i, float ratio) const override;
};

// Called from the scheduler's tick() in the audio interrupt.
//
// Polyphony (docs/tidal_voices.md): each track has kPlayers sample players
// (playerEngine::kTidalPlayers) feeding its one crusher / filter / amp
// chain; at most cap() of all 24 sound at once. A note with `voices N`
// (Entry::voices, default 1) lets at most N notes ring on its track, itself
// included: if more already sound, the oldest is retriggered with the new
// note and any further extra ones fade out (tidalCut). Otherwise it takes a
// free player while under the cap; else its track's oldest sounding one
// (retrigger); else (its track silent, cap reached) the oldest sounding
// player of any track fades out for it. A player started in the last 2
// blocks counts as sounding (it may not report isPlaying() yet).
// Streamed instruments (VoiceStreamManager: one stream per track) always
// use player 0. The noteOn tag is player | (that player's note serial << 2):
// a noteOff for a note whose player was taken since is ignored.
class DeviceVoice : public tc::Voice {
public:
    static constexpr int kTracks = 8;
    static constexpr int kPlayers = 3;
    // The cap is load-driven (v10): at most cap() (default kCap), less as
    // the audio load rises: kCap below kLoadLo % (smoothed), falling in a
    // line to kMinCap at kLoadHi %. Over it at a noteOn, the oldest
    // sounding players fade out (governor cuts).
    static constexpr int kCap = 16;
    static constexpr int kMinCap = 8;
    static constexpr int kLoadLo = 70, kLoadHi = 85;
    void setCap(int c) { cap_ = c < 1 ? 1 : c > kTracks * kPlayers ? kTracks * kPlayers : c; }
    int cap() const { return cap_; }
    // Audio interrupt, once per block: the last callback's time in us.
    void blockLoad(int us);
    int loadPercent() const { return (int)(loadQ8_ >> 8); }
    int effectiveCap() const;

    uint16_t noteOn(int slot, const tc::Entry& e, uint32_t late) override;
    void noteOff(int slot, uint8_t chan, uint8_t note, uint8_t offset, uint16_t tag) override;
    void allOff() override;  // hush: every track's notes, MIDI all notes off

    // counters for the tick test (interrupt writes, main loop reads)
    struct Stats {
        uint32_t trackSteals;   // notes that retriggered their track's oldest sounding player
        uint32_t voicesCuts;    // extra notes faded out by a note's `voices` limit
        uint32_t governorCuts;  // notes faded out because the load lowered the cap
        uint32_t capMin;        // lowest effective cap at a noteOn (0 = none yet)
        uint32_t capSum;        // sum of the effective cap over sample noteOns
        uint32_t loadMax;       // highest smoothed load seen (%)
        uint32_t globalSteals;  // notes that stopped another track's player (cap)
        uint32_t staleOffs;     // noteOffs ignored: their player was taken since
        uint32_t maxSounding;   // most players sounding at a noteOn (before it)
        uint32_t soundingSum;   // sum over sample noteOns, for the average
        uint32_t picks;         // sample noteOns (non-streamed instruments)
    };
    Stats vstats{};

private:
    int pickPlayer(int slot, uint8_t instr, int voices, uint32_t now);
    bool oldestSounding(uint32_t now, int& t, int& k) const;
    uint32_t loadQ8_ = 0;  // smoothed audio load, % << 8
    bool sounding(int t, int k, uint32_t now) const;
    void cut(int t, int k);
    int cap_ = kCap;
    uint32_t clock_ = 0;
    uint32_t started_[kTracks][kPlayers] = {};
    uint32_t startBlock_[kTracks][kPlayers] = {};
    bool cutting_[kTracks][kPlayers] = {};  // faded out by cut(), until its next note
    uint32_t cutBlock_[kTracks][kPlayers] = {};
    uint32_t now_ = 0;  // block of the noteOn being placed
    uint16_t serial_[kTracks][kPlayers] = {};
};
#endif
