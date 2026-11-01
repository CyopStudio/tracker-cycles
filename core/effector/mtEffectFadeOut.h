#ifndef EFFECTOR_MTEFFECTFADEOUT_H_
#define EFFECTOR_MTEFFECTFADEOUT_H_

#include "mtEffect.h"

class mtEffectFadeOut : public mtEffect
{
  public:
    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    uint32_t length;

    struct ChannelParams
    {
        int16_t* srcAddr;
        int16_t* dstAddr;
        float decreaseVolumePerSampel;
        float volumeMultiplier;
    };

    ChannelParams channelParams[kChannels];

    uint32_t processedSamples = 0;
    bool state = false;
};

extern mtEffectFadeOut sampleEditorFadeOut;

#endif /* EFFECTOR_MTEFFECTREVERSE_H_ */
