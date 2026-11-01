#ifndef EFFECTOR_MTEFFECTOVERDRIVE_H_
#define EFFECTOR_MTEFFECTOVERDRIVE_H_

#include "mtEffect.h"

class mtEffectOverdrive : public mtEffect
{
  public:
    enum Params
    {
        GAIN,
        TYPE
    };
    enum Types
    {
        QUADRATIC,
        EXPONENTIAL,
        RECIPROCAL
    };

    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    float gain;
    int type;

    uint32_t length;

    struct ChannelParams
    {
        int16_t* srcAddr;
        int16_t* dstAddr;
    };

    ChannelParams channelParams[kChannels];
    uint32_t processedSamples = 0;
    bool state = false;
};

extern mtEffectOverdrive sampleEditorOverdrive;

#endif /* EFFECTOR_MTEFFECTOVERDRIVE_H_ */
