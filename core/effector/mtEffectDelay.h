#ifndef EFFECTOR_MTEFFECTDELAY_H_
#define EFFECTOR_MTEFFECTDELAY_H_
#include "mtEffect.h"

struct strEffectDelayParams
{
    float feedback;
    int time;
};

constexpr uint8_t MAX_DELAY_VOICE_NUMBER = 99;

class mtEffectDelay : public mtEffect
{
  public:
    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    void calcLastVoiceIdx();
    strEffectDelayParams effectDelayParams;

    struct ChannelParams
    {
        int16_t* currentDstAddr;
        int16_t* sourceVoicePointer[MAX_DELAY_VOICE_NUMBER];
        float feedbackVoiceMult[MAX_DELAY_VOICE_NUMBER];
        uint32_t lastIndexInVoice[MAX_DELAY_VOICE_NUMBER];
        int16_t* lastSrcAddr;

        uint8_t firstActiveVoiceIdx;
        uint8_t lastActiveVoiceIdx;
        uint32_t processedSamples;
    };

    static ChannelParams channelParams[kChannels];
    uint8_t lastVoiceIdx;

    bool state = false;
};

extern mtEffectDelay sampleEditorDelay;

#endif /* EFFECTOR_MTEFFECTDELAY_H_ */
