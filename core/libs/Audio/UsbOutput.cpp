#include "UsbOutput.h"
#include "Platform.h"

static int16_t audioUsbZeroBlock[AUDIO_BLOCK_SAMPLES] = {};

AUDIO_CORE_FLASH void UsbOutputStream::connectInputToUsbChannel(int source,
                                                                SourceChannel channel,
                                                                int usbChannel)
{
    assert(usbChannel < kUsbChannelsCount);
#ifdef PLATFORM_IMX
    int sourceIdx = SourceChannel::kChannelsCount * source + channel;
#else
    (void)source;  // Original Tracker: the master mix only (2 channels)
    int sourceIdx = channel;
#endif

    assert(sourceIdx < kInputChannelsCount);

    usbChannelToInput[usbChannel] = sourceIdx;
}

AUDIO_CORE_RAM void UsbOutputStream::update()
{
    if (nullptr == usbAudio)
    {
        return;
    }

    bool isAudioUsb = nullptr != usbAudio;

    for (int channel = 0; channel < kUsbChannelsCount; channel++)
    {
        audio_block_t* block = receiveReadOnly(usbChannelToInput[channel]);

        if (block != nullptr)
        {
            usbAudio->fillChannelData(block->data, AUDIO_BLOCK_SAMPLES, channel);
            release(block);
        }
        else
        {
            usbAudio->fillChannelData((int16_t*)audioUsbZeroBlock, AUDIO_BLOCK_SAMPLES, channel);
        }
    }
}
