// See mtSequencer.h (Tracker Cycles plan 4a: the slim sequencer).
#include "mtSequencer.h"

#include "mtAudioEngine.h"
#include "polyassert.h"
#include <cr_section_macros.h>

Sequencer sequencer;

extern audioEngine engine;

const float Sequencer::MAX_TEMPO = 800.0f;
const float Sequencer::MIN_TEMPO = 40.0f;
const float Sequencer::TEMPO_FX_STP = 4.0f;
const float Sequencer::MIN_TEMPO_FX = 5.0f;
const float Sequencer::MAX_TEMPO_FX = 200.0f;

void Sequencer::init()
{
    for (LiveNote& n : live_)
        n = LiveNote{};
}

AUDIO_CORE_RAM void Sequencer::tick(float intervalSeconds)
{
    // a running clock in microsteps (48 per 16th) for tempo-synced LFOs
    const float uStepsPerSecond = getCurrentTempo() / 60.0f * 4.0f * 48.0f;
    timerAccu_ += intervalSeconds * uStepsPerSecond;
    while (timerAccu_ >= 1.0f)
    {
        timerAccu_ -= 1.0f;
        timerTicks_++;
    }
}

void Sequencer::playPattern()
{
    if (transportPlay_)
        transportPlay_();
}

void Sequencer::stop()
{
    stopManualNotes();
    if (transportStop_)
        transportStop_();
}

AUDIO_CORE_RAM float Sequencer::getStepPeriodMs(void)
{
    return 60000.0f / (getCurrentTempo() * 4.0f);
}

// ---- live notes ----------------------------------------------------------------
// As the stock sequencer's "play mode" path: the project's last used
// instrument on the first sample track without an open live note (MIDI
// instruments go out on their channel).

void Sequencer::handleNoteOn(uint8_t, uint8_t note, int8_t velocity, int16_t source)
{
    if (velocity < 0)
        velocity = 127;
    const int instrument = mtProject.values.lastUsedInstrument;
    velocity = (int8_t)mapClamped<int>(velocity, 0, 127, 0, getInstrumentVelo(instrument));
    lastLiveNote_ = (int8_t)note;
    if (isMidiInstrument(instrument) || isSynthInstrument(instrument))
    {
        sendMidiNoteOn(note, velocity, instrumentIdxToMidiChannel(instrument), isSynthInstrument(instrument));
        for (LiveNote& n : live_)
            if (!n.open)
            {
                n = LiveNote{ true, note, (uint8_t)instrument, source };
                break;
            }
        return;
    }
    for (int tr = 0; tr < TRACKS; tr++)
    {
        LiveNote& n = live_[tr];
        if (n.open || !isSampleTrack(tr))
            continue;
        n = LiveNote{ true, note, (uint8_t)instrument, source };
        instrumentPlayer[tr].noteOff();
        instrumentPlayer[tr].noteOn(
          instrument, note, velocityToVolume(velocity), fx.FX_TYPE_VOLUME, 0, 0, 0);
        engine.setLastUsedVoice(tr);
        return;
    }
}

void Sequencer::handleNoteOff(uint8_t, uint8_t note, uint8_t, int16_t source)
{
    for (int tr = 0; tr < TRACKS; tr++)
    {
        LiveNote& n = live_[tr];
        if (!n.open || !((source >= 0 && n.source == source) || (source < 0 && n.note == note)))
            continue;
        if (isMidiInstrument(n.instrument) || isSynthInstrument(n.instrument))
            sendMidiNoteOff(n.note, 0, instrumentIdxToMidiChannel(n.instrument), isSynthInstrument(n.instrument));
        else
            instrumentPlayer[tr].noteOff();
        n.open = false;
        return;
    }
}

AUDIO_CORE_RAM void Sequencer::stopManualNotes(void)
{
    for (int tr = 0; tr < TRACKS; tr++)
    {
        LiveNote& n = live_[tr];
        if (!n.open)
            continue;
        if (isMidiInstrument(n.instrument) || isSynthInstrument(n.instrument))
            sendMidiNoteOff(n.note, 0, instrumentIdxToMidiChannel(n.instrument), isSynthInstrument(n.instrument));
        else
            instrumentPlayer[tr].noteOff();
        n.open = false;
    }
}

// ---- velocity / volume ------------------------------------------------------------

int8_t Sequencer::velocityToVolume(int8_t velo)
{
    if (velo < 0)
        return velo;
    velo = clamp<int>(velo, 0, 127);
    return (int8_t)std::roundf(mapClamped<float>(velo, 0, 127, 0, 100));
}

int8_t Sequencer::volumeToVelocity(int8_t vol)
{
    if (vol < 0)
        return vol;
    vol = clamp<int>(vol, 0, 100);
    return (int8_t)std::roundf(mapClamped<float>(vol, 0, 100, 0, 127));
}

uint8_t Sequencer::getInstrumentVelo(uint8_t instrumentIdx)
{
    if (isMidiInstrument(instrumentIdx))
    {
        int midiInstrumentIdx = instrumentIdxToMidiChannel(instrumentIdx) - 1;
        return mtProject.values.midiInstrument[midiInstrumentIdx].velocity;
    }
    if (isSampleInstrument(instrumentIdx))
        return volumeToVelocity(mtProject.instrument[instrumentIdx].volume);
    return 100;
}

// ---- FX value ranges (unchanged from the stock sequencer) --------------------------

AUDIO_CORE_RAM int16_t Sequencer::getFxMax(uint8_t fxID, bool forRandom)
{
    switch (fxID)
    {
        case fx.FX_TYPE_VOLUME: return 100;
        case fx.FX_TYPE_PANNING: return 100;
        case fx.FX_TYPE_MICROTUNING: return 198;
        case fx.FX_TYPE_CHANCE: return forRandom ? fx.CHANCE_PROB_MAX : fx.CHANCE_MAX;

        case fx.FX_TYPE_BREAK_PATTERN: return 1;

        case fx.FX_TYPE_MICROMOVE: return 100;
        case fx.FX_TYPE_TEMPO: return MAX_TEMPO_FX;
        case fx.FX_TYPE_SWING: return 75;
        case fx.FX_TYPE_OFF: return 0;
        case fx.FX_TYPE_RANDOM_INSTRUMENT: return 47;
        case fx.FX_TYPE_RANDOM_VALUE: return 255;
        case fx.FX_TYPE_SAMPLE_SLICE: return 47;
        case fx.FX_TYPE_REVERSE_PLAYBACK: return 1;
        case fx.FX_TYPE_ROLL: return (fx.ROLL_PERIOD_MAX + 1) * (fx.rollNoteDir_max + 1) - 1;

        case fx.FX_TYPE_SEND_CC_A:
        case fx.FX_TYPE_SEND_CC_B:
        case fx.FX_TYPE_SEND_CC_C:
        case fx.FX_TYPE_SEND_CC_D:
        case fx.FX_TYPE_SEND_CC_E:
        case fx.FX_TYPE_SEND_CC_F: return 127;

        case fx.FX_TYPE_FILTER_LFO:
        case fx.FX_TYPE_PANNING_LFO:
        case fx.FX_TYPE_FINETUNE_LFO:
        case fx.FX_TYPE_POSITION_LFO: return CONSTRAIN_SPEED_OTHER_ENVELOPE;
        case fx.FX_TYPE_VOLUME_LFO: return CONSTRAIN_SPEED_AMP_ENVELOPE;

        case fx.FX_TYPE_ARP: return 33;
        case fx.FX_TYPE_MIDI_CHORD: return MAX_CHORD;

        case fx.FX_TYPE_BIT_DEPTH: return BIT_DEPTH_MAX;
        case fx.FX_TYPE_TUNE: return 48;
        case fx.FX_TYPE_SLIDE_UP:
        case fx.FX_TYPE_SLIDE_DOWN:
        case fx.FX_TYPE_AMP_ENV:
        case fx.FX_TYPE_FILTER_ENV: return 255;
        default: return 100;
    }
}

AUDIO_CORE_RAM int16_t Sequencer::getFxMin(uint8_t fxID, bool forRandom)
{
    switch (fxID)
    {
        case fx.FX_TYPE_MICROMOVE: return 0;
        case fx.FX_TYPE_SAMPLE_SLICE: return 0;
        case fx.FX_TYPE_TEMPO:
        {
            return forRandom ? MIN_TEMPO_FX : TEMPO_FX_STP;
        }
        case fx.FX_TYPE_SWING: return 25;
        case fx.FX_TYPE_PANNING: return 0;
        case fx.FX_TYPE_BREAK_PATTERN: return 1;
        case fx.FX_TYPE_ROLL: return fx.ROLL_PERIOD_MIN;
        case fx.FX_TYPE_MIDI_CHORD: return MIN_CHORD;

        case fx.FX_TYPE_BIT_DEPTH: return BIT_DEPTH_MIN;
        default: return 0;
    }
}


// ---- instrument / track kinds ------------------------------------------------------

AUDIO_CORE_RAM bool isSampleTrack(int track)
{
    assert(track >= 0);
    return (track < kSampleTracks);
}

AUDIO_CORE_RAM bool isSampleInstrument(int instrument)
{
    assert(instrument >= 0);
    assert(instrument < kTotalInstrumentsCount);
    return instrument < kSampleInstrumentsCount;
}

AUDIO_CORE_RAM bool isMidiInstrument(int instrument)
{
    assert(instrument >= 0);
    assert(instrument < kTotalInstrumentsCount);
    return instrument >= kSampleInstrumentsCount &&
           instrument < kSampleInstrumentsCount + kMidiInstrumentsCount;
}

AUDIO_CORE_RAM bool isSynthInstrument(int instrument)
{
    assert(instrument >= 0);
    assert(instrument < kTotalInstrumentsCount);
    return instrument >= kSampleInstrumentsCount + kMidiInstrumentsCount &&
           instrument < kSampleInstrumentsCount + kMidiInstrumentsCount + kSynthInstrumentsCount;
}

AUDIO_CORE_RAM int instrumentIdxToMidiChannel(int instrument)
{
    assert(instrument >= 0);
    if (isSampleInstrument(instrument))
        return kNoChannel;
    if (isMidiInstrument(instrument))
        return instrument - kSampleInstrumentsCount + 1;
    if (isSynthInstrument(instrument))
        return instrument - kSampleInstrumentsCount - kMidiInstrumentsCount + 1;
    assert(false);
    return kNoChannel;
}

int instrumentIdxToSynthIdx(int instrumentIdx)
{
    assert(isSynthInstrument(instrumentIdx));
    return instrumentIdxToMidiChannel(instrumentIdx) - 1;
}
