#ifndef EFFECTOR_MTEFFECTCOMPRESOR_H_
#define EFFECTOR_MTEFFECTCOMPRESOR_H_

#include "mtEffect.h"

class mtEffectCompresor : public mtEffect
{
  public:
    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    static constexpr int kProcessingBlockLength = 8192;

    struct strCompresorParams
    {
        int threshold;
        float ratio;
        int attack;
        int release;
    } interfaceParams;

    struct ChannelParams
    {
        float inputRms;
        float filteredLinearGain;
        float logarythmicInput;
        float logarythmicGain;
        float linearGain;
        float filterCoeff;
        int16_t* srcAddr;
        int16_t* dstAddr;
    } channelParams[kChannels];

    float compressorThreshold;
    float compressorSlope;
    float attackCoeff;
    float releaseCoeff;
    float timeAverageCoeff;

    uint32_t length;

    uint32_t processedSamples = 0;
    bool state = false;
};

extern mtEffectCompresor sampleEditorCompresor;

#endif /* EFFECTOR_MTEFFECTCOMPRESOR_H_ */
