#ifndef LIBS_AUDIO_EFFECT_BITDEPTH_H_
#define LIBS_AUDIO_EFFECT_BITDEPTH_H_
#include "AudioStream.h"

class AudioBitDepth : public AudioStream
{
  public:
    AudioBitDepth(void)
      : AudioStream(1, &inputQueueArray)
    {
    }
    virtual void update(void);
    void setBitDepth(uint8_t val);

  private:
    uint8_t bitDepth = 16;
    uint32_t bitCrusherMask = 0;
    audio_block_t* inputQueueArray;
};

#endif /* LIBS_AUDIO_EFFECT_BITDEPTH_H_ */
