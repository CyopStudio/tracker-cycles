#ifndef _MT_PADS_BACKLIGHT_H
#define _MT_PADS_BACKLIGHT_H

#include "elapsedMillis.h"

#include <stdint.h>

class Leds;

class cMtPadsBacklight
{
  public:
    void init(uint16_t blink, Leds* leds);
    void update();
    void startBlink(uint8_t gamma_pwm, uint8_t n);
    void stopBlink(uint8_t n);
    void setFrontLayer(uint8_t state, uint8_t gamma_pwm, uint8_t n);
    void setBackLayer(uint8_t state, uint8_t gamma_pwm, uint8_t n);
    void clearGlobalPads(uint8_t front, uint8_t back, uint8_t blink);

  private:
    elapsedMillis blinkTimer;
    uint8_t frontLayer[48];
    uint8_t backLayer[48];
    uint8_t blinkLayer[48];
    uint16_t blinkConstrain;
    uint8_t blinkCounter;
    uint8_t toggler;
    Leds* leds;
};

extern cMtPadsBacklight padsBacklight;

#endif
