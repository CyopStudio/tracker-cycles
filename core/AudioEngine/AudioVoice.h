#ifndef AUDIOENGINE_AUDIOVOICE_H_
#define AUDIOENGINE_AUDIOVOICE_H_
#include "Audio.h"

class AudioVoice
{
  public:
    enum EnvelopeGeneratorType
    {
        kFilter,
        kWavetablePosition,
        kGranularLength = kWavetablePosition,
        kGranularPosition,
        kPanning,
        kFinetune,
        kEnvelopeGeneratorCount
    };

#ifdef PLATFORM_IMX
    enum VoiceChannels
    {
        kLeft,
        kRight,
        kChannelsCount
    };
#endif

    AudioVoice();
#ifndef PLATFORM_IMX
    AudioPlayMemory* getMemoryPlayer();
    AudioBitDepth* getBitDepth();
    AudioFilterStateVariable* getFilter();
    AudioAmplifier* getAmplifier();
    MonoToStereo* getMonoToStereo();
    AudioAnalyzeRMS* getRMS();
#else
    AudioPlayMemory* getMemoryPlayer(VoiceChannels channel);
    AudioBitDepth* getBitDepth(VoiceChannels channel);
    AudioFilterStateVariable* getFilter(VoiceChannels channel);
    AudioAmplifier* getAmplifier(VoiceChannels channel);
    AudioAmplifier* getPanningAmplifier(VoiceChannels channel);
    AudioAnalyzeRMS* getRMS(VoiceChannels channel);
#endif
    envelopeGenerator* getEnvelopeGenerator(EnvelopeGeneratorType type);

  private:
#ifndef PLATFORM_IMX
    AudioPlayMemory memoryPlayer;
    AudioBitDepth bitDepth;
    AudioFilterStateVariable filter;
    AudioAmplifier amplifier;
    MonoToStereo monoToStereo;
    AudioAnalyzeRMS rms;
#else
    AudioPlayMemory memoryPlayer[VoiceChannels::kChannelsCount];
    AudioBitDepth bitDepth[VoiceChannels::kChannelsCount];
    AudioFilterStateVariable filter[VoiceChannels::kChannelsCount];
    AudioAmplifier amplifier[VoiceChannels::kChannelsCount];
    AudioAmplifier panningAmplifier[VoiceChannels::kChannelsCount];
    AudioAnalyzeRMS rms[VoiceChannels::kChannelsCount];
#endif

    envelopeGenerator envelope[kEnvelopeGeneratorCount];
    std::vector<AudioConnection*> connections;
};

#endif /* AUDIOENGINE_AUDIOVOICE_H_ */
