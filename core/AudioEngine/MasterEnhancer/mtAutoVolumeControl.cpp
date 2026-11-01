#include "mtAutoVolumeControl.h"
#include "Platform.h"
#include "mtAudioEngine.h"

void mtAutoVolumeControl::setCodec(Sgtl5000* codec)
{
    mtAutoVolumeControl::codec = codec;
}

void mtAutoVolumeControl::enable(autoVolumeMaxExpanderGain maxExpanderGain,
                                 autoVolumeCompressorResponseTime responseTime,
                                 autoVolumeLimiterType limiterType,
                                 float threshold,
                                 float attack,
                                 float decay)
{
    codec->setAutoVolumeEnabled(true);
    change(maxExpanderGain, responseTime, limiterType, threshold, attack, decay);
}
void mtAutoVolumeControl::change(autoVolumeMaxExpanderGain maxExpanderGain,
                                 autoVolumeCompressorResponseTime responseTime,
                                 autoVolumeLimiterType limiterType,
                                 float threshold,
                                 float attack,
                                 float decay)
{
    constrainParams(&maxExpanderGain, &responseTime, &limiterType, &threshold, &attack, &decay);

    Sgtl5000::AutoVolumeParams params;
    params.maxExpanderGainType = (int)maxExpanderGain;
    params.responseTimeType = (int)responseTime;
    params.limiterType = (int)limiterType;
    params.threshold = threshold;
    params.attack = attack;
    params.decay = decay;

    codec->configureAutoVolume(&params);
}
void mtAutoVolumeControl::disable()
{
    codec->setAutoVolumeEnabled(false);
}

void mtAutoVolumeControl::constrainParams(autoVolumeMaxExpanderGain* maxExpanderGain,
                                          autoVolumeCompressorResponseTime* responseTime,
                                          autoVolumeLimiterType* limiterType,
                                          float* threshold,
                                          float* attack,
                                          float* decay)
{
    if ((uint8_t)(*(maxExpanderGain)) > AUTO_VOLUME_GAIN_EXPANDER_MAX)
        *(maxExpanderGain) = (autoVolumeMaxExpanderGain)AUTO_VOLUME_GAIN_EXPANDER_MAX;
    else if ((uint8_t)(*(maxExpanderGain)) < AUTO_VOLUME_GAIN_EXPANDER_MIN)
        *(maxExpanderGain) = (autoVolumeMaxExpanderGain)AUTO_VOLUME_GAIN_EXPANDER_MIN;

    if ((uint8_t)(*(responseTime)) > AUTO_VOLUME_COMPRESSOR_RESPONSE_TIME_MAX)
        *(responseTime) =
          (autoVolumeCompressorResponseTime)AUTO_VOLUME_COMPRESSOR_RESPONSE_TIME_MAX;
    else if ((uint8_t)(*(responseTime)) < AUTO_VOLUME_COMPRESSOR_RESPONSE_TIME_MIN)
        *(responseTime) =
          (autoVolumeCompressorResponseTime)AUTO_VOLUME_COMPRESSOR_RESPONSE_TIME_MIN;

    if ((uint8_t)(*(limiterType)) > AUTO_VOLUME_LIMITER_TYPE_MAX)
        *(limiterType) = (autoVolumeLimiterType)AUTO_VOLUME_LIMITER_TYPE_MAX;
    else if ((uint8_t)(*(limiterType)) < AUTO_VOLUME_LIMITER_TYPE_MIN)
        *(limiterType) = (autoVolumeLimiterType)AUTO_VOLUME_LIMITER_TYPE_MIN;

    if (*threshold > AUTO_VOLUME_TRESHOLD_MAX)
        *threshold = AUTO_VOLUME_TRESHOLD_MAX;
    else if (*threshold < AUTO_VOLUME_TRESHOLD_MIN)
        *threshold = AUTO_VOLUME_TRESHOLD_MIN;

    if (*attack > AUTO_VOLUME_ATTACK_MAX)
        *attack = AUTO_VOLUME_ATTACK_MAX;
    else if (*attack < AUTO_VOLUME_ATTACK_MIN)
        *attack = AUTO_VOLUME_ATTACK_MIN;

    if (*decay > AUTO_VOLUME_DECAY_MAX)
        *decay = AUTO_VOLUME_DECAY_MAX;
    else if (*decay < AUTO_VOLUME_DECAY_MIN)
        *decay = AUTO_VOLUME_DECAY_MIN;
}
