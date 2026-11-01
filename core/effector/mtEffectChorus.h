#ifndef EFFECTOR_MTEFFECTCHORUS_H_
#define EFFECTOR_MTEFFECTCHORUS_H_

#include "mtEffect.h"

constexpr int CHORUS_BUFFER_LENGTH = 4096;

class mtEffectChorus : public mtEffect
{
  public:
    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    void calculate(int16_t* src, int16_t* dst, int channel); // process 128 samples

    uint32_t length;

    struct ChannelParams
    {
        int16_t* srcAddr;
        int16_t* dstAddr;
        int16_t* l_delayline;
        int16_t l_circ_idx;
    };

    ChannelParams channelParams[kChannels];
    uint32_t processedSamples = 0;

    int num_chorus;
    int delay_length;

    bool state = false;
};

extern mtEffectChorus sampleEditorChorus;

#endif /* EFFECTOR_MTEFFECTCHORUS_H_ */
