#ifndef LIBS_AUDIO_INPUT_H_
#define LIBS_AUDIO_INPUT_H_

#include "AudioStream.h"

class AudioInputStream : public AudioStream
{
  public:
    AudioInputStream()
      : AudioStream(0, NULL)
    {
    }

    virtual void update();
    void setInBuffer(void* data);

  private:
    void* data = nullptr;
};

#endif /* LIBS_AUDIO_INPUT_H_ */
