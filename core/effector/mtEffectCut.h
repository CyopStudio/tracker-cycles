#ifndef EFFECTOR_MTEFFECTCUT_H_
#define EFFECTOR_MTEFFECTCUT_H_
#include "mtEffect.h"

class mtEffectCut : public mtEffect
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
    };

    ChannelParams channelParams[kChannels];

    uint32_t processedSamples = 0;
    bool state = false;
};

extern mtEffectCut sampleEditorCut;

#endif /* EFFECTOR_MTEFFECTCUT_H_ */
