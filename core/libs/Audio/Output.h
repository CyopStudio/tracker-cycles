#ifndef OUTPUT_H_
#define OUTPUT_H_

#include "AudioStream.h"

class AudioOutputStream : public AudioStream
{
  public:
    AudioOutputStream()
      : AudioStream(4, inputQueueArray)
    {
        begin();
    }

    virtual void update(void);
    void begin(void);
    void selectSource(int channel, int idx);
    void setOutBuffers(void* left, void* right);

  private:
    static constexpr int kOutChannels = 2;
    static constexpr int kSources = 2;

    audio_block_t* inputQueueArray[kOutChannels * kSources];
    void* outBuffers[kOutChannels];
    int source[kOutChannels];
};

#endif /* OUTPUT_H_ */
