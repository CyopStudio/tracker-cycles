#ifndef SOURCE_LEDSIMPL_H_
#define SOURCE_LEDSIMPL_H_
#include "Platform.h"
#include "mtLED.h"

class LedsImpl : public Leds
{
  public:
    virtual void setLed(int idx, bool enabled, int brightness) override;

    void init(void (*restI2C)(void));
    bool update();
    void turnOff();

  private:
    mtLED leds;
};

#endif /* SOURCE_LEDSIMPL_H_ */
