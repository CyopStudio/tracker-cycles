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

#include "output_i2s.h"
#include "config.h"
#include "memcpy_audio.h"

int16_t AudioOutputI2S::buffer_left[kAudioBlockSamples * 2];
int16_t AudioOutputI2S::buffer_right[kAudioBlockSamples * 2];
void (*AudioOutputI2S::updateBuffersFunc)(void* left, void* right) = nullptr;

DMAMEM static uint32_t i2s_tx_buffer[kAudioBlockSamples * 2];

DMAChannel AudioOutputI2S::dma(false);

void AudioOutputI2S::begin(void (*updateBuffersFunc)(void* left, void* right))
{
    this->updateBuffersFunc = updateBuffersFunc;
    dma.begin(true); // Allocate the DMA channel first

    // TODO: should we set & clear the I2S_TCSR_SR bit here?
    config_i2s();
    // CORE_PIN22_CONFIG = PORT_PCR_MUX(6); // pin 22, PTC1, I2S0_TXD0
    PORTE_PCR10 = PORT_PCR_MUX(4);

#if defined(KINETISK)
    dma.TCD->SADDR = i2s_tx_buffer;
    dma.TCD->SOFF = 2;
    dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    dma.TCD->NBYTES_MLNO = 2;
    dma.TCD->SLAST = -sizeof(i2s_tx_buffer);
    dma.TCD->DADDR = (void*)((uint32_t)&I2S0_TDR0 + 2);
    dma.TCD->DOFF = 0;
    dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
    dma.TCD->DLASTSGA = 0;
    dma.TCD->BITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
    dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
#endif
    dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_TX);
    dma.enable();

    I2S0_TCSR = I2S_TCSR_SR;
    I2S0_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;
    dma.attachInterrupt(isr);
    dma.setInterruptPriority(kAudioOutputInterruptLevel);
}

void AudioOutputI2S::isr(void)
{
    int16_t* dest;
    uint32_t saddr, offset;

    saddr = (uint32_t)(dma.TCD->SADDR);
    dma.clearInterrupt();
    if (saddr < (uint32_t)i2s_tx_buffer + sizeof(i2s_tx_buffer) / 2)
    {
        // DMA is transmitting the first half of the buffer
        // so we must fill the second half
        dest = (int16_t*)&i2s_tx_buffer[kAudioBlockSamples];
        offset = kAudioBlockSamples;
    }
    else
    {
        // DMA is transmitting the second half of the buffer
        // so we must fill the first half
        dest = (int16_t*)i2s_tx_buffer;
    }

    if (updateBuffersFunc != nullptr)
    {
        (*updateBuffersFunc)(buffer_left + offset, buffer_right + offset);
    }

    memcpy_tointerleaveLR(dest, buffer_left + offset, buffer_right + offset);
}

// MCLK needs to be 48e6 / 1088 * 256 = 11.29411765 MHz -> 44.117647 kHz sample rate
//
#if F_CPU == 96000000 || F_CPU == 48000000 || F_CPU == 24000000
// PLL is at 96 MHz in these modes
#define MCLK_MULT 2
#define MCLK_DIV 17
#elif F_CPU == 72000000
#define MCLK_MULT 8
#define MCLK_DIV 51
#elif F_CPU == 120000000
#define MCLK_MULT 8
#define MCLK_DIV 85
#elif F_CPU == 144000000
#define MCLK_MULT 4
#define MCLK_DIV 51
#elif F_CPU == 168000000
#define MCLK_MULT 8
#define MCLK_DIV 119
#elif F_CPU == 180000000
#define MCLK_MULT 16
#define MCLK_DIV 255
#define MCLK_SRC 0
#elif F_CPU == 192000000
#define MCLK_MULT 1
#define MCLK_DIV 17
#elif F_CPU == 216000000
#define MCLK_MULT 8
#define MCLK_DIV 153
#define MCLK_SRC 0
#elif F_CPU == 240000000
#define MCLK_MULT 4
#define MCLK_DIV 85
#elif F_CPU == 16000000
#define MCLK_MULT 12
#define MCLK_DIV 17
#else
#error "This CPU Clock Speed is not supported by the Audio library";
#endif

#ifndef MCLK_SRC
#if F_CPU >= 20000000
#define MCLK_SRC 3 // the PLL
#else
#define MCLK_SRC 0 // system clock
#endif
#endif

void AudioOutputI2S::config_i2s(void)
{
    SIM_SCGC6 |= SIM_SCGC6_I2S;
    SIM_SCGC7 |= SIM_SCGC7_DMA;
    SIM_SCGC6 |= SIM_SCGC6_DMAMUX;

    // if either transmitter or receiver is enabled, do nothing
    if (I2S0_TCSR & I2S_TCSR_TE)
        return;
    if (I2S0_RCSR & I2S_RCSR_RE)
        return;

    // enable MCLK output
    I2S0_MCR = I2S_MCR_MICS(MCLK_SRC) | I2S_MCR_MOE;
    while (I2S0_MCR & I2S_MCR_DUF)
        ;
    I2S0_MDR = I2S_MDR_FRACT((MCLK_MULT - 1)) | I2S_MDR_DIVIDE((MCLK_DIV - 1));

    // configure transmitter
    I2S0_TMR = 0;
    I2S0_TCR1 = I2S_TCR1_TFW(1); // watermark at half fifo size
    I2S0_TCR2 = I2S_TCR2_SYNC(0) | I2S_TCR2_BCP | I2S_TCR2_MSEL(1) | I2S_TCR2_BCD | I2S_TCR2_DIV(1);
    I2S0_TCR3 = I2S_TCR3_TCE;
    I2S0_TCR4 = I2S_TCR4_FRSZ(1) | I2S_TCR4_SYWD(31) | I2S_TCR4_MF | I2S_TCR4_FSE | I2S_TCR4_FSP |
                I2S_TCR4_FSD;
    I2S0_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

    // configure receiver (sync'd to transmitter clocks)
    I2S0_RMR = 0;
    I2S0_RCR1 = I2S_RCR1_RFW(1);
    I2S0_RCR2 = I2S_RCR2_SYNC(1) | I2S_TCR2_BCP | I2S_RCR2_MSEL(1) | I2S_RCR2_BCD | I2S_RCR2_DIV(1);
    I2S0_RCR3 = I2S_RCR3_RCE;
    I2S0_RCR4 = I2S_RCR4_FRSZ(1) | I2S_RCR4_SYWD(31) | I2S_RCR4_MF | I2S_RCR4_FSE | I2S_RCR4_FSP |
                I2S_RCR4_FSD;
    I2S0_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

    // configure pin mux for 3 clock signals
    // CORE_PIN23_CONFIG = PORT_PCR_MUX(6); // pin 23, PTC2, I2S0_TX_FS (LRCLK)
    // CORE_PIN9_CONFIG  = PORT_PCR_MUX(6); // pin  9, PTC3, I2S0_TX_BCLK
    // CORE_PIN11_CONFIG = PORT_PCR_MUX(6); // pin 11, PTC6, I2S0_MCLK

    PORTE_PCR11 = PORT_PCR_MUX(4); // LRCLK
    PORTE_PCR12 = PORT_PCR_MUX(4); // BCLK
    PORTE_PCR6 = PORT_PCR_MUX(4);  // MCLK
}

/******************************************************************/

void AudioOutputI2Sslave::begin(void)
{
    dma.begin(true); // Allocate the DMA channel first

    // pinMode(2, OUTPUT);

    AudioOutputI2Sslave::config_i2s();
    CORE_PIN22_CONFIG = PORT_PCR_MUX(6); // pin 22, PTC1, I2S0_TXD0
#if defined(KINETISK)
    dma.TCD->SADDR = i2s_tx_buffer;
    dma.TCD->SOFF = 2;
    dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    dma.TCD->NBYTES_MLNO = 2;
    dma.TCD->SLAST = -sizeof(i2s_tx_buffer);
    dma.TCD->DADDR = (void*)((uint32_t)&I2S0_TDR0 + 2);
    dma.TCD->DOFF = 0;
    dma.TCD->CITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
    dma.TCD->DLASTSGA = 0;
    dma.TCD->BITER_ELINKNO = sizeof(i2s_tx_buffer) / 2;
    dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
#endif
    dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_TX);
    dma.enable();

    I2S0_TCSR = I2S_TCSR_SR;
    I2S0_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE | I2S_TCSR_FRDE;
    dma.attachInterrupt(isr);
}

void AudioOutputI2Sslave::config_i2s(void)
{
    SIM_SCGC6 |= SIM_SCGC6_I2S;
    SIM_SCGC7 |= SIM_SCGC7_DMA;
    SIM_SCGC6 |= SIM_SCGC6_DMAMUX;

    // if either transmitter or receiver is enabled, do nothing
    if (I2S0_TCSR & I2S_TCSR_TE)
        return;
    if (I2S0_RCSR & I2S_RCSR_RE)
        return;

    // Select input clock 0
    // Configure to input the bit-clock from pin, bypasses the MCLK divider
    I2S0_MCR = I2S_MCR_MICS(0);
    I2S0_MDR = 0;

    // configure transmitter
    I2S0_TMR = 0;
    I2S0_TCR1 = I2S_TCR1_TFW(1); // watermark at half fifo size
    I2S0_TCR2 = I2S_TCR2_SYNC(0) | I2S_TCR2_BCP;

    I2S0_TCR3 = I2S_TCR3_TCE;
    I2S0_TCR4 = I2S_TCR4_FRSZ(1) | I2S_TCR4_SYWD(31) | I2S_TCR4_MF | I2S_TCR4_FSE | I2S_TCR4_FSP;

    I2S0_TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

    // configure receiver (sync'd to transmitter clocks)
    I2S0_RMR = 0;
    I2S0_RCR1 = I2S_RCR1_RFW(1);
    I2S0_RCR2 = I2S_RCR2_SYNC(1) | I2S_TCR2_BCP;

    I2S0_RCR3 = I2S_RCR3_RCE;
    I2S0_RCR4 = I2S_RCR4_FRSZ(1) | I2S_RCR4_SYWD(31) | I2S_RCR4_MF | I2S_RCR4_FSE | I2S_RCR4_FSP |
                I2S_RCR4_FSD;

    I2S0_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

    // configure pin mux for 3 clock signals
    CORE_PIN23_CONFIG = PORT_PCR_MUX(6); // pin 23, PTC2, I2S0_TX_FS (LRCLK)
    CORE_PIN9_CONFIG = PORT_PCR_MUX(6);  // pin  9, PTC3, I2S0_TX_BCLK
    CORE_PIN11_CONFIG = PORT_PCR_MUX(6); // pin 11, PTC6, I2S0_MCLK
}
