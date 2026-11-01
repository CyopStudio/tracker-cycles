#include "MIMXRT1051.h"
#include "config.h"

AUDIO_CORE_FLASH void enableAudioInterrupt()
{
    NVIC_EnableIRQ(IRQn_Type::DMA0_DMA16_IRQn);
}

AUDIO_CORE_FLASH void disableAudioInterrupt()
{
    NVIC_DisableIRQ(IRQn_Type::DMA0_DMA16_IRQn);
}
