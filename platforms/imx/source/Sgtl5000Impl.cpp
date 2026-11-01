#include "Sgtl5000Impl.h"
#include "util_math.h"

void Sgtl5000Impl::init()
{
    codecDriver.init(AudioControlSGTL5000::kI2cAddressCsLow);
}

void Sgtl5000Impl::setInputType(InputType input)
{
    codecDriver.inputSelect((AudioControlSGTL5000::InputType)input);
}

void Sgtl5000Impl::setHeadphoneSource(HeadphoneSource source)
{
    assert(source == HeadphoneSource::kAudioInput || source == HeadphoneSource::kAudioOutput);
    codecDriver.dapSourceSelect(source == HeadphoneSource::kAudioInput
                                  ? AudioControlSGTL5000::DapSource::kDapSourceADC
                                  : AudioControlSGTL5000::DapSource::kDapSourceI2sIn);
}

void Sgtl5000Impl::setLineInLevel(int level)
{
    codecDriver.lineInLevel(level);
}

void Sgtl5000Impl::setMicGain(int gainDb)
{
    codecDriver.micGain(gainDb);
}

void Sgtl5000Impl::setHeadphoneMuted(bool muted)
{
    if (muted)
    {
        codecDriver.muteHeadphone();
    }
    else
    {
        codecDriver.unmuteHeadphone();
    }
}

void Sgtl5000Impl::setMasterVolume(float volume)
{

    if (volume == 0.0f)
    {
        if (!codecDriver.isHeadphoneMuted())
        {
            codecDriver.muteHeadphone();
        }
    }
    else
    {
        if (codecDriver.isHeadphoneMuted())
        {
            codecDriver.unmuteHeadphone();
        }

        uint8_t channelVolume =
          (uint8_t)mapClamped<float>(volume,
                                     0.01f,
                                     1.0f,
                                     AudioControlSGTL5000::kMinHeadphonesAnalogVolume,
                                     AudioControlSGTL5000::kMaxHeadphonesAnalogVolume);

        codecDriver.setHeadphonesVolume(channelVolume, channelVolume);
    }
}

void Sgtl5000Impl::setAutoVolumeEnabled(bool enabled)
{
    if (enabled)
    {
        codecDriver.autoVolumeEnable();
    }
    else
    {
        codecDriver.autoVolumeDisable();
    }
}

void Sgtl5000Impl::configureAutoVolume(AutoVolumeParams* params)
{
    codecDriver.autoVolumeControl(params->maxExpanderGainType,
                                  params->maxExpanderGainType,
                                  params->limiterType,
                                  params->threshold,
                                  params->attack,
                                  params->decay);
}

void Sgtl5000Impl::setEnhanceBassEnabled(bool enabled)
{
    if (enabled)
    {
        codecDriver.enhanceBassEnable();
    }
    else
    {
        codecDriver.enhanceBassDisable();
    }
}

void Sgtl5000Impl::configureEnhanceBass(EnhanceBassParams* params)
{
    codecDriver.enhanceBass(params->inputEnhanceLevel,
                            params->bassEnhanceLevel,
                            params->cutoffEnabled,
                            params->cutoffFrequency);
}

void Sgtl5000Impl::setEqualizerType(EqualizerType type)
{
    codecDriver.eqSelect(type);
}

void Sgtl5000Impl::configureFiveBandEqualizer(float* values)
{
    codecDriver.eqBands(values[FiveBandsEqFrequencies::k115Hz],
                        values[FiveBandsEqFrequencies::k330Hz],
                        values[FiveBandsEqFrequencies::k990Hz],
                        values[FiveBandsEqFrequencies::k3000Hz],
                        values[FiveBandsEqFrequencies::k9900Hz]);
}

void Sgtl5000Impl::powerOff()
{
    codecDriver.write(0x30, 0);
}

I2cDevice* Sgtl5000Impl::getDriverI2c()
{
    return &codecDriver;
}

void Sgtl5000Impl::enableSurroundSound(bool enabled)
{
    if (enabled)
    {
        codecDriver.surroundSoundEnable();
    }
    else
    {
        codecDriver.surroundSoundDisable();
    }
}

void Sgtl5000Impl::configureSurroundSound(SurroundSoundParams* params)
{
    codecDriver.surroundSound(params->width, params->selection);
}

void Sgtl5000Impl::enableHardwareProcessor(bool enabled)
{
    codecDriver.enableDAP(enabled);
}
