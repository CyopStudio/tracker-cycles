
#include "BlinkLed.h"
#include "core_pins.h"

cBlinkLed BlinkLed;

void cBlinkLed::begin(uint8_t pin)
{
    _pin = pin;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, HIGH);
    pinState = 0;
}

void cBlinkLed::update()
{
    if (setCount > 0 && (millis() - timeMs) > setTime)
    {
        if (pinState)
        {
            digitalWrite(_pin, HIGH);
            setCount--;
            pinState = 0;
        }
        else
        {
            digitalWrite(_pin, LOW);
            pinState = 1;
        }

        timeMs = millis();
    }
}

void cBlinkLed::blinkOnceWithDelay()
{
    digitalWrite(_pin, LOW);
    delay(200);
    digitalWrite(_pin, HIGH);
}

void cBlinkLed::blinkOnce()
{
    digitalWrite(_pin, LOW);
    setTime = BLINK_ONCE_TIME;
    setCount = 1;
    timeMs = millis();
}

void cBlinkLed::blinkXTimes(uint8_t x)
{
    if (x == 0)
        return;

    digitalWrite(_pin, LOW);
    setTime = BLINK_ONCE_TIME;
    setCount = x;
    timeMs = millis();
}
