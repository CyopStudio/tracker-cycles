#include "AudioHardware.h"
#include "Sgtl5000Impl.h"
#include "fsl_gpio.h"
#include "pin_mux.h"

void AudioHardware::init(Sgtl5000* codec,
                         void (*updateAudioBuffersFunc)(void* left, void* right),
                         void (*updateAudioInputFunc)(void* data))
{
    i2sOut.begin(updateAudioBuffersFunc);
    i2sInput.begin(updateAudioInputFunc);

    codec->init();
}

void AudioHardware::update()
{
    //
}
void AudioHardware::enableOutput(bool enabled)
{
    // audio output pin is initialized in common file generated from .mex
    GPIO_WritePinOutput(BOARD_INITPINS_AUDIO_OUTPUT_ENABLE_PERIPHERAL,
                        BOARD_INITPINS_AUDIO_OUTPUT_ENABLE_PIN,
                        enabled);
}

bool AudioHardware::isOutputEnabled()
{
    return GPIO_ReadPinInput(BOARD_INITPINS_AUDIO_OUTPUT_ENABLE_PERIPHERAL,
                             BOARD_INITPINS_AUDIO_OUTPUT_ENABLE_PIN);
}

AudioHardware audioHardware;
