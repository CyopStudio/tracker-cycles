#ifndef AUDIOENGINE_SYNTHVOICE_H_
#define AUDIOENGINE_SYNTHVOICE_H_
#ifdef FEATURE_SYNTH
#include "amplifier.h"
#include "monoToStereo.h"
#include "synth_PolySynth.h"
#include <vector>

class Synth;

class SynthVoice
{
  public:
    static constexpr int kSend1ChannelOffset = PolySynth::kSend1Offset;
    static constexpr int kSend2ChannelOffset = PolySynth::kSend2Offset;
    static constexpr int kPostFaderLeftChannelOffset = 0;
    static constexpr int kPostFaderRightChannelOffset = PolySynth::kRightChannelOffset;
    static constexpr int kMacroCCIdxStart = 102;

    enum Channels
    {
        kLeft,
        kRight,

        kChannelsCount
    };

    SynthVoice();
    void init(Synth* synth);
    PolySynth* getPolySynth();
    AudioAmplifier* getPanningAmplifier(int synthIdx, Channels channel);

  private:
    AudioAmplifier panningAmplifier[kSynthParts][Channels::kChannelsCount];
    PolySynth polySynth;
};

#endif
#endif /* AUDIOENGINE_SYNTHVOICE_H_ */
