#include "effect_bitDepth.h"

AUDIO_CORE_RAM void AudioBitDepth::update(void)
{
    audio_block_t* in = NULL;

    in = receiveWritable(0);

    if (!in)
        return;

    if (bitDepth != 16)
    {
        uint32_t* input_data = (uint32_t*)in->data;

        for (uint8_t i = 0; i < AUDIO_BLOCK_SAMPLES / 2; i++)
        {
            input_data[i] &= bitCrusherMask;
        }
    }

    transmit(in);
    release(in);
}

AUDIO_CORE_FLASH void AudioBitDepth::setBitDepth(uint8_t val)
{
    if (val < 4)
        bitDepth = 4;
    else if (val > 16)
        bitDepth = 16;
    else
        bitDepth = val;

    uint8_t maskSize = 16 - val;
    uint16_t singleBitcrusherMask = ~((1 << maskSize) - 1);
    bitCrusherMask = singleBitcrusherMask | (singleBitcrusherMask << 16);
}
