#include "stereoToMono.h"
#include "Audio.h"

AUDIO_CORE_FLASH void AudioStereoToMono::update(void)
{
    audio_block_t *inL = nullptr, *inR = nullptr;
    constexpr uint8_t LEFT_CHANNEL = 0;
    constexpr uint8_t RIGHT_CHANNEL = 1;

    inL = receiveReadOnly(LEFT_CHANNEL);
    inR = receiveReadOnly(RIGHT_CHANNEL);

    if (!inL || !inR)
    {
        if (inL)
            release(inL);
        if (inR)
            release(inR);

        return;
    }

    switch (signalType)
    {
        case OutSignalType::stereo:
            transmit(inL, LEFT_CHANNEL);
            transmit(inR, RIGHT_CHANNEL);
            break;
        case OutSignalType::monoL:
            transmit(inL, LEFT_CHANNEL);
            transmit(inL, RIGHT_CHANNEL);
            break;
        case OutSignalType::monoR:
            transmit(inR, LEFT_CHANNEL);
            transmit(inR, RIGHT_CHANNEL);
            break;
        default: break;
    }

    release(inR);
    release(inL);
}

AUDIO_CORE_FLASH void AudioStereoToMono::setOutSignalType(OutSignalType signalType)
{
    if (signalType >= OutSignalType::outSignalMax)
        return;
    this->signalType = signalType;
}
