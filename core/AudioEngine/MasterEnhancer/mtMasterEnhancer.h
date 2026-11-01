#ifndef AUDIOENGINE_MTMASTERENHANCER_H_
#define AUDIOENGINE_MTMASTERENHANCER_H_
#include "mtBassEnhance.h"
#include "mtFiveBandsGraphicEqualizer.h"
#include "mtSurroundSound.h"

class Sgtl5000;

class mtMasterEnhancer
{
  public:
    void setCodec(Sgtl5000* codec);
    void enable();
    void changeBassBoost(uint8_t bassBoostValue);
    void changeStereoBoost(uint8_t bassBoostValue);
    mtSurroundSound surroundSound;
    mtBassEnhance bassEnhance;
    mtFiveBandsGraphicEqualizer fiveBandsEq;

  private:
};

extern mtMasterEnhancer masterEnhancer;

#endif /* AUDIOENGINE_MTMASTERENHANCER_H_ */
