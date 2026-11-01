#ifndef _input_i2s_h_
#define _input_i2s_h_

#include "interruptsConfig.h"

class AudioInputI2S
{
  public:
    //     Left and right channel samples are grouped in pairs. It's client responsibility to split
    //     it correctly to left and right buffers.
    void begin(void (*updateBuffersFunc)(void* data));
    static void isr(void);

  protected:
  private:
    static void (*updateBuffersFunc)(void* data);
};

#endif
