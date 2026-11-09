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
class DeviceVoice : public tc::Voice {
public:
    void noteOn(int slot, const tc::Entry& e, uint32_t late) override;
    void noteOff(int slot, uint8_t chan, uint8_t note, uint8_t offset) override;
    void allOff() override;  // hush: every track's note, MIDI all notes off
};
#endif
