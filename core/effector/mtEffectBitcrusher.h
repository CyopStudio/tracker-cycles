#ifndef EFFECTOR_MTEFFECTBITCRUSHER_H_
#define EFFECTOR_MTEFFECTBITCRUSHER_H_

#include "mtEffect.h"

struct strEffectBitcrusherParams
{
    int rate;
    int bits;
};

class mtEffectBitcrusher : public mtEffect
{
  public:
    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    strEffectBitcrusherParams effectBitcrusherParams;

    uint8_t crushBits;
    uint16_t sampleStep;

    uint32_t length;
    struct ChannelParams
    {
        int16_t* srcAddr;
        int16_t* dstAddr;
    } channelParams[kChannels];

    uint32_t processedSamples = 0;
    bool state = false;
};

extern mtEffectBitcrusher sampleEditorBitcrusher;

#endif /* EFFECTOR_MTEFFECTBITCRUSHER_H_ */
