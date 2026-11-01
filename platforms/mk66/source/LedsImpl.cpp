#include "LedsImpl.h"

void LedsImpl::setLed(int idx, bool enabled, int brightness)
{
    leds.setLED(idx, enabled ? 1 : 0, brightness);
}

void LedsImpl::init(void (*restI2C)(void))
{
    leds.begin(restI2C);
    leds.setAllLEDPWM(leds.ledPWM, 0);
}

bool LedsImpl::update()
{
    return leds.update_all_leds();
}

void LedsImpl::turnOff()
{
    leds.setAllLEDPWM(leds.ledPWM, 0);
}
