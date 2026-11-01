#ifndef EFFECTOR_MTEFFECTNORMALIZER_H_
#define EFFECTOR_MTEFFECTNORMALIZER_H_

#include "mtEffect.h"

class mtEffectNormalizer : public mtEffect
{
  public:
    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    int find_max(int16_t* ptr, int len);

    uint32_t length;

    struct ChannelParams
    {
        int16_t* srcAddr;
        int16_t* dstAddr;
        float amp;
    };

    ChannelParams channelParams[kChannels];

    uint32_t processedSamples = 0;

    int norm_level;
    bool state = false;
};

extern mtEffectNormalizer sampleEditorNormalizer;

#endif /* EFFECTOR_MTEFFECTNORMALIZER_H_ */
