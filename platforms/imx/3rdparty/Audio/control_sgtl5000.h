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
 * furnished to do so, subject to the following conditions:
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

#ifndef control_sgtl5000_h_
#define control_sgtl5000_h_

#include "i2cDevice.h"
#include "stdint.h"
class AudioControlSGTL5000 : public I2cDevice
{
  public:
    AudioControlSGTL5000(void)
      : i2c_addr(kI2cAddressCsLow)
    {
    }

    static constexpr uint8_t kI2cAddressCsLow = 0x0A;
    static constexpr uint8_t kI2cAddressCsHigh = 0x2A;

    static constexpr uint8_t kMaxHeadphonesAnalogVolume = 0x00;
    static constexpr uint8_t kMinHeadphonesAnalogVolume = 0x7F;
    static constexpr uint8_t kNoGainHeadphonesAnalogVolume = 0x18;

    enum InputType
    {
        kLineIn,
        kMic
    };

    enum DacSource
    {
        kDacSourceADC,
        kDacSourceI2sIn,
        kDacSourceDAP,
        kDacSourceMax
    };

    enum DapSource
    {
        kDapSourceADC,
        kDapSourceI2sIn,
        kDapSourceMax
    };

    enum EqBand
    {
        k115Hz,
        k330Hz,
        k990Hz,
        k3000Hz,
        k9900Hz
    };

    bool init(uint8_t i2c_addr);
    virtual bool update() override { return false; };
    bool setHeadphonesVolume(uint8_t left, uint8_t right);
    bool muteHeadphone(void);
    bool unmuteHeadphone(void);
    bool isHeadphoneMuted();
    bool muteLineout(void);
    bool unmuteLineout(void);
    bool inputSelect(InputType type);
    bool dapSourceSelect(DapSource source);
    bool dacSourceSelect(DacSource source);
    bool enableDAP(bool enable);
    bool micGain(uint32_t dB);
    bool lineInLevel(uint8_t n);
    bool lineInLevel(uint8_t left, uint8_t right);
    uint16_t lineOutLevel(uint8_t n);
    uint16_t lineOutLevel(uint8_t left, uint8_t right);
    uint16_t dacVolume(float n);
    uint16_t dacVolume(float left, float right);
    bool dacVolumeRamp();
    bool dacVolumeRampLinear();
    bool dacVolumeRampDisable();
    uint16_t adcHighPassFilterEnable(void);
    uint16_t adcHighPassFilterFreeze(void);
    uint16_t adcHighPassFilterDisable(void);
    uint16_t eqFilterCount(uint8_t n);
    uint16_t eqSelect(uint8_t n);
    uint16_t eqBand(uint8_t bandNum, float n);
    void eqBands(float bass, float mid_bass, float midrange, float mid_treble, float treble);
    void eqBands(float bass, float treble);
    void eqFilter(uint8_t filterNum, int* filterParameters);
    uint16_t autoVolumeControl(uint8_t maxGain,
                               uint8_t lbiResponse,
                               uint8_t hardLimit,
                               float threshold,
                               float attack,
                               float decay);
    uint16_t autoVolumeEnable(void);
    uint16_t autoVolumeDisable(void);
    uint16_t enhanceBass(float lr_lev, float bass_lev);
    uint16_t enhanceBass(float lr_lev, float bass_lev, uint8_t hpf_bypass, uint8_t cutoff);
    uint16_t enhanceBassEnable(void);
    uint16_t enhanceBassDisable(void);
    uint16_t surroundSound(uint8_t width);
    uint16_t surroundSound(uint8_t width, uint8_t select);
    uint16_t surroundSoundEnable(void);
    uint16_t surroundSoundDisable(void);
    bool isSurroundSoundEnable();
    void killAutomation(void) { semi_automated = false; }
    bool write(uint32_t reg, uint32_t val);

  protected:
    uint16_t ana_ctrl;
    uint8_t i2c_addr;
    unsigned char calcVol(float n, uint8_t range);
    unsigned int read(uint32_t reg);

    unsigned int modify(uint32_t reg, uint32_t val, uint32_t iMask);
    uint16_t dap_audio_eq_band(uint8_t bandNum, float n);

  private:
    bool semi_automated;
    void automate(uint8_t dap, uint8_t eq);
    void automate(uint8_t dap, uint8_t eq, uint8_t filterCount);
    bool isSurroundSound;
    bool headphoneMuted;
};

#define FILTER_LOPASS 0
#define FILTER_HIPASS 1
#define FILTER_BANDPASS 2
#define FILTER_NOTCH 3
#define FILTER_PARAEQ 4
#define FILTER_LOSHELF 5
#define FILTER_HISHELF 6

// For frequency adjustment
#define FLAT_FREQUENCY 0
#define PARAMETRIC_EQUALIZER 1
#define TONE_CONTROLS 2
#define GRAPHIC_EQUALIZER 3

void calcBiquad(uint8_t filtertype,
                float fC,
                float dB_Gain,
                float Q,
                uint32_t quantization_unit,
                uint32_t fS,
                int* coef);

#endif
