#ifndef stereo_to_mono_h_
#define stereo_to_mono_h_

#include "AudioStream.h"

class AudioStereoToMono : public AudioStream
{
  public:
    AudioStereoToMono(void)
      : AudioStream(2, inputQueueArray)
    {
    }
    virtual void update(void);
    enum OutSignalType
    {
        stereo = 0,
        monoL,
        monoR,
        outSignalMax
    };

    void setOutSignalType(OutSignalType signalType);

  private:
    OutSignalType signalType;
    audio_block_t* inputQueueArray[2];
};

#endif
