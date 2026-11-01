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
#include "MIMXRT1051.h"
#include "core_cm7.h"
#include "fsl_dmamux.h"
#include "fsl_edma.h"
#include "fsl_sai.h"
#include "memcpy_audio.h"
#include "peripherals.h"

int16_t AudioOutputI2S::buffer_left[kAudioBlockSamples * 2];
int16_t AudioOutputI2S::buffer_right[kAudioBlockSamples * 2];
void (*AudioOutputI2S::updateBuffersFunc)(void* left, void* right) = nullptr;

static uint32_t i2s_tx_buffer[kAudioBlockSamples * 2];
AT_QUICKACCESS_SECTION_DATA_ALIGN(static edma_tcd_t s_emdaTcd, 32);

void EDMA_TX_Callback(edma_handle_t*, void*, bool, uint32_t)
{
    AudioOutputI2S::isr();
}

void AudioOutputI2S::begin(void (*updateBuffersFunc)(void* left, void* right))
{
    this->updateBuffersFunc = updateBuffersFunc;

    config_i2s();

    EDMA_SetCallback(&SAI1_TX_Handle, EDMA_TX_Callback, nullptr);
    EDMA_ResetChannel(DMA0, SAI1_TX_Handle.channel);

    edma_transfer_config_t transferConfig = { 0 };
    uint32_t destAddr = SAI_TxGetDataRegisterAddress(SAI1, 0);

    EDMA_PrepareTransfer(&transferConfig,
                         i2s_tx_buffer,
                         kSAI_WordWidth16bits / 8U,
                         (void*)destAddr,
                         kSAI_WordWidth16bits / 8U,
                         (FSL_FEATURE_SAI_FIFO_COUNT - SAI1_Tx_config.fifo.fifoWatermark) *
                           (kSAI_WordWidth16bits / 8U),
                         sizeof(i2s_tx_buffer),
                         kEDMA_MemoryToPeripheral);

    EDMA_TcdSetTransferConfig(&s_emdaTcd, &transferConfig, &s_emdaTcd);
    EDMA_TcdEnableInterrupts(&s_emdaTcd, kEDMA_MajorInterruptEnable | kEDMA_HalfInterruptEnable);
    EDMA_InstallTCD(DMA0, SAI1_TX_Handle.channel, &s_emdaTcd);
    EDMA_StartTransfer(&SAI1_TX_Handle);

    NVIC_SetPriority(DMA0_DMA16_IRQn, kAudioOutputInterruptLevel);
    SAI_TxEnableDMA(SAI1, kSAI_FIFORequestDMAEnable, true);
    SAI_TxEnable(SAI1, true);
    /* Enable the channel FIFO */
    SAI_TxSetChannelFIFOMask(SAI1, 1U << 0);
}

void AudioOutputI2S::isr(void)
{
    int16_t* dest;
    uint32_t offset;
    static bool firstHalf = false;

    firstHalf = !firstHalf;

    uint32_t saddr = (uint32_t)(DMA0->TCD[SAI1_TX_Handle.channel].SADDR);

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
        offset = 0;
    }

    if (updateBuffersFunc != nullptr)
    {
        (*updateBuffersFunc)(buffer_left + offset, buffer_right + offset);
    }

    memcpy_tointerleaveLR(dest, buffer_left + offset, buffer_right + offset);
}

void AudioOutputI2S::config_i2s(void)
{
    IOMUXC_GPR->GPR1 |= IOMUXC_GPR_GPR1_SAI1_MCLK_DIR_MASK;
}
