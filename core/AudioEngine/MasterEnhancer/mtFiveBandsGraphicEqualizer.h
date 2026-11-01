#ifndef AUDIOENGINE_MASTERENHANCER_MTFIVEBANDSGRAPHICEQUALIZER_H_
#define AUDIOENGINE_MASTERENHANCER_MTFIVEBANDSGRAPHICEQUALIZER_H_

#include "stdint.h"

/*
It is implemented by five parallel, second order biquad filters
with fixed frequencies of 115Hz, 330Hz, 990Hz, 3kHz, and 9.9kHz.
Each band has a range of adjustment from 1.00 (+12dB) to -1.00 (-11.75dB).
*/

constexpr float BAND_ADJUSTMENT_MAX = 1.0f;
constexpr float BAND_ADJUSTMENT_MIN = -1.0f;
constexpr float BAND_ADJUSTMENT_DEFAULT = 0.0f;

constexpr uint8_t BANDS_NUMBER = 5;

enum struct mtGraphicEqBands
{
    freq115Hz,
    freq330Hz,
    freq990Hz,
    freq3000Hz,
    freq9900Hz
};

class Sgtl5000;

class mtFiveBandsGraphicEqualizer
{
  public:
    void setCodec(Sgtl5000* codec);

    void enable(float* bandsAdjustments);
    void change(float* bandsAdjustments);
    void disable();

  private:
    void constrainParams(float* bandsAdjustments);

    Sgtl5000* codec;
};

#endif /* AUDIOENGINE_MASTERENHANCER_MTFIVEBANDSGRAPHICEQUALIZER_H_ */
