#ifndef AUDIOENGINE_AUDIOOUTPUT_H_
#define AUDIOENGINE_AUDIOOUTPUT_H_

#include "Audio.h"

class AudioOutput
{
  public:
    enum OutputChannels
    {
        kLeft,
        kRight,
        kChannelsCount
    };

    enum FinalMixInputs
    {
        kStream = 0,
        kSdCardPreview = 1,
        kTestWaveform = 4,
        kMetronomeTick = 5,
        kNesEmulatorSound = 6,
        kFinalMixerSize
    };

    AudioOutput();
#ifndef PLATFORM_IMX
    AudioMux* getLimiterSidechainMux();
#else
    UsbOutputStream* getUsbStream();
    AudioMux* getLimiterSidechainMux(OutputChannels channel);
#endif
    AudioEffectLimiter* getLimiter(OutputChannels channel);
    AudioMixer<FinalMixInputs::kFinalMixerSize>* getFinalMixer(OutputChannels channel);
    AudioOutputStream* getStream();

  private:
#ifndef PLATFORM_IMX
    AudioMux limiterSidechainMux;
#else
    AudioMux limiterSidechainMux[OutputChannels::kChannelsCount];
    UsbOutputStream usbOutput;
#endif
    AudioEffectLimiter limiter[OutputChannels::kChannelsCount];
    AudioOutputStream outputStream;
    AudioMixer<FinalMixInputs::kFinalMixerSize> finalMixer[OutputChannels::kChannelsCount];
    std::vector<AudioConnection*> connections;
};

#endif /* AUDIOENGINE_AUDIOOUTPUT_H_ */
