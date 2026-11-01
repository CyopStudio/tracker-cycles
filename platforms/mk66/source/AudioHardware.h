#ifndef SOURCE_AUDIOHARDWARE_H_
#define SOURCE_AUDIOHARDWARE_H_

//#include "control_sgtl5000.h"
#include "input_i2s.h"
#include "output_i2s.h"

class Sgtl5000;

class AudioHardware // todo: probably this class should be implementation of abstraction Sgtl5000
                    // with key switch support
{
  public:
    void init(Sgtl5000* codec,
              int audioOutputEnablePin,
              void (*updateAudioBuffersFunc)(void* left, void* right),
              void (*updateAudioInputFunc)(void* data));
    void update();
    void enableOutput(bool enabled);
    bool isOutputEnabled();

  private:
    AudioOutputI2S i2sOut;
    AudioInputI2S i2sInput;
    int audioOutputEnablePin;
    Sgtl5000* codec;
};

extern AudioHardware audioHardware;

#endif /* SOURCE_AUDIOHARDWARE_H_ */
