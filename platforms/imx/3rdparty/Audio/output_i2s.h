/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:y
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef output_i2s_h_
#define output_i2s_h_

#include "config.h"
#include "stdint.h"

#include "interruptsConfig.h"

enum destinationState
{
    stateSdCard,
    statei2sOutput
};

class AudioOutputI2S
{
  public:
    AudioOutputI2S(void) {}

    static void update(void);
    void begin(void (*updateBuffersFunc)(void* left, void* right));

    friend class AudioInputI2S;
    static void isr(void);

  protected:
    static void config_i2s(void);

    static void (*updateBuffersFunc)(void* left, void* right);

  private:
    static constexpr uint32_t kAudioDmaChannel = 0;
    static int16_t buffer_left[kAudioBlockSamples * 2];
    static int16_t buffer_right[kAudioBlockSamples * 2];
};

#endif
