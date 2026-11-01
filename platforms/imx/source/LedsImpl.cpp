#include "LedsImpl.h"
#ifdef TRACKER_MINI
void LedsImpl::setLed(int, bool, int)
{
}

#else
void LedsImpl::setLed(int idx, bool enabled, int brightness)
{
    leds.setLED(idx, enabled ? 1 : 0, brightness);
}

void LedsImpl::init()
{
    leds.begin();
    leds.setAllLEDPWM(leds.ledPWM, 0);
}

void LedsImpl::turnOff()
{
    leds.setAllLEDPWM(leds.ledPWM, 0);
}

I2cDevice* LedsImpl::getI2cDevice()
{
    return &leds;
}
#endif
