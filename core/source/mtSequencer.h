// Tracker Cycles (plan 4a): what is left of the stock step sequencer. The
// pattern / song engine, recording, performance mode, MIDI in and the
// metronome are gone; this keeps the services the rest of the firmware uses:
// the FX ids and value ranges (step-FX machinery that tidalNoteOn reuses),
// live notes from the pads and screens (handleNoteOn / handleNoteOff /
// stopManualNotes), the tempo (delay / LFO sync) and a transport that the
// Tidal player (plan 4a stage 3) drives through setTransport().
#ifndef _MTSEQUENCER_H_
#define _MTSEQUENCER_H_

#include <stdint.h>

#include "Platform.h"
#include "mtChordDefs.h"
#include "scales.h"
#include "mtMidi.h"
#include "mtStructs.h"
#include "util_math.h"

bool isSampleInstrument(int instrument);
bool isMidiInstrument(int instrument);
bool isSynthInstrument(int instrument);
int instrumentIdxToMidiChannel(int instrument);
int instrumentIdxToSynthIdx(int instrumentIdx);
bool isSampleTrack(int track);

static constexpr int kNoChannel = -1;

class Sequencer
{
  public:
    Sequencer() {}

    static constexpr int TRACKS = 8;
    static constexpr int kStepFxSlots = 2;

    enum enStep
    {
        STEP_NOTE_EMPTY = -1,
        STEP_NOTE_OFF_FADE = -2,
        STEP_NOTE_OFF_CUT = -3,
        STEP_NOTE_OFF_DEFAULT = -4,
        STEP_NOTE_DEFAULT = 60,
        STEP_NOTE_MAX = 127,
        STEP_NOTE_MIN = 0,

        STEP_VELO_MIN = 0,
        STEP_VELO_MAX = 127,
        STEP_VELO_DEFAULT = -1,
    };

    enum enMisc
    {
        MINSTEP = 0,
        MAXSTEP = 127,
        DEFAULT_PATTERN_LENGTH = 32,
    };

    enum MuteState
    {
        kUnmuted,
        kMuted,
    };

    enum enMidiInVoiceMode
    {
        midiInVoiceMode_sequencer,
        midiInVoiceMode_ignore,
        midiInVoiceMode_SamplePlayback,
        midiInVoiceMode_SampleEditor,
        midiInVoiceMode_SampleRecorder,
        midiInVoiceMode_default = midiInVoiceMode_sequencer,
    };

    static const float MAX_TEMPO;
    static const float MIN_TEMPO;
    static const float TEMPO_FX_STP;
    static const float MIN_TEMPO_FX;
    static const float MAX_TEMPO_FX;

    static struct strFxConsts
    {
        enum enFxType
        {
            // powiązane z listą tekstów w
            //			 #include <interfaceDefs.h>
            FX_TYPE_NONE,
            FX_TYPE_OFF,
            FX_TYPE_MICROMOVE,
            FX_TYPE_ROLL,
            FX_TYPE_CHANCE,
            FX_TYPE_RANDOM_NOTE,
            FX_TYPE_RANDOM_INSTRUMENT,
            FX_TYPE_RANDOM_VOLUME,
            FX_TYPE_SEND_CC_A,
            FX_TYPE_SEND_CC_B,
            FX_TYPE_SEND_CC_C,
            FX_TYPE_SEND_CC_D,
            FX_TYPE_SEND_CC_E,
            FX_TYPE_BREAK_PATTERN, // dziala jak koniec patternu
            FX_TYPE_MIDI_CHORD,
            FX_TYPE_TEMPO,
            FX_TYPE_RANDOM_VALUE,

            FX_TYPE_SWING,
            FX_TYPE_VOLUME,
            FX_TYPE_GLIDE,
            FX_TYPE_GATE_LENGTH,
            FX_TYPE_ARP,
            FX_TYPE_POSITION,
            FX_TYPE_VOLUME_LFO,
            FX_TYPE_PANNING_LFO,
            FX_TYPE_SAMPLE_SLICE,
            FX_TYPE_REVERSE_PLAYBACK,
            FX_TYPE_FILTER_LOWPASS,
            FX_TYPE_FILTER_HIGHPASS,
            FX_TYPE_FILTER_BANDPASS,
            FX_TYPE_DELAY_SEND,
            FX_TYPE_PANNING,
            FX_TYPE_REVERB_SEND,
            FX_TYPE_FINETUNE_LFO,
            FX_TYPE_MICROTUNING,
            FX_TYPE_FILTER_LFO,
            FX_TYPE_POSITION_LFO,
            FX_TYPE_SEND_CC_F,
            FX_TYPE_OVERDRIVE,
            FX_TYPE_BIT_DEPTH,
            FX_TYPE_TUNE,
            FX_TYPE_SLIDE_UP,
            FX_TYPE_SLIDE_DOWN,
            FX_TYPE_R30,
            FX_TYPE_AMP_ENV = FX_TYPE_R30,
            FX_TYPE_R31,
            FX_TYPE_FILTER_ENV = FX_TYPE_R31,

            // FX_TYPE_Rx to pozostałość po starych efektach i zostają tu żeby nic się nie rozpadło
            FX_TYPE_R1,
            FX_TYPE_GRANULAR_LENGTH = FX_TYPE_R1,
            FX_TYPE_R2,
            FX_TYPE_R3,
            FX_TYPE_R7,
            FX_TYPE_R8,
            FX_TYPE_R9,

        };

        enum enRollPeriod
        {
            ROLL_PERIOD_NONE = 0,
            ROLL_PERIOD_MIN = 0,

            ROLL_PERIOD_1_1,
            ROLL_PERIOD_1_2,
            ROLL_PERIOD_1_3,
            ROLL_PERIOD_1_4,
            ROLL_PERIOD_1_6,
            ROLL_PERIOD_1_8,
            ROLL_PERIOD_1_12,
            ROLL_PERIOD_1_16,
            ROLL_PERIOD_MAX = ROLL_PERIOD_1_16
        };
        enum enStutterPeriod
        {
            STUTTER_PERIOD_NONE = 0,
            STUTTER_PERIOD_MIN = 0,
            STUTTER_PERIOD_16_1,
            STUTTER_PERIOD_12_1,
            STUTTER_PERIOD_8_1,
            STUTTER_PERIOD_6_1,
            STUTTER_PERIOD_4_1,
            STUTTER_PERIOD_3_1,
            STUTTER_PERIOD_2_1,
            STUTTER_PERIOD_1_1,
            STUTTER_PERIOD_1_2,
            STUTTER_PERIOD_1_3,
            STUTTER_PERIOD_1_4,
            STUTTER_PERIOD_1_6,
            STUTTER_PERIOD_1_8,
            STUTTER_PERIOD_1_12,
            STUTTER_PERIOD_1_16,
            STUTTER_PERIOD_MAX = STUTTER_PERIOD_1_16
        };

        enum enRollDir
        {
            rollType_const = 0,
            rollType_volDown,
            rollType_volUp,
            rollType_noteDown,
            rollType_noteUp,
            rollType_noteRandom,
            rollNoteDir_max = rollType_noteRandom,
        };

        enum enArpConsts
        {
            arpPeriod_none = -1,
            arpPeriod_8 = 0,
            arpPeriod_6,
            arpPeriod_4,
            arpPeriod_3,
            arpPeriod_2,
            arpPeriod_1,
            arpPeriod_1_2,
            arpPeriod_1_3,
            arpPeriod_1_4,
            arpPeriod_1_6,
            arpPeriod_1_8,

            arpPeriodsCount = 11,

            arpOption_up = 0,
            arpOption_down,
            arpOption_random,
        };

        enum enChanceConsts
        {
            CHANCE_PROB_MAX = 100,
            CHANCE_GROUP_WITH_LAST = 101,
            CHANCE_NOT_LAST = 102,
            CHANCE_PLAY_SKIP_START = 103,
            CHANCE_CYCLE_STEPS = 6,
            CHANCE_CYCLE_COUNT = CHANCE_CYCLE_STEPS * CHANCE_CYCLE_STEPS,
            CHANCE_PLAY_SKIP_END = CHANCE_PLAY_SKIP_START + CHANCE_CYCLE_COUNT - 1,
            CHANCE_SKIP_PLAY_START = CHANCE_PLAY_SKIP_END + 1,
            CHANCE_SKIP_PLAY_END = CHANCE_SKIP_PLAY_START + CHANCE_CYCLE_COUNT - 1,
            CHANCE_MAX = CHANCE_SKIP_PLAY_END,
        };

    } fx;

    enum enRollVolumeOption
    {
        rollVolumeOption_const = 0,
        rollVolumeOption_up,
        rollVolumeOption_down,
        rollVolumeOption_random,
    };



    enum enAltMidiChannel
    {
        GRID_OUTSIDE_PATTERN = 20,
        GRID_INSIDE_PATTERN = 21,
        GRID_INSIDE_PATTERN_INSTRUMENT = 22,
    };

    enum enSeqState
    {
        SEQ_STATE_STOP = 0,
        SEQ_STATE_PLAY_PATTERN,
        SEQ_STATE_PLAY_SONG,
        SEQ_STATE_PLAY_PERFORMANCE,
        SEQ_STATE_PLAY_SELECTION,
    };

    void init();
    void update() {}
    void setCrcCalculator(CrcCalculator*) {}
    void initAfterReadConfig() {}
    // audio interrupt, once per block (MIDI clock out: stage 3)
    void tick(float intervalSeconds);

    // ---- transport: the Tidal player sets it (stage 3) ------------------------
    // play(): what the Play button starts (stock: pattern / song); stop(): hush
    void setTransport(void (*play)(), void (*stop)(), bool (*playing)())
    {
        transportPlay_ = play;
        transportStop_ = stop;
        transportPlaying_ = playing;
    }
    void playPattern();
    void playSong() { playPattern(); }
    void stop();
    bool isPlay() { return transportPlaying_ && transportPlaying_(); }
    uint8_t isStop() { return !isPlay(); }
    bool isRec() { return false; }
    uint8_t isRenderMode() { return 0; }
    uint8_t isSongMode() { return 0; }
    enSeqState getSeqState() { return isPlay() ? SEQ_STATE_PLAY_PATTERN : SEQ_STATE_STOP; }
    uint8_t getActualPos() { return 0; }

    // ---- tempo (BPM; the Tidal player sets it from cps x 240) ----------------
    float getCurrentTempo() { return tempo_ > 0 ? tempo_ : mtProject.values.globalTempo; }
    void setTempo(float bpm) { tempo_ = bpm; }
    uint32_t getSeqTimer() { return timerTicks_; }
    float getStepPeriodMs();  // one 16th
    void setExternalClockComp(uint8_t) {}

    // ---- live notes (pads, MIDI preview on the screens) ------------------------
    void handleNoteOn(uint8_t channel, uint8_t note, int8_t velocity, int16_t source);
    void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, int16_t source);
    void stopManualNotes();
    int8_t getSelectionFirstNote() { return lastLiveNote_; }
    void setMidiInVoiceMode(enMidiInVoiceMode mode) { midiInVoiceMode_ = mode; }
    uint8_t getMidiInVoiceMode() { return midiInVoiceMode_; }

    // ---- FX value ranges (step-FX machinery in the player engine) -------------
    int16_t getFxMax(uint8_t fxID, bool forRandom = false);
    int16_t getFxMin(uint8_t fxID, bool forRandom = false);

    static int8_t velocityToVolume(int8_t velo);
    static int8_t volumeToVelocity(int8_t vol);
    static uint8_t getInstrumentVelo(uint8_t instrumentIdx);

  private:
    struct LiveNote
    {
        bool open;
        uint8_t note;
        uint8_t instrument;
        int16_t source;
    };
    LiveNote live_[TRACKS] = {};
    int8_t lastLiveNote_ = STEP_NOTE_DEFAULT;
    uint8_t midiInVoiceMode_ = midiInVoiceMode_default;
    float tempo_ = 0;
    uint32_t timerTicks_ = 0;
    float timerAccu_ = 0;
    void (*transportPlay_)() = nullptr;
    void (*transportStop_)() = nullptr;
    bool (*transportPlaying_)() = nullptr;
};

extern Sequencer sequencer;

#endif
