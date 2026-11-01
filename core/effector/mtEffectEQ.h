#ifndef EFFECTOR_MTEFFECTEQ_H_
#define EFFECTOR_MTEFFECTEQ_H_

#include "AudioStream.h"
#include "mtEffect.h"

/*
 * Equalizer effect.
 * Level values need to be provided in range of +-12 dB
 */
class mtEffectEQ : public mtEffect
{
  public:
    static constexpr float qOffsetMax = 3.0f;
    static constexpr float frequencyMax = 20000.0f;

    enum Params
    {
        LEVEL_LOW,
        FREQ_LOW,
        Q_LOW,
        LEVEL_MID,
        FREQ_MID,
        Q_MID,
        LEVEL_HIGH,
        FREQ_HIGH,
        Q_HIGH
    };

    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    float calculateBandwidth(float freq, float q);
    struct EqContext
    {
        float gain;
        float z1, z2, sr;
        float c, d;
    };

    float levelLow;
    float levelMid;
    float levelHigh;
    float freqLow = 10.0f;
    float freqMid = 200.0f;
    float freqHigh = 12000.0f;
    float qLow = 1.0f;
    float qMid = 1.0f;
    float qHigh = 1.0f;
    float bwLow;
    float bwMid;
    float bwHigh;

    struct ChannelParams
    {
        int16_t* srcAddr;
        int16_t* dstAddr;
        EqContext ctxLow;
        EqContext ctxMid;
        EqContext ctxHigh;
    };

    ChannelParams channelParams[kChannels];

    uint32_t length;

    uint32_t processedSamples = 0;
    bool state = false;

    float computeSample(EqContext* p, float in);
    void initContext(EqContext* ctx, float freq, float bandwidth, float gain);
};

extern mtEffectEQ sampleEditorEQ;

#endif /* EFFECTOR_MTEFFECTEQ_H_ */
