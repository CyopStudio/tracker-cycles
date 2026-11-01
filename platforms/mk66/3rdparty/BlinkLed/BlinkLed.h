
#ifndef _BLINK_LED_H
#define _BLINK_LED_H

#include "stdint.h"
#define BLINK_ONCE_TIME 200

class cBlinkLed
{
  public:
    void begin(uint8_t pin);
    void update();
    void blinkOnce();
    void blinkOnceWithDelay();
    void blinkXTimes(uint8_t x);

  private:
    uint32_t timeMs;
    uint8_t _pin;
    uint8_t pinState;
    uint16_t setTime;
    int16_t setCount;
};

extern cBlinkLed BlinkLed;

#endif
