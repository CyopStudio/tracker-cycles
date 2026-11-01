#include "Input.h"

AUDIO_CORE_FLASH void AudioInputStream::update()
{
    audio_block_t* blockLeft = allocate();
    audio_block_t* blockRight = allocate();

    if (blockLeft == nullptr || blockRight == nullptr)
    {
        if (blockLeft)
        {
            release(blockLeft);
        }

        if (blockRight)
        {
            release(blockRight);
        }

        return;
    }

    if (data == nullptr)
    {
        for (int idx = 0; idx < AUDIO_BLOCK_SAMPLES; idx++)
        {
            blockLeft->data[idx] = 0;
            blockRight->data[idx] = 0;
        }
    }
    else
    {
        int16_t* inputData = (int16_t*)data;
        int16_t* leftData = blockLeft->data;
        int16_t* rightData = blockRight->data;

        for (int idx = 0; idx < AUDIO_BLOCK_SAMPLES; idx++)
        {
            *leftData++ = *inputData++;
            *rightData++ = *inputData++;
        }
    }

    transmit(blockLeft, 0);
    transmit(blockRight, 1);

    release(blockLeft);
    release(blockRight);
}

AUDIO_CORE_FLASH void AudioInputStream::setInBuffer(void* data)
{
    AudioInputStream::data = data;
}
