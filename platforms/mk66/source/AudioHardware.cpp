#include "AudioHardware.h"
#include "Sgtl5000Impl.h"
#include "core_pins.h"

void AudioHardware::init(Sgtl5000* codec,
                         int audioOutputEnablePin,
                         void (*updateAudioBuffersFunc)(void* left, void* right),
                         void (*updateAudioInputFunc)(void* data))
{
    this->audioOutputEnablePin = audioOutputEnablePin;
    pinMode(this->audioOutputEnablePin, OUTPUT);
    enableOutput(false);

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
    digitalWrite(audioOutputEnablePin, enabled ? HIGH : LOW);
}

bool AudioHardware::isOutputEnabled()
{
    return digitalRead(audioOutputEnablePin);
}

AudioHardware audioHardware;
