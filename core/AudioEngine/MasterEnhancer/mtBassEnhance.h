#ifndef AUDIOENGINE_MASTERENHANCER_MTBASSENHANCE_H_
#define AUDIOENGINE_MASTERENHANCER_MTBASSENHANCE_H_

#include "stdint.h"

enum struct mtBassEnhanceCutoffEnable
{
    enable,
    disable
};

enum struct mtBassEnhanceCutoffFrequency
{
    freq80Hz,
    freq100Hz,
    freq125Hz,
    freq150Hz,
    freq175Hz,
    freq200Hz,
    freq225Hz
};

constexpr float BASS_ENHANCE_INPUT_LEVEL_MAX = 1.0f;
constexpr float BASS_ENHANCE_INPUT_LEVEL_MIN = 0.0f;

constexpr float BASS_ENHANCE_BASS_LEVEL_MAX = 1.0f;
constexpr float BASS_ENHANCE_BASS_LEVEL_MIN = 0.0f;

constexpr uint8_t BASS_ENHANCE_CUTOFF_ENABLE_MAX = (uint8_t)mtBassEnhanceCutoffEnable::disable;
constexpr uint8_t BASS_ENHANCE_CUTOFF_ENABLE_MIN = (uint8_t)mtBassEnhanceCutoffEnable::enable;

constexpr uint8_t BASS_ENHANCE_CUTOFF_FREQUENCY_MAX =
  (uint8_t)mtBassEnhanceCutoffFrequency::freq225Hz;
constexpr uint8_t BASS_ENHANCE_CUTOFF_FREQUENCY_MIN =
  (uint8_t)mtBassEnhanceCutoffFrequency::freq80Hz;

class Sgtl5000;

class mtBassEnhance
{
  public:
    void setCodec(Sgtl5000* codec);

    void enable(float initInputEnhanceLevel,
                float initBassEnhanceLevel,
                mtBassEnhanceCutoffEnable initCutoffEnable,
                mtBassEnhanceCutoffFrequency initCutoffFreq);
    void change(float inputEnhanceLevel,
                float bassEnhanceLevel,
                mtBassEnhanceCutoffEnable cutoffEnable,
                mtBassEnhanceCutoffFrequency cutoffFreq);
    void disable();

  private:
    void constrainChangeParams(float* inputEnhanceLevel,
                               float* bassEnhanceLevel,
                               mtBassEnhanceCutoffEnable* cutoffEnable,
                               mtBassEnhanceCutoffFrequency* cutoffFreq);
    Sgtl5000* codec;
};

#endif /* AUDIOENGINE_MASTERENHANCER_MTBASSENHANCE_H_ */
