#include "mtMasterEnhancer.h"
#include "util_math.h"

mtMasterEnhancer masterEnhancer;

constexpr float INPUT_ENHANCE_LEVEL_5DB = 5.0f / 6.0f;

void mtMasterEnhancer::setCodec(Sgtl5000* codec)
{
    surroundSound.setCodec(codec);
    bassEnhance.setCodec(codec);
    fiveBandsEq.setCodec(codec);
}

void mtMasterEnhancer::enable()
{
    surroundSound.disable();
    bassEnhance.enable(INPUT_ENHANCE_LEVEL_5DB,
                       0.0f,
                       mtBassEnhanceCutoffEnable::disable,
                       mtBassEnhanceCutoffFrequency::freq80Hz);
}

void mtMasterEnhancer::changeBassBoost(uint8_t bassBoostValue)
{
    float bassEnhanceValue = mapClamped<float>((float)bassBoostValue, 0.0f, 100.0f, 0.0f, 0.8f);

    bassEnhance.change(INPUT_ENHANCE_LEVEL_5DB,
                       bassEnhanceValue,
                       mtBassEnhanceCutoffEnable::disable,
                       mtBassEnhanceCutoffFrequency::freq80Hz);
}

void mtMasterEnhancer::changeStereoBoost(uint8_t bassBoostValue)
{
    if (!bassBoostValue)
    {
        surroundSound.disable();
    }
    else
    {
        uint8_t surroundSoundWidthValue = 7 - (bassBoostValue - 1);

        if (surroundSound.isEffectDisable())
            surroundSound.enable(surroundSoundWidthValue, mtSorroundSoundSelection::stereo);
        else
            surroundSound.change(surroundSoundWidthValue, mtSorroundSoundSelection::stereo);
    }
}
