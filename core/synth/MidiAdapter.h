#pragma once
#ifdef FEATURE_SYNTH
#include "Platform.h"
#include "PolySynth/MidiSource.h"
#include "config.h"

#include <utility>

class MidiAdapter
  : public polysynth::MidiSource
  , public ::Midi
{
  public:
    static constexpr auto kTrackCCRange = std::pair{ 110, 112 };

    MidiAdapter(std::function<void(int channel, int program)> onProgramChange)
      : polysynth::MidiSource(Type::kInternalKeyboard)
      , onProgramChange(std::move(onProgramChange)){};

    void subscribeMidiEvents(int instrumentId, std::weak_ptr<MidiEventCallback> callback) override;
    // Tracker doesn't have separated output and input for MIDI, so this implementation ignore input
    // Part of API
    virtual bool isReadAvailable() override
    {
        assert(false);
        return false;
    }
    virtual void read() override { assert(false); }
    virtual void sendRealTime(MessageType messageType) override;
    virtual void sendControlChange(uint8_t index, uint8_t value, uint8_t channel) override;
    virtual void sendProgramChange(uint8_t programNumber, uint8_t channel) override;
    virtual void sendAfterTouch(uint8_t velocity, uint8_t channel) override;
    virtual void sendPolyPressure(uint8_t note, uint8_t value, uint8_t channel) override;
    virtual void sendPitchBend(int value, uint8_t channel) override;
    virtual void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) override;
    virtual void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) override;

    uint8_t getTrackedCCValue(int channel, int ccId);

  private:
    // tracker send channel == 1 for 1st Midi instrument
    void translateChannel(uint8_t& channel)
    {
        assert(channel > 0 && channel <= kMidiChannelsPerPort);

        channel--;
    }

    std::vector<std::weak_ptr<MidiEventCallback>> callbacks;
    std::array<std::array<uint8_t, kTrackCCRange.second - kTrackCCRange.first>,
               kMidiChannelsPerPort>
      trackedCCValues = {};
    std::function<void(int channel, int program)> onProgramChange;
};
#endif
