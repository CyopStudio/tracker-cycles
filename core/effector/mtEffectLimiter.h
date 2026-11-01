#ifndef EFFECTOR_MTEFFECTLIMITER_H_
#define EFFECTOR_MTEFFECTLIMITER_H_

#include "mtEffect.h"

class mtEffectLimiter : public mtEffect
{
  public:
    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    uint16_t threshold;
    float attackCoeff;
    float releaseCoeff;
    float timeAverageCoeff = 0.002f;

    struct ChannelParams
    {
        float g = 1.0;
        uint16_t xpeak;
        uint16_t a;
        float coeff;
        float f;
        int16_t* srcAddr;
        int16_t* dstAddr;
    };

    ChannelParams channelParams[kChannels];

    uint32_t length;
    uint32_t processedSamples = 0;
    bool state = false;
};

extern mtEffectLimiter sampleEditorLimiter;

#endif /* EFFECTOR_MTEFFECTLIMITER_H_ */
