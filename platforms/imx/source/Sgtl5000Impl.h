#ifndef SOURCE_SGTL5000IMPL_H_
#define SOURCE_SGTL5000IMPL_H_
#include "Platform.h"
#include "control_sgtl5000.h"

class I2cDevice;

class Sgtl5000Impl : public Sgtl5000
{
  public:
    virtual void init() override;
    virtual void setInputType(InputType input) override;
    virtual void setHeadphoneSource(HeadphoneSource source) override;
    virtual void setLineInLevel(int level) override;
    virtual void setMicGain(int gainDb) override;
    virtual void setHeadphoneMuted(bool muted) override;
    virtual void setMasterVolume(float volume) override;
    virtual void setAutoVolumeEnabled(bool enabled) override;
    virtual void configureAutoVolume(AutoVolumeParams* params) override;
    virtual void setEnhanceBassEnabled(bool enabled) override;
    virtual void configureEnhanceBass(EnhanceBassParams* params) override;
    virtual void setEqualizerType(EqualizerType type) override;
    virtual void configureFiveBandEqualizer(float* values) override;
    virtual void enableSurroundSound(bool enabled) override;
    virtual void configureSurroundSound(SurroundSoundParams* params) override;
    virtual void enableHardwareProcessor(bool enabled) override;

    void powerOff();
    I2cDevice* getDriverI2c();

  private:
    enum FiveBandsEqFrequencies
    {
        k115Hz,
        k330Hz,
        k990Hz,
        k3000Hz,
        k9900Hz
    };

    AudioControlSGTL5000 codecDriver;
};

#endif /* SOURCE_SGTL5000IMPL_H_ */
