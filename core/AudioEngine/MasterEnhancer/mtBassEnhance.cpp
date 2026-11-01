#include "mtBassEnhance.h"
#include "Platform.h"
#include "mtAudioEngine.h"

void mtBassEnhance::setCodec(Sgtl5000* codec)
{
    mtBassEnhance::codec = codec;
}

void mtBassEnhance::enable(float initInputEnhanceLevel,
                           float initBassEnhanceLevel,
                           mtBassEnhanceCutoffEnable initCutoffEnable,
                           mtBassEnhanceCutoffFrequency initCutoffFreq)
{
    codec->setEnhanceBassEnabled(true);
    change(initInputEnhanceLevel, initBassEnhanceLevel, initCutoffEnable, initCutoffFreq);
}
void mtBassEnhance::change(float inputEnhanceLevel,
                           float bassEnhanceLevel,
                           mtBassEnhanceCutoffEnable cutoffEnable,
                           mtBassEnhanceCutoffFrequency cutoffFreq)
{
    constrainChangeParams(&inputEnhanceLevel, &bassEnhanceLevel, &cutoffEnable, &cutoffFreq);

    Sgtl5000::EnhanceBassParams params;
    params.inputEnhanceLevel = inputEnhanceLevel;
    params.bassEnhanceLevel = bassEnhanceLevel;
    params.cutoffEnabled = (int)cutoffEnable;
    params.cutoffFrequency = (int)cutoffFreq;

    codec->configureEnhanceBass(&params);
}
void mtBassEnhance::disable()
{
    codec->setEnhanceBassEnabled(false);
}

void mtBassEnhance::constrainChangeParams(float* inputEnhanceLevel,
                                          float* bassEnhanceLevel,
                                          mtBassEnhanceCutoffEnable* cutoffEnable,
                                          mtBassEnhanceCutoffFrequency* cutoffFreq)
{
    if (*inputEnhanceLevel > BASS_ENHANCE_INPUT_LEVEL_MAX)
        *inputEnhanceLevel = BASS_ENHANCE_INPUT_LEVEL_MAX;
    else if (*inputEnhanceLevel < BASS_ENHANCE_INPUT_LEVEL_MIN)
        *inputEnhanceLevel = BASS_ENHANCE_INPUT_LEVEL_MIN;

    if (*bassEnhanceLevel > BASS_ENHANCE_BASS_LEVEL_MAX)
        *bassEnhanceLevel = BASS_ENHANCE_BASS_LEVEL_MAX;
    else if (*bassEnhanceLevel < BASS_ENHANCE_BASS_LEVEL_MIN)
        *bassEnhanceLevel = BASS_ENHANCE_BASS_LEVEL_MIN;

    if ((uint8_t)(*cutoffEnable) > BASS_ENHANCE_CUTOFF_ENABLE_MAX)
        *cutoffEnable = (mtBassEnhanceCutoffEnable)BASS_ENHANCE_CUTOFF_ENABLE_MAX;
    else if ((uint8_t)(*cutoffEnable) < BASS_ENHANCE_CUTOFF_ENABLE_MIN)
        *cutoffEnable = (mtBassEnhanceCutoffEnable)BASS_ENHANCE_CUTOFF_ENABLE_MIN;

    if ((uint8_t)(*cutoffFreq) > BASS_ENHANCE_CUTOFF_FREQUENCY_MAX)
        *cutoffFreq = (mtBassEnhanceCutoffFrequency)BASS_ENHANCE_CUTOFF_FREQUENCY_MAX;
    else if ((uint8_t)(*cutoffFreq) < BASS_ENHANCE_CUTOFF_FREQUENCY_MIN)
        *cutoffFreq = (mtBassEnhanceCutoffFrequency)BASS_ENHANCE_CUTOFF_FREQUENCY_MIN;
}
