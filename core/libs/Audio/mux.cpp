#include "mux.h"
#include "Audio.h"
AUDIO_CORE_FLASH void AudioMux::update(void)
{
    audio_block_t* in = nullptr;

    for (uint8_t channel = 0; channel < MUX_SIZE; channel++)
    {
        in = receiveReadOnly(channel);
        if (in)
        {
            if (channel == outChannel)
                transmit(in);
            release(in);
            in = nullptr;
        }
    }
}

AUDIO_CORE_FLASH void AudioMux::setOut(uint8_t channel)
{
    if (channel >= MUX_SIZE)
        return;

    AudioNoInterrupts();
    outChannel = channel;
    AudioInterrupts();
}
