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
#include "control_sgtl5000.h"
#include "TimeControl.h"
#include "fsl_debug_console.h"
#include "i2cBusController.h"
#include "math.h"

//#define HARDWARE_TEST

#define CHIP_ID 0x0000

#define CHIP_DIG_POWER 0x0002
#define CHIP_CLK_CTRL 0x0004
#define CHIP_I2S_CTRL 0x0006
#define CHIP_SSS_CTRL 0x000A
#define CHIP_ADCDAC_CTRL 0x000E
#define CHIP_DAC_VOL 0x0010
#define CHIP_PAD_STRENGTH 0x0014
#define CHIP_ANA_ADC_CTRL 0x0020
#define CHIP_ANA_HP_CTRL 0x0022
#define CHIP_ANA_CTRL 0x0024
#define CHIP_LINREG_CTRL 0x0026
#define CHIP_REF_CTRL 0x0028
#define CHIP_MIC_CTRL 0x002A
#define CHIP_LINE_OUT_CTRL 0x002C
#define CHIP_LINE_OUT_VOL 0x002E
#define CHIP_ANA_POWER 0x0030
#define CHIP_PLL_CTRL 0x0032
#define CHIP_CLK_TOP_CTRL 0x0034
#define CHIP_ANA_STATUS 0x0036
#define CHIP_ANA_TEST1 0x0038
#define CHIP_ANA_TEST2 0x003A
#define CHIP_SHORT_CTRL 0x003C

#define DAP_CONTROL 0x0100
#define DAP_PEQ 0x0102
#define DAP_BASS_ENHANCE 0x0104
#define DAP_BASS_ENHANCE_CTRL 0x0106
#define DAP_AUDIO_EQ 0x0108
#define DAP_SGTL_SURROUND 0x010A
#define DAP_FILTER_COEF_ACCESS 0x010C
#define DAP_COEF_WR_B0_MSB 0x010E
#define DAP_COEF_WR_B0_LSB 0x0110
#define DAP_AUDIO_EQ_BASS_BAND0 0x0116   // 115 Hz
#define DAP_AUDIO_EQ_BAND1 0x0118        // 330 Hz
#define DAP_AUDIO_EQ_BAND2 0x011A        // 990 Hz
#define DAP_AUDIO_EQ_BAND3 0x011C        // 3000 Hz
#define DAP_AUDIO_EQ_TREBLE_BAND4 0x011E // 9900 Hz
#define DAP_MAIN_CHAN 0x0120
#define DAP_MIX_CHAN 0x0122
#define DAP_AVC_CTRL 0x0124
#define DAP_AVC_THRESHOLD 0x0126
#define DAP_AVC_ATTACK 0x0128
#define DAP_AVC_DECAY 0x012A
#define DAP_COEF_WR_B1_MSB 0x012C
#define DAP_COEF_WR_B1_LSB 0x012E
#define DAP_COEF_WR_B2_MSB 0x0130
#define DAP_COEF_WR_B2_LSB 0x0132
#define DAP_COEF_WR_A1_MSB 0x0134
#define DAP_COEF_WR_A1_LSB 0x0136
#define DAP_COEF_WR_A2_MSB 0x0138
#define DAP_COEF_WR_A2_LSB 0x013A

bool AudioControlSGTL5000::init(uint8_t i2c_addr)
{
    this->i2c_addr = i2c_addr;

    timeControl.delay(5);

    write(CHIP_ANA_POWER, 0x4060); // VDDD is externally driven with 1.8V

    write(CHIP_LINREG_CTRL, 0x006C); // VDDA & VDDIO both over 3.1V

    write(CHIP_REF_CTRL, 0x01F3);      // VAG=1.575, small pop ramp, +12.5% bias current
    write(CHIP_LINE_OUT_CTRL, 0x0F22); // LO_VAGCNTRL=1.65V, OUT_CURRENT=0.54mA
    write(CHIP_SHORT_CTRL, 0x4446);    // allow up to 125mA
    write(CHIP_ANA_CTRL, 0x0137);      // enable zero cross detectors
    write(CHIP_ANA_POWER, 0x40FF);     // power up: lineout, hp, adc, dac
    write(CHIP_DIG_POWER, 0x0073);     // power up all digital stuff
    timeControl.delay(400);
    write(CHIP_LINE_OUT_VOL, 0x1D1D); // default approx 1.3 volts peak-to-peak
    write(CHIP_CLK_CTRL, 0x0004);     // 44.1 kHz, 256*Fs
    write(CHIP_I2S_CTRL, 0x0130);     // SCLK=32*Fs, 16bit, I2S format
    write(CHIP_ADCDAC_CTRL, 0x0000);  // disable dac mute
    write(CHIP_DAC_VOL, 0x3C3C);      // digital gain, 0dB
    write(CHIP_ANA_HP_CTRL, 0x7F7F);  // set volume (lowest level)
    write(CHIP_ANA_CTRL, 0x0036);     // enable zero cross detectors

    setHeadphonesVolume(kNoGainHeadphonesAnalogVolume, kNoGainHeadphonesAnalogVolume);
    unmuteHeadphone();

    // uC-->i2s-->codec dap--> codec dac -> headphones
    dapSourceSelect(DapSource::kDapSourceI2sIn);
    dacSourceSelect(DacSource::kDacSourceDAP);
    enableDAP(true);
    enhanceBassEnable();
    eqSelect(GRAPHIC_EQUALIZER);

    semi_automated = true;
    return true;
}

unsigned int AudioControlSGTL5000::read(uint32_t reg)
{
    uint16_t val;

    if (i2cBusController->receiveData(i2c_addr, reg, 2, &val, 2) ==
        I2cBusController::OperationResult::kError)
    {
#ifdef HARDWARE_TEST
        PRINTF("Codec audio read error\n");
#endif
        return 0;
    }

#ifdef HARDWARE_TEST
    PRINTF("Codec audio read succesfull\n");
#endif
    return __REV16(val);
}

bool AudioControlSGTL5000::write(uint32_t reg, uint32_t val)
{
    uint16_t value = __REV16((uint16_t)val);

    if (reg == CHIP_ANA_CTRL)
    {
        ana_ctrl = val;
    }

    if (i2cBusController->sendData(i2c_addr, reg, 2, &value, 2) ==
        I2cBusController::OperationResult::kOK)
    {
#ifdef HARDWARE_TEST
        PRINTF("Codec audio write succesfull\n");
#endif
        return true;
    }
#ifdef HARDWARE_TEST
    PRINTF("Codec audio write error\n");
#endif
    return false;
}

unsigned int AudioControlSGTL5000::modify(uint32_t reg, uint32_t val, uint32_t iMask)
{
    unsigned int val1 = (read(reg) & (~iMask)) | val;
    if (!write(reg, val1))
        return 0;
    return val1;
}

bool AudioControlSGTL5000::setHeadphonesVolume(uint8_t left, uint8_t right)
{
    uint16_t hpVolLeft = left;
    uint16_t hpVolRight = right << 8;

    return write(CHIP_ANA_HP_CTRL, hpVolLeft | hpVolRight);
}

bool AudioControlSGTL5000::dapSourceSelect(DapSource source)
{
    constexpr uint32_t kDapSelectClearMask = 0x00C0;
    constexpr uint32_t kDapSelectMask[DapSource::kDapSourceMax] = { 0x0000, 0x0040 };
    assert(source < DapSource::kDapSourceMax);

    uint32_t reg = read(CHIP_SSS_CTRL);

    reg &= ~kDapSelectClearMask;
    reg |= kDapSelectMask[source];

    return write(CHIP_SSS_CTRL, reg);
}

bool AudioControlSGTL5000::dacSourceSelect(DacSource source)
{
    constexpr uint32_t kDacSelectClearMask = 0x0030;
    constexpr uint32_t kDacSelectMask[DacSource::kDacSourceMax] = { 0x0000, 0x0010, 0x0030 };
    assert(source < DacSource::kDacSourceMax);

    uint32_t reg = read(CHIP_SSS_CTRL);

    reg &= ~kDacSelectClearMask;
    reg |= kDacSelectMask[source];

    return write(CHIP_SSS_CTRL, reg);
}

bool AudioControlSGTL5000::enableDAP(bool enable)
{

    constexpr uint32_t kDapEnableClearMask = 0x0001;
    uint32_t kDapEnableMask = (uint32_t)enable;

    uint32_t reg = read(DAP_CONTROL);

    reg &= ~kDapEnableClearMask;
    reg |= kDapEnableMask;

    return write(DAP_CONTROL, reg);
}

bool AudioControlSGTL5000::muteHeadphone(void)
{
    headphoneMuted = true;
    return write(CHIP_ANA_CTRL, ana_ctrl | (1 << 4));
}

bool AudioControlSGTL5000::unmuteHeadphone(void)
{
    headphoneMuted = false;
    return write(CHIP_ANA_CTRL, ana_ctrl & ~(1 << 4));
}

bool AudioControlSGTL5000::isHeadphoneMuted()
{
    return headphoneMuted;
}

bool AudioControlSGTL5000::muteLineout(void)
{
    return write(CHIP_ANA_CTRL, ana_ctrl | (1 << 8));
}

bool AudioControlSGTL5000::unmuteLineout(void)
{
    return write(CHIP_ANA_CTRL, ana_ctrl & ~(1 << 8));
}

bool AudioControlSGTL5000::inputSelect(InputType type)
{
    if (type == InputType::kLineIn)
    {
        return write(CHIP_ANA_CTRL, ana_ctrl | (1 << 2)); // enable linein
    }
    else if (type == InputType::kMic)
    {
        return write(CHIP_ANA_CTRL, ana_ctrl & ~(1 << 2)); // enable mic
    }
    else
    {
        return false;
    }
}

bool AudioControlSGTL5000::lineInLevel(uint8_t n)
{
    return lineInLevel(n, n);
}

bool AudioControlSGTL5000::micGain(uint32_t dB)
{
    unsigned int preamp_gain, input_gain;

    if (dB >= 40)
    {
        preamp_gain = 3;
        dB -= 40;
    }
    else if (dB >= 30)
    {
        preamp_gain = 2;
        dB -= 30;
    }
    else if (dB >= 20)
    {
        preamp_gain = 1;
        dB -= 20;
    }
    else
    {
        preamp_gain = 0;
    }
    input_gain = (dB * 2) / 3;
    if (input_gain > 15)
        input_gain = 15;

    return write(CHIP_MIC_CTRL, 0x0170 | preamp_gain) &&
           write(CHIP_ANA_ADC_CTRL, (input_gain << 4) | input_gain);
}

bool AudioControlSGTL5000::lineInLevel(uint8_t left, uint8_t right)
{
    if (left > 15)
        left = 15;
    if (right > 15)
        right = 15;
    return write(CHIP_ANA_ADC_CTRL, (left << 4) | right);
}

unsigned short AudioControlSGTL5000::lineOutLevel(uint8_t n)
{
    if (n > 31)
        n = 31;
    else if (n < 13)
        n = 13;
    return modify(CHIP_LINE_OUT_VOL, (n << 8) | n, (31 << 8) | 31);
}

unsigned short AudioControlSGTL5000::lineOutLevel(uint8_t left, uint8_t right)
{
    if (left > 31)
        left = 31;
    else if (left < 13)
        left = 13;
    if (right > 31)
        right = 31;
    else if (right < 13)
        right = 13;
    return modify(CHIP_LINE_OUT_VOL, (right << 8) | left, (31 << 8) | 31);
}

unsigned short AudioControlSGTL5000::dacVolume(float n) // set both directly
{
    if ((read(CHIP_ADCDAC_CTRL) & (3 << 2)) != ((n > 0 ? 0 : 3) << 2))
    {
        modify(CHIP_ADCDAC_CTRL, (n > 0 ? 0 : 3) << 2, 3 << 2);
    }
    unsigned char m = calcVol(n, 0xC0);
    return modify(CHIP_DAC_VOL, ((0xFC - m) << 8) | (0xFC - m), 65535);
}
unsigned short AudioControlSGTL5000::dacVolume(float left, float right)
{
    unsigned short adcdac = ((right > 0 ? 0 : 2) | (left > 0 ? 0 : 1)) << 2;
    if ((read(CHIP_ADCDAC_CTRL) & (3 << 2)) != adcdac)
    {
        modify(CHIP_ADCDAC_CTRL, adcdac, 1 << 2);
    }
    unsigned short m = (0xFC - calcVol(right, 0xC0)) << 8 | (0xFC - calcVol(left, 0xC0));
    return modify(CHIP_DAC_VOL, m, 65535);
}

bool AudioControlSGTL5000::dacVolumeRamp()
{
    return modify(CHIP_ADCDAC_CTRL, 0x300, 0x300);
}

bool AudioControlSGTL5000::dacVolumeRampLinear()
{
    return modify(CHIP_ADCDAC_CTRL, 0x200, 0x300);
}

bool AudioControlSGTL5000::dacVolumeRampDisable()
{
    return modify(CHIP_ADCDAC_CTRL, 0, 0x300);
}

unsigned short AudioControlSGTL5000::adcHighPassFilterEnable(void)
{
    return modify(CHIP_ADCDAC_CTRL, 0, 3);
}

unsigned short AudioControlSGTL5000::adcHighPassFilterFreeze(void)
{
    return modify(CHIP_ADCDAC_CTRL, 2, 3);
}

unsigned short AudioControlSGTL5000::adcHighPassFilterDisable(void)
{
    return modify(CHIP_ADCDAC_CTRL, 1, 3);
}

// DAP_PEQ
unsigned short AudioControlSGTL5000::eqFilterCount(
  uint8_t n) // valid to n&7, 0 thru 7 filters enabled.
{
    return modify(DAP_PEQ, (n & 7), 7);
}

// DAP_AUDIO_EQ
unsigned short AudioControlSGTL5000::eqSelect(
  uint8_t n) // 0=NONE, 1=PEQ (7 IIR Biquad filters), 2=TONE (tone), 3=GEQ (5 band EQ)
{
    return modify(DAP_AUDIO_EQ, n & 3, 3);
}

unsigned short AudioControlSGTL5000::eqBand(uint8_t bandNum, float n)
{
    if (semi_automated)
    {
        automate(1, 3);
    }
    return dap_audio_eq_band(bandNum, n);
}
void AudioControlSGTL5000::eqBands(float bass,
                                   float mid_bass,
                                   float midrange,
                                   float mid_treble,
                                   float treble)
{
    if (semi_automated)
        automate(1, 3);
    dap_audio_eq_band(EqBand::k115Hz, bass);
    dap_audio_eq_band(EqBand::k330Hz, mid_bass);
    dap_audio_eq_band(EqBand::k990Hz, midrange);
    dap_audio_eq_band(EqBand::k3000Hz, mid_treble);
    dap_audio_eq_band(EqBand::k9900Hz, treble);
}
void AudioControlSGTL5000::eqBands(float bass, float treble) // dap_audio_eq(2);
{
    if (semi_automated)
        automate(1, 2);
    dap_audio_eq_band(0, bass);
    dap_audio_eq_band(4, treble);
}

// SGTL5000 PEQ Coefficient loader
void AudioControlSGTL5000::eqFilter(uint8_t filterNum, int* filterParameters)
{
    // TODO: add the part that selects 7 PEQ filters.
    if (semi_automated)
        automate(1, 1, filterNum + 1);
    modify(DAP_FILTER_COEF_ACCESS, (uint16_t)filterNum, 15);
    write(DAP_COEF_WR_B0_MSB, (*filterParameters >> 4) & 65535);
    write(DAP_COEF_WR_B0_LSB, (*filterParameters++) & 15);
    write(DAP_COEF_WR_B1_MSB, (*filterParameters >> 4) & 65535);
    write(DAP_COEF_WR_B1_LSB, (*filterParameters++) & 15);
    write(DAP_COEF_WR_B2_MSB, (*filterParameters >> 4) & 65535);
    write(DAP_COEF_WR_B2_LSB, (*filterParameters++) & 15);
    write(DAP_COEF_WR_A1_MSB, (*filterParameters >> 4) & 65535);
    write(DAP_COEF_WR_A1_LSB, (*filterParameters++) & 15);
    write(DAP_COEF_WR_A2_MSB, (*filterParameters >> 4) & 65535);
    write(DAP_COEF_WR_A2_LSB, (*filterParameters++) & 15);
    write(DAP_FILTER_COEF_ACCESS, (uint16_t)0x100 | filterNum);
}

unsigned short AudioControlSGTL5000::autoVolumeControl(uint8_t maxGain,
                                                       uint8_t lbiResponse,
                                                       uint8_t hardLimit,
                                                       float threshold,
                                                       float attack,
                                                       float decay)
{
    // if(semi_automated&&(!read(DAP_CONTROL)&1)) audioProcessorEnable();
    if (maxGain > 2)
        maxGain = 2;
    lbiResponse &= 3;
    hardLimit &= 1;
    uint8_t thresh = (pow(10, threshold / 20) * 0.636) * pow(2, 15);
    uint8_t att = (1 - pow(10, -(attack / (20 * 44100)))) * pow(2, 19);
    uint8_t dec = (1 - pow(10, -(decay / (20 * 44100)))) * pow(2, 23);
    write(DAP_AVC_THRESHOLD, thresh);
    write(DAP_AVC_ATTACK, att);
    write(DAP_AVC_DECAY, dec);
    return modify(
      DAP_AVC_CTRL, maxGain << 12 | lbiResponse << 8 | hardLimit << 5, 3 << 12 | 3 << 8 | 1 << 5);
}

unsigned short AudioControlSGTL5000::autoVolumeEnable(void)
{
    return modify(DAP_AVC_CTRL, 1, 1);
}

unsigned short AudioControlSGTL5000::autoVolumeDisable(void)
{
    return modify(DAP_AVC_CTRL, 0, 1);
}

unsigned short AudioControlSGTL5000::enhanceBass(float lr_lev, float bass_lev)
{
    return modify(DAP_BASS_ENHANCE_CTRL,
                  ((0x3F - calcVol(lr_lev, 0x3F)) << 8) | (0x7F - calcVol(bass_lev, 0x7F)),
                  (0x3F << 8) | 0x7F);
}

unsigned short AudioControlSGTL5000::enhanceBass(float lr_lev,
                                                 float bass_lev,
                                                 uint8_t hpf_bypass,
                                                 uint8_t cutoff)
{
    modify(DAP_BASS_ENHANCE, (hpf_bypass & 1) << 8 | (cutoff & 7) << 4, 1 << 8 | 7 << 4);
    return enhanceBass(lr_lev, bass_lev);
}

unsigned short AudioControlSGTL5000::enhanceBassEnable(void)
{
    return modify(DAP_BASS_ENHANCE, 1, 1);
}

unsigned short AudioControlSGTL5000::enhanceBassDisable(void)
{
    return modify(DAP_BASS_ENHANCE, 0, 1);
}

unsigned short AudioControlSGTL5000::surroundSound(uint8_t width)
{
    return modify(DAP_SGTL_SURROUND, (width & 7) << 4, 7 << 4);
}

unsigned short AudioControlSGTL5000::surroundSound(uint8_t width, uint8_t select)
{
    return modify(DAP_SGTL_SURROUND, ((width & 7) << 4) | (select & 3), (7 << 4) | 3);
}

unsigned short AudioControlSGTL5000::surroundSoundEnable(void)
{
    isSurroundSound = true;
    return modify(DAP_SGTL_SURROUND, 3, 3);
}

unsigned short AudioControlSGTL5000::surroundSoundDisable(void)
{
    isSurroundSound = false;
    return modify(DAP_SGTL_SURROUND, 0, 3);
}

bool AudioControlSGTL5000::isSurroundSoundEnable()
{
    return isSurroundSound;
}

unsigned char AudioControlSGTL5000::calcVol(float n, unsigned char range)
{
    // n=(n*(((float)range)/100))+0.499;
    n = (n * (float)range) + 0.499;
    if ((unsigned char)n > range)
        n = range;
    return (unsigned char)n;
}

// DAP_AUDIO_EQ_BASS_BAND0 & DAP_AUDIO_EQ_BAND1 & DAP_AUDIO_EQ_BAND2 etc etc
unsigned short AudioControlSGTL5000::dap_audio_eq_band(
  uint8_t bandNum,
  float n) // by signed percentage -100/+100; dap_audio_eq(3);
{
    n = (n * 48) + 0.499;
    if (n < -47)
        n = -47;
    if (n > 48)
        n = 48;
    n += 47;
    return modify(DAP_AUDIO_EQ_BASS_BAND0 + (bandNum * 2), (unsigned int)n, 127);
}

void AudioControlSGTL5000::automate(uint8_t, uint8_t eq)
{
    // if((dap!=0)&&(!(read(DAP_CONTROL)&1))) audioProcessorEnable();
    if ((read(DAP_AUDIO_EQ) & 3) != eq)
        eqSelect(eq);
}

void AudioControlSGTL5000::automate(uint8_t dap, uint8_t eq, uint8_t filterCount)
{
    automate(dap, eq);
    if (filterCount > (read(DAP_PEQ) & 7))
        eqFilterCount(filterCount);
}

// if(SGTL5000_PEQ) quantization_unit=524288; if(AudioFilterBiquad) quantization_unit=2147483648;
void calcBiquad(uint8_t filtertype,
                float fC,
                float dB_Gain,
                float Q,
                uint32_t quantization_unit,
                uint32_t fS,
                int* coef)
{

    // I used resources like http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
    // to make this routine, I tested most of the filter types and they worked. Such filters have
    // limits and before calling this routine with varying values the end user should check that
    // those values are limited to valid results.

    float A;
    if (filtertype < FILTER_PARAEQ)
        A = pow(10, dB_Gain / 20);
    else
        A = pow(10, dB_Gain / 40);
    float W0 = 2 * 3.14159265358979323846 * fC / fS;
    float cosw = cosf(W0);
    float sinw = sinf(W0);
    // float alpha = sinw*sinh((log(2)/2)*BW*W0/sinw);
    // float beta = sqrt(2*A);
    float alpha = sinw / (2 * Q);
    float beta = sqrtf(A) / Q;
    float b0, b1, b2, a0, a1, a2;

    switch (filtertype)
    {
        case FILTER_LOPASS:
            b0 = (1.0F - cosw) * 0.5F; // =(1-COS($H$2))/2
            b1 = 1.0F - cosw;
            b2 = (1.0F - cosw) * 0.5F;
            a0 = 1.0F + alpha;
            a1 = 2.0F * cosw;
            a2 = alpha - 1.0F;
            break;
        case FILTER_HIPASS:
            b0 = (1.0F + cosw) * 0.5F;
            b1 = -(cosw + 1.0F);
            b2 = (1.0F + cosw) * 0.5F;
            a0 = 1.0F + alpha;
            a1 = 2.0F * cosw;
            a2 = alpha - 1.0F;
            break;
        case FILTER_BANDPASS:
            b0 = alpha;
            b1 = 0.0F;
            b2 = -alpha;
            a0 = 1.0F + alpha;
            a1 = 2.0F * cosw;
            a2 = alpha - 1.0F;
            break;
        case FILTER_NOTCH:
            b0 = 1;
            b1 = -2 * cosw;
            b2 = 1;
            a0 = 1 + alpha;
            a1 = 2 * cosw;
            a2 = -(1 - alpha);
            break;
        case FILTER_PARAEQ:
            b0 = 1 + (alpha * A);
            b1 = -2 * cosw;
            b2 = 1 - (alpha * A);
            a0 = 1 + (alpha / A);
            a1 = 2 * cosw;
            a2 = -(1 - (alpha / A));
            break;
        case FILTER_LOSHELF:
            b0 = A * ((A + 1.0F) - ((A - 1.0F) * cosw) + (beta * sinw));
            b1 = 2.0F * A * ((A - 1.0F) - ((A + 1.0F) * cosw));
            b2 = A * ((A + 1.0F) - ((A - 1.0F) * cosw) - (beta * sinw));
            a0 = (A + 1.0F) + ((A - 1.0F) * cosw) + (beta * sinw);
            a1 = 2.0F * ((A - 1.0F) + ((A + 1.0F) * cosw));
            a2 = -((A + 1.0F) + ((A - 1.0F) * cosw) - (beta * sinw));
            break;
        case FILTER_HISHELF:
            b0 = A * ((A + 1.0F) + ((A - 1.0F) * cosw) + (beta * sinw));
            b1 = -2.0F * A * ((A - 1.0F) + ((A + 1.0F) * cosw));
            b2 = A * ((A + 1.0F) + ((A - 1.0F) * cosw) - (beta * sinw));
            a0 = (A + 1.0F) - ((A - 1.0F) * cosw) + (beta * sinw);
            a1 = -2.0F * ((A - 1.0F) - ((A + 1.0F) * cosw));
            a2 = -((A + 1.0F) - ((A - 1.0F) * cosw) - (beta * sinw));
            break;
        default:
            b0 = 0.5;
            b1 = 0.0;
            b2 = 0.0;
            a0 = 1.0;
            a1 = 0.0;
            a2 = 0.0;
    }

    a0 = (a0 * 2) / (float)quantization_unit; // once here instead of five times there...
    b0 /= a0;
    *coef++ = (int)(b0 + 0.499);
    b1 /= a0;
    *coef++ = (int)(b1 + 0.499);
    b2 /= a0;
    *coef++ = (int)(b2 + 0.499);
    a1 /= a0;
    *coef++ = (int)(a1 + 0.499);
    a2 /= a0;
    *coef++ = (int)(a2 + 0.499);
}
