#ifndef TRACKER_H_
#define TRACKER_H_

#include "Platform.h"

class Tracker
{
  public:
    virtual void init(Platform* platform) = 0;
    virtual void update() = 0;
    virtual void updateAudioBuffers(void* left, void* right, int length) = 0;
    virtual void updateInputAudioBuffers(void* dataI2s, void* dataUsb, int length) = 0;
    virtual void (*getStartDisplayFunc())(Platform*) = 0;
    virtual Platform* getPlatform() = 0;

    static Tracker* getInstance();

    static const char* getVersion();
};

#endif /* GROOVEBOX_H_ */
