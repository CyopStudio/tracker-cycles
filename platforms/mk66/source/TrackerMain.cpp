#include "PlatformImpl.h"
#include "Tracker.h"
#include "config.h"

void initHardware(PlatformImpl* platform,
                  const char* version,
                  void (*updateAudioBuffersFunc)(void* left, void* right),
                  void (*updateAudioInputFunc)(void* data),
                  void (*startDisplayFunc)(Platform*));
extern void updateHardware();

Tracker* trackerInstance;
PlatformImpl platform;

extern "C" void restartMCU()
{
    platform.resetMCU();
}

void updateBuffers(void* bufLeft, void* bufRight)
{
    if (trackerInstance != nullptr)
    {
        trackerInstance->updateAudioBuffers(bufLeft, bufRight, kAudioBlockSamples);
    }
}

void updateInputBuffers(void* data)
{
    if (trackerInstance != nullptr)
    {
        trackerInstance->updateInputAudioBuffers(data, nullptr, kAudioBlockSamples);
    }
}

//=======================================================================
void setup()
{
    trackerInstance = Tracker::getInstance();

    initHardware(&platform,
                 Tracker::getVersion(),
                 updateBuffers,
                 updateInputBuffers,
                 trackerInstance->getStartDisplayFunc());

    platform.init();

    trackerInstance->init(&platform);
}

//=======================================================================
void loop()
{
    updateHardware();

    platform.update();

    trackerInstance->update();
}
