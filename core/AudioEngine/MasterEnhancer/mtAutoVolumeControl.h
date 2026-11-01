#ifndef AUDIOENGINE_MASTERENHANCER_MTAUTOVOLUMECONTROL_H_
#define AUDIOENGINE_MASTERENHANCER_MTAUTOVOLUMECONTROL_H_

#include "stdint.h"

enum struct autoVolumeMaxExpanderGain
{
    gain0dB,
    gain6dB,
    gain12dB
};

enum struct autoVolumeCompressorResponseTime
{
    time0ms,
    time25ms,
    time50ms,
    time100ms
};

enum struct autoVolumeLimiterType
{
    compressor,
    hardLimiter
};

constexpr uint8_t AUTO_VOLUME_GAIN_EXPANDER_MAX = (uint8_t)autoVolumeMaxExpanderGain::gain12dB;
constexpr uint8_t AUTO_VOLUME_GAIN_EXPANDER_MIN = (uint8_t)autoVolumeMaxExpanderGain::gain0dB;

constexpr uint8_t AUTO_VOLUME_COMPRESSOR_RESPONSE_TIME_MAX =
  (uint8_t)autoVolumeCompressorResponseTime::time100ms;
constexpr uint8_t AUTO_VOLUME_COMPRESSOR_RESPONSE_TIME_MIN =
  (uint8_t)autoVolumeCompressorResponseTime::time0ms;

constexpr uint8_t AUTO_VOLUME_LIMITER_TYPE_MAX = (uint8_t)autoVolumeLimiterType::hardLimiter;
constexpr uint8_t AUTO_VOLUME_LIMITER_TYPE_MIN = (uint8_t)autoVolumeLimiterType::compressor;

constexpr float AUTO_VOLUME_TRESHOLD_MAX = 0.0f;
constexpr float AUTO_VOLUME_TRESHOLD_MIN = -96.0f;

constexpr float AUTO_VOLUME_ATTACK_MAX = 32.0f;
constexpr float AUTO_VOLUME_ATTACK_MIN = 2.0f;

constexpr float AUTO_VOLUME_DECAY_MAX = 32.0f;
constexpr float AUTO_VOLUME_DECAY_MIN = 2.0f;

class Sgtl5000;

class mtAutoVolumeControl
{
  public:
    void enable(autoVolumeMaxExpanderGain maxExpanderGain,
                autoVolumeCompressorResponseTime responseTime,
                autoVolumeLimiterType limiterType,
                float threshold,
                float attack,
                float decay);
    void change(autoVolumeMaxExpanderGain maxExpanderGain,
                autoVolumeCompressorResponseTime responseTime,
                autoVolumeLimiterType limiterType,
                float threshold,
                float attack,
                float decay);
    void disable();

    void setCodec(Sgtl5000* codec);

  private:
    void constrainParams(autoVolumeMaxExpanderGain* maxExpanderGain,
                         autoVolumeCompressorResponseTime* responseTime,
                         autoVolumeLimiterType* limiterType,
                         float* threshold,
                         float* attack,
                         float* decay);

    Sgtl5000* codec;
};

#endif /* AUDIOENGINE_MASTERENHANCER_MTAUTOVOLUMECONTROL_H_ */
