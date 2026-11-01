#include "input_i2s.h"
#include "MIMXRT1051.h"
#include "core_cm7.h"
#include "fsl_dmamux.h"
#include "fsl_edma.h"
#include "fsl_sai.h"
#include "output_i2s.h"
#include "peripherals.h"

static uint32_t i2s_rx_buffer[2 * kAudioBlockSamples];
static uint32_t i2s_rx_buffer_intermediate[2 * kAudioBlockSamples];

void (*AudioInputI2S::updateBuffersFunc)(void* data) = nullptr;
AT_QUICKACCESS_SECTION_DATA_ALIGN(static edma_tcd_t s_emdaTcd, 32);

void EDMA_RX_Callback(edma_handle_t*, void*, bool, uint32_t)
{
    AudioInputI2S::isr();
}

void AudioInputI2S::begin(void (*updateBuffersFunc)(void* data))
{
    AudioInputI2S::updateBuffersFunc = updateBuffersFunc;

    AudioOutputI2S::config_i2s();

    EDMA_SetCallback(&SAI1_RX_Handle, EDMA_RX_Callback, nullptr);
    EDMA_ResetChannel(DMA0, SAI1_RX_Handle.channel);

    edma_transfer_config_t transferConfig = { 0 };
    uint32_t srcAddr = SAI_RxGetDataRegisterAddress(SAI1, 0);

    EDMA_PrepareTransfer(&transferConfig,
                         (void*)srcAddr,
                         kSAI_WordWidth16bits / 8U,
                         i2s_rx_buffer,
                         kSAI_WordWidth16bits / 8U,
                         (FSL_FEATURE_SAI_FIFO_COUNT - SAI1_Tx_config.fifo.fifoWatermark) *
                           (kSAI_WordWidth16bits / 8U),
                         sizeof(i2s_rx_buffer),
                         kEDMA_PeripheralToMemory);

    EDMA_TcdSetTransferConfig(&s_emdaTcd, &transferConfig, &s_emdaTcd);
    EDMA_TcdEnableInterrupts(&s_emdaTcd, kEDMA_MajorInterruptEnable | kEDMA_HalfInterruptEnable);
    EDMA_InstallTCD(DMA0, SAI1_RX_Handle.channel, &s_emdaTcd);
    EDMA_StartTransfer(&SAI1_RX_Handle);

    NVIC_SetPriority(DMA1_DMA17_IRQn, kAudioInputInterruptLevel);

    SAI_RxEnableDMA(SAI1, kSAI_FIFORequestDMAEnable, true);
    SAI_RxEnable(SAI1, true);
    /* Enable the channel FIFO */
    SAI_RxSetChannelFIFOMask(SAI1, 1U << 0);
}

void AudioInputI2S::isr(void)
{
    uint32_t daddr, offset;

    daddr = (uint32_t)(DMA0->TCD[SAI1_RX_Handle.channel].DADDR);

    if (daddr < (uint32_t)i2s_rx_buffer + sizeof(i2s_rx_buffer) / 2)
    {
        offset = kAudioBlockSamples;
    }
    else
    {
        offset = 0;
    }

    memcpy(&i2s_rx_buffer_intermediate[offset], &i2s_rx_buffer[offset], sizeof(i2s_rx_buffer) / 2);

    if (updateBuffersFunc != nullptr)
    {
        updateBuffersFunc(&i2s_rx_buffer_intermediate[offset]);
    }
}
