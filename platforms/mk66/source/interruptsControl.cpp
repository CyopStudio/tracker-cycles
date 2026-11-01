#include "MK66F18.h"
#include "core_cm4.h"

void enableAudioInterrupt()
{
    NVIC_EnableIRQ(IRQn_Type::DMA1_DMA17_IRQn);
}

void disableAudioInterrupt()
{
    NVIC_DisableIRQ(IRQn_Type::DMA1_DMA17_IRQn);
}
