#ifndef monoToStereo_h_
#define monoToStereo_h_

#include "AudioStream.h"
#include "util_math.h"

class MonoToStereo : public AudioStream
{
  public:
    static constexpr float kUnityGain = 65536.0f;
    static constexpr float kMinGain = 0.0f;
    static constexpr float kMaxGain = 1.0f;

    MonoToStereo(void)
      : AudioStream(1, inputQueueArray)
    {
        for (int channel = 0; channel < Channel::channelCount; channel++)
        {
            multiplier[channel] = (int32_t)kUnityGain;
            previous_multiplier[channel] = (int32_t)kUnityGain;
        }
    }
    virtual void update(void);

    enum Channel
    {
        left,
        right,
        channelCount
    };
    void gain(float gain, Channel channel)
    {
        gain = clamp<float>(gain, kMinGain, kMaxGain);

        previous_multiplier[channel] = multiplier[channel];
        multiplier[channel] = gain * kUnityGain;
    }

  private:
    int32_t multiplier[channelCount];
    int32_t previous_multiplier[channelCount];
    audio_block_t* inputQueueArray[1];
};

#endif
