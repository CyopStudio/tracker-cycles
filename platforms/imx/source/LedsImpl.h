#ifndef SOURCE_LEDSIMPL_H_
#define SOURCE_LEDSIMPL_H_

#ifndef TRACKER_MINI
#include "LedsDriver.h"
#endif
#include "Platform.h"

class I2cDevice;

class LedsImpl : public Leds
{
  public:
    virtual void setLed(int idx, bool enabled, int brightness) override;
#ifndef TRACKER_MINI
    void init();
    void turnOff();
    I2cDevice* getI2cDevice();

  private:
    mtLED leds;
#endif
};

#endif /* SOURCE_LEDSIMPL_H_ */
