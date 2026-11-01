#include "Encoder.h"
#include "config.h"
#include "log.h"
#include "mk20dx128.h"

static constexpr uint8_t accelSettingCount = 3;

static const uint16_t potAcc[accelSettingCount][50] = {
    /*1*/
    {
      0,  1,  2,  3,  4,  5,  8,  12, 16, 18, 20, 22, 24, 26, 28, 30,
      32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62,
      64, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92,
    },

    /*2*/
    { 0,   1,   2,   6,   12,  20,  30,  45,  60,  80,  100, 100, 100, 100, 100, 100, 100,
      100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
      100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 },

    /*3*/
    { 0,   1,   2,   3,   4,   6,   7,   8,   9,   10,  38,  47,  57,  68,  80,  93,
      107, 122, 138, 155, 173, 192, 212, 233, 255, 278, 302, 327, 353, 380, 408, 437,
      467, 498, 530, 563, 597, 632, 668, 705, 743, 782, 822, 863, 905, 948 },
};

//--------------------------------------------------------------------------
void cEncoder::begin()
{
    PORTA_PCR10 = PORT_PCR_PE | PORT_PCR_PFE | PORT_PCR_MUX(6);
    PORTA_PCR11 = PORT_PCR_PE | PORT_PCR_PFE | PORT_PCR_MUX(6);

    FTM2_MODE = FTM_MODE_WPDIS | FTM_MODE_FTMEN;

    FTM2_C0SC = 0;
    FTM2_C1SC = 0;
    FTM2_CNT = 0;
    FTM2_MOD = 0;
    FTM2_SC = 0;

    FTM2_FILTER = FTM_FILTER_CH0FVAL(4) | FTM_FILTER_CH1FVAL(4);
    FTM2_CNTIN = 0;
    FTM2_MOD = MAX_16BIT;
    FTM2_CNT = 0;

    FTM2_C0SC = FTM_CSC_MSA;
    FTM2_COMBINE = 0;
    FTM2_C0V = MAX_16BIT_FOR_CALC * 3 / 8;
    FTM2_FMS = FTM_FMS_WPEN;
    FTM2_QDCTRL = FTM_QDCTRL_QUADEN | FTM_QDCTRL_PHBFLTREN | FTM_QDCTRL_PHAFLTREN;

    clear();
}

//--------------------------------------------------------------------------
int32_t cEncoder::read()
{
    int16_t resolution_step;
    uint8_t direction;
    int16_t diffrence = readCounter();

    // clear global_diff when dir change
    direction = 0;
    if (diffrence > 0)
    {
        direction = 1;
    }
    else if (diffrence < 0)
    {
        direction = 2;
    }
    else
    {
        return 0;
    }

    if (direction != encoder.lastDirection)
    {
        encoder.globalDiff = 0;
    }
    encoder.lastDirection = direction;

    // cacl step for current resolution
    resolution_step = ENCODER_PPR / encoder.resolution;
    encoder.globalDiff = encoder.globalDiff + diffrence;

    if (encoder.globalDiff >= resolution_step || encoder.globalDiff <= resolution_step * (-1))
    {
        int16_t result = encoder.globalDiff / resolution_step;
        encoder.globalDiff = encoder.globalDiff % resolution_step;

        if (encoder.speed > 0)
        {
            if (encoder.speed > accelSettingCount)
            {
                encoder.speed = accelSettingCount;
            }

            if (result > 0)
            {
                if (result > 49)
                {
                    result = 49;
                }
                result = potAcc[encoder.speed - 1][result];
            }
            else if (result < 0)
            {
                if (result < -49)
                {
                    result = -49;
                }
                result = potAcc[encoder.speed - 1][-result] * (-1);
            }
        }

        return result;
    }

    return 0;
}

//--------------------------------------------------------------------------
void cEncoder::clear()
{
    readReg = FTM2_FMS;

    FTM2_MODE = FTM_MODE_WPDIS | FTM_MODE_FTMEN;
    FTM2_QDCTRL = FTM_QDCTRL_PHAFLTREN | FTM_QDCTRL_PHBFLTREN;

    FTM2_CNT = 0;
    readReg = FTM2_SC;
    FTM2_SC = 0;
    readReg = FTM2_C0SC;

    FTM2_C0SC = FTM_CSC_MSA;

    if (FTM2_QDCTRL & FTM_QDCTRL_QUADIR)
    {
        FTM2_C0V = MAX_16BIT_FOR_CALC * 3 / 8;
    }
    else
    {
        FTM2_C0V = MAX_16BIT_FOR_CALC * 5 / 8;
    }

    FTM2_QDCTRL = FTM_QDCTRL_QUADEN | FTM_QDCTRL_PHBFLTREN | FTM_QDCTRL_PHAFLTREN;
    FTM2_FMS = FTM_FMS_WPEN;
    FTM2_C0SC = FTM_CSC_MSA | FTM_CSC_CHIE;
}

//--------------------------------------------------------------------------
int32_t cEncoder::readCounter()
{
    if (!(FTM2_SC & FTM_SC_TOIE))
    {
        uint16_t value = FTM2_CNT;
        FTM2_CNT = 0;

        if (value < MAX_16BIT_FOR_CALC / 4)
        {
            return value;
        }
        else if (value > MAX_16BIT_FOR_CALC * 3 / 4)
        {
            return value - MAX_16BIT_FOR_CALC;
        }
    }

    return 0;
}

//--------------------------------------------------------------------------
void cEncoder::setAcceleration(uint8_t value)
{
    encoder.speed = value;
}

//-----------------------------------------------------------------------------------------------
void cEncoder::setResolution(uint16_t value)
{
    encoder.resolution = value;
    encoder.globalDiff = 0;
}
