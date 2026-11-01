#include "effect_limiter.h"
#include "ampLogarythmicValues.h"
#include "mtStructs.h"
#include "util_math.h"
#include <cmath>

int16_t zeroblock[AUDIO_BLOCK_SAMPLES];

AUDIO_CORE_FLASH void AudioEffectLimiter::begin()
{
    setAttack(mtProject.values.limiterAttack);
    setRelease(mtProject.values.limiterRelease);
    setThreshold(mtProject.values.limiterThreshold);
    isSideChainEnable = (bool)mtProject.values.limiterSidechain;
    isAverageGain = false;

    memset(inputStorageBuffer, 0, 10);
}

AUDIO_CORE_FLASH void AudioEffectLimiter::setThreshold(uint16_t ts)
{
    uint8_t quantizedThresholdIndex = mapClamped<int>(ts, 0, MAX_SIGNED_16BIT, 0, 100);
    threshold = limiterThresholdValues[quantizedThresholdIndex];
}
AUDIO_CORE_FLASH void AudioEffectLimiter::setAttack(uint16_t a)
{
    if (mtProject.values.limiterMode == limiterModeSubtle)
        attackTime = 1 - expf(-2.2f * (1000.0f / (44100.0f * a)));
    else if (mtProject.values.limiterMode == limiterModeHard)
        attackTime = a / 1000.0f;
}
AUDIO_CORE_FLASH void AudioEffectLimiter::setRelease(float r)
{
    if (mtProject.values.limiterMode == limiterModeSubtle)
        releaseTime = 1 - expf(-2.2f * (1000.0f / (44100.0f * r)));
    else if (mtProject.values.limiterMode == limiterModeHard)
        releaseTime = r / 1000.0f;
}

AUDIO_CORE_FLASH void AudioEffectLimiter::setSidechainEnable(bool isEnable)
{
    isSideChainEnable = isEnable;
}

AUDIO_CORE_FLASH float AudioEffectLimiter::getAverageGainChange()
{
    isAverageGain = false;
    return averageGain;
}
AUDIO_CORE_FLASH bool AudioEffectLimiter::isAvailableGainToGet()
{
    return isAverageGain;
}

AUDIO_CORE_RAM void AudioEffectLimiter::update(void)
{
    if (isBlocked)
        return;

    if (isSideChainEnable)
        processSidechain();
    else
        processMainStream();
}

AUDIO_CORE_RAM void AudioEffectLimiter::processSidechain()
{
    audio_block_t* inputBlock;
    audio_block_t* sidechainBlock;
    int16_t* inputData;
    int16_t* outputData;
    int16_t inputTemp;
    float localf;

    inputBlock = receiveWritable(0);
    sidechainBlock = receiveReadOnly(1);
    if (!inputBlock)
    {
        if (sidechainBlock)
            release(sidechainBlock);
        return;
    }

    inputData = sidechainBlock ? sidechainBlock->data : zeroblock;
    outputData = inputBlock->data;

    for (uint8_t n = 0; n < AUDIO_BLOCK_SAMPLES; n++)
    {
        a = (*inputData < 0) ? -*inputData : *inputData;

        coeff = (a > xpeak) ? attackTime : releaseTime;

        xpeak = (1 - coeff) * xpeak + coeff * a;

        localf = threshold / (float)xpeak;
        f = (localf < 1.0f) ? localf : 1.0f;

        coeff = (f < g) ? attackTime : releaseTime;

        g = (1 - coeff) * g + coeff * f;

        inputTemp = *inputData;

        int16_t outputTemp = *outputData;

        *outputData = g * outputStorageBuffer[LIMITER_DELAY - 1] + (1 - g) * (*inputData);

        sumGain += g;

        for (uint8_t i = (LIMITER_DELAY - 1); i >= 1; i--)
        {
            inputStorageBuffer[i] = inputStorageBuffer[i - 1];
            outputStorageBuffer[i] = outputStorageBuffer[i - 1];
        }
        inputStorageBuffer[0] = inputTemp;
        outputStorageBuffer[0] = outputTemp;
        inputData++;
        outputData++;
    }

    averageGain = sumGain / AUDIO_BLOCK_SAMPLES;
    sumGain = 0.0f;
    isAverageGain = true;

    transmit(inputBlock, 0);
    release(inputBlock);
    if (sidechainBlock)
        release(sidechainBlock);
}
AUDIO_CORE_RAM void AudioEffectLimiter::processMainStream()
{
    audio_block_t* inputBlock;
    audio_block_t* sidechainBlock;
    int16_t* inputData;
    int16_t inputTemp;
    float localf;

    inputBlock = receiveWritable(0);
    sidechainBlock = receiveReadOnly(1);
    if (!inputBlock)
    {
        if (sidechainBlock)
            release(sidechainBlock);
        return;
    }

    if (sidechainBlock)
    {
        release(sidechainBlock);
        sidechainBlock = nullptr;
    }
    inputData = inputBlock->data;

    for (uint8_t n = 0; n < AUDIO_BLOCK_SAMPLES; n++)
    {
        a = (*inputData < 0) ? -*inputData : *inputData;

        coeff = (a > xpeak) ? attackTime : releaseTime;

        xpeak = (1 - coeff) * xpeak + coeff * a;

        localf = threshold / (float)xpeak;
        f = (localf < 1.0f) ? localf : 1.0f;

        coeff = (f < g) ? attackTime : releaseTime;

        g = (1 - coeff) * g + coeff * f;

        inputTemp = *inputData;

        *inputData = g * inputStorageBuffer[LIMITER_DELAY - 1];

        sumGain += g;

        for (uint8_t i = (LIMITER_DELAY - 1); i >= 1; i--)
        {
            inputStorageBuffer[i] = inputStorageBuffer[i - 1];
        }
        inputStorageBuffer[0] = inputTemp;
        inputData++;
    }

    averageGain = sumGain / AUDIO_BLOCK_SAMPLES;
    sumGain = 0.0f;
    isAverageGain = true;

    transmit(inputBlock, 0);
    release(inputBlock);
    if (sidechainBlock)
        release(sidechainBlock);
}

AUDIO_CORE_FLASH void AudioEffectLimiter::blockUpdate()
{
    isBlocked = true;
}
AUDIO_CORE_FLASH void AudioEffectLimiter::unblockUpdate()
{
    isBlocked = false;
}
