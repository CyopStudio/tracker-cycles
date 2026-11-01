#ifndef EFFECTOR_MTEFFECTTIMESTRETCH_H_
#define EFFECTOR_MTEFFECTTIMESTRETCH_H_

#include "mtEffect.h"

// SOLA algorithm. Performs time scaling for sample data given in 'input',
// write result to 'output'.
class SOLA
{
  private:
    int16_t* output;
    const int16_t* input;
    uint32_t inSamples;
    uint32_t outSamples;
    const int16_t* seqOffset;
    uint32_t sequenceSize;
    uint32_t sequenceSkip;
    uint32_t flatSize;
    uint32_t overlapSize;
    uint32_t seekWindow;

    int seekBestOverlap(const int16_t* inputPrev, const int16_t* inputNew);
    void processOverlap(int16_t* output,
                        const int16_t* inputPrev,
                        const int16_t* inputNew,
                        uint32_t outSamples);

  public:
    void init(int16_t* output,
              const int16_t* input,
              uint32_t inSamples,
              uint32_t outSamples,
              uint32_t sequenceSize);

    // process a single sequence/granule, returns number of samples processed, 0 if finished
    uint32_t processSequence();
};

class mtEffectTimestretch : public mtEffect
{
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;

  protected:
    uint32_t targetSamples;
    uint32_t sequenceSize;

    virtual bool startProcess() override;

  private:
    struct ChannelParams
    {
        SOLA sola;
        int16_t* srcAddr;
        int16_t* dstAddr;
    };

    ChannelParams channelParams[kChannels];

    uint32_t length;

    uint32_t processedSamples = 0;
    bool finished = false;

    int seekBestOverlap(const int16_t* input_prev, const int16_t* input_new);
    void processOverlap(int16_t* output, const int16_t* input_prev, const int16_t* input_new);
    void initSOLA(int16_t* output, const int16_t* input, uint32_t inSamples, uint32_t outSamples);
    void processSOLA();
};

class mtEffectTimestretchBeat : public mtEffectTimestretch
{
  public:
    enum Params
    {
        TEMPO,
        STEPS,
        MICRO_STEP,
        GRANULE_DIV
    };

    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    uint16_t tempo;
    uint16_t steps;
    float microStep;
    float granuleDiv;
};

class mtEffectTimestretchNote : public mtEffectTimestretch
{
  public:
    enum Params
    {
        LENGTH,
        GRANULE_SIZE
    };

    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    float targetLength;
    uint16_t granuleSize;
};

extern mtEffectTimestretchBeat sampleEditorTimestretchBeat;
extern mtEffectTimestretchNote sampleEditorTimestretchNote;

#endif /* EFFECTOR_MTEFFECTTIMESTRETCH_H_ */
