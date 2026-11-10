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
// chain; at most kCap of all 24 sound at once. A note takes a free player
// of its track while under the cap; else its track's oldest sounding one
// (retrigger, as one voice per track did); else (its track silent, cap
// reached) the oldest sounding player of any track is stopped for it.
// Streamed instruments (VoiceStreamManager: one stream per track) always
// use player 0. The noteOn tag is player | (that player's note serial << 2):
// a noteOff for a note whose player was taken since is ignored.
class DeviceVoice : public tc::Voice {
public:
    static constexpr int kTracks = 8;
    static constexpr int kPlayers = 3;
    static constexpr int kCap = 16;

    uint16_t noteOn(int slot, const tc::Entry& e, uint32_t late) override;
    void noteOff(int slot, uint8_t chan, uint8_t note, uint8_t offset, uint16_t tag) override;
    void allOff() override;  // hush: every track's notes, MIDI all notes off

    // counters for the tick test (interrupt writes, main loop reads)
    struct Stats {
        uint32_t trackSteals;   // notes that retriggered their track's oldest sounding player
        uint32_t globalSteals;  // notes that stopped another track's player (cap)
        uint32_t staleOffs;     // noteOffs ignored: their player was taken since
        uint32_t maxSounding;   // most players sounding at a noteOn (before it)
        uint32_t soundingSum;   // sum over sample noteOns, for the average
        uint32_t picks;         // sample noteOns (non-streamed instruments)
    };
    Stats vstats{};

private:
    int pickPlayer(int slot, uint8_t instr);
    uint32_t clock_ = 0;
    uint32_t started_[kTracks][kPlayers] = {};
    uint16_t serial_[kTracks][kPlayers] = {};
};
#endif
