#ifndef TRACKERIMPL_H_
#define TRACKERIMPL_H_

#ifdef FEATURE_SYNTH
#include "Synth.h"
#endif
#include "Tracker.h"

// Tracker is the main entry point to the module.
// It is instanced, initialized and driven by the client code.
class TrackerImpl final : public Tracker
{
  public:
    virtual void init(Platform* platform) override;
    virtual void update() override;
    virtual void updateAudioBuffers(void* left, void* right, int length) override;
    virtual void updateInputAudioBuffers(void* dataI2s, void* dataUsb, int length) override;
    virtual void (*getStartDisplayFunc())(Platform*) override;

    static TrackerImpl* getInstance();
    virtual Platform* getPlatform() override;
#ifdef FEATURE_SYNTH
    Synth& getSynth();
#endif

  protected:
    Platform* platform;
#ifdef FEATURE_SYNTH
    Synth synth;
#endif
};

#endif /* TRACKERIMPL_H_ */
