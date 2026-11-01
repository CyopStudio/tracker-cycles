#include "EncoderImpl.h"

#ifdef TRACKER_MINI

void EncoderImpl::setAcceleration(int)
{
}

void EncoderImpl::setResolution(int)
{
}
#else
#include "TimeControl.h"
#include "fsl_enc.h"
#include "fsl_xbara.h"
#include "util_math.h"

const uint16_t
  EncoderImpl::accelerationLookupTable[kAccelerationLevelMax][kAccelerationLookupSize] = {
      /*1*/ {
        0,  1,  2,  3,  4,  5,  8,  12, 16, 18, 20, 22, 24, 26, 28, 30,
        32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62,
        64, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92,
      },
      /*2*/
      {
        0,  1,  2,  3,  4,  5,  8,  12, 17, 23, 2,  22, 24, 26, 28, 30,
        32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62,
        64, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92,
      },
      /*3*/ { 0,   1,   2,   3,   4,   6,   7,   8,   9,   10,  38,  47,  57,  68,  80,  93,
              107, 122, 138, 155, 173, 192, 212, 233, 255, 278, 302, 327, 353, 380, 408, 437,
              467, 498, 530, 563, 597, 632, 668, 705, 743, 782, 822, 863, 905, 948 },
      /*4*/ { 0,   1,   2,   3,   4,   8,   12,  17,  23,  30,  38,  47,  57,  68,  80,  93,
              107, 122, 138, 155, 173, 192, 212, 233, 255, 278, 302, 327, 353, 380, 408, 437,
              467, 498, 530, 563, 597, 632, 668, 705, 743, 782, 822, 863, 905, 948 },
      /*5*/ { 0, 1, 2, 3, 4 },
  };

void EncoderImpl::setAcceleration(int acceleration)
{
    EncoderImpl::acceleration = acceleration;
}

void EncoderImpl::setResolution(int resolution)
{
}

void EncoderImpl::init()
{
    XBARA_Init(XBARA1);

    enc_config_t encoderConfig;
    ENC_GetDefaultConfig(&encoderConfig);

    ENC_Init(ENC1, &encoderConfig);
    ENC_DoSoftwareLoadInitialPositionValue(ENC1);

    updateTime = timeControl.millis();

    setAcceleration(3);
}

void EncoderImpl::update()
{
    if ((timeControl.millis() - updateTime) > kUpdateTimeMs)
    {
        uint32_t newPositionPulses = ENC_GetPositionValue(ENC1);

        if (newPositionPulses != accumulatedPositionPulses)
        {

            int difPulses = (int)newPositionPulses - (int)accumulatedPositionPulses;
            int difClicks = difPulses / kPulsesOnClick;

            if (difClicks != 0)
            {
                lastClickTime = timeControl.millis();

                changeCallback(0, accelerate(difPulses) / kPulsesOnClick);
                accumulatedPositionPulses += difClicks * kPulsesOnClick;
            }
        }

        if ((timeControl.millis() - lastClickTime) > kResetPulsesTimeout)
        {
            accumulatedPositionPulses = newPositionPulses;
            lastClickTime = timeControl.millis();
        }

        updateTime = timeControl.millis();
    }
}

void EncoderImpl::setChangeCallback(void (*changeCallback)(uint8_t, int16_t))
{
    EncoderImpl::changeCallback = changeCallback;
}

int EncoderImpl::accelerate(int diff)
{
    if (acceleration == 0)
    {
        return diff;
    }

    int result = 0;
    int kDiffMax = kAccelerationLookupSize - 1;
    int kDiffMin = -kDiffMax;

    diff = clamp<int>(diff, kDiffMin, kDiffMax);

    if (diff > 0)
    {
        result = accelerationLookupTable[acceleration - 1][diff];
    }
    else if (diff < 0)
    {
        result = accelerationLookupTable[acceleration - 1][diff * (-1)] * (-1);
    }

    return result;
}
#endif
