#include "mtFiveBandsGraphicEqualizer.h"
#include "Platform.h"
#include "mtAudioEngine.h"

void mtFiveBandsGraphicEqualizer::setCodec(Sgtl5000* codec)
{
    mtFiveBandsGraphicEqualizer::codec = codec;
}

void mtFiveBandsGraphicEqualizer::enable(float* bandAdjustments)
{
    codec->setEqualizerType(Sgtl5000::EqualizerType::kGraphicEqualizer);
    change(bandAdjustments);
}

void mtFiveBandsGraphicEqualizer::change(float* bandAdjustments)
{
    constrainParams(bandAdjustments);
    codec->configureFiveBandEqualizer(bandAdjustments);
}

void mtFiveBandsGraphicEqualizer::disable()
{
    codec->setEqualizerType(Sgtl5000::EqualizerType::kFlatFrequency);
}

void mtFiveBandsGraphicEqualizer::constrainParams(float* bandAdjustments)
{
    for (uint8_t i = 0; i < BANDS_NUMBER; i++)
    {
        if (bandAdjustments[i] > BAND_ADJUSTMENT_MAX)
            bandAdjustments[i] = BAND_ADJUSTMENT_MAX;
        else if (bandAdjustments[i] < BAND_ADJUSTMENT_MIN)
            bandAdjustments[i] = BAND_ADJUSTMENT_MIN;
    }
}
