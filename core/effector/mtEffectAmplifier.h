#ifndef EFFECTOR_MTEFFECTAMPLIFIER_H_
#define EFFECTOR_MTEFFECTAMPLIFIER_H_

#include "mtEffect.h"

class mtEffectAmplifier : public mtEffect
{
  public:
    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    uint32_t length;
    float amp;
    uint32_t processedSamples = 0;
    bool state = false;

    struct ChannelParams
    {
        int16_t* srcAddr;
        int16_t* dstAddr;
    } channelParams[kChannels];
};

extern mtEffectAmplifier sampleEditorAmplifier;

#endif /* EFFECTOR_MTEFFECTAMPLIFIER_H_ */
