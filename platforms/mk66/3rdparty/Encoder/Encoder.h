#ifndef __ENCODER_H__
#define __ENCODER_H__

#include <stdint.h>

class cEncoder
{
  public:
    void begin();
    void setResolution(uint16_t value);
    void setAcceleration(uint8_t value);
    int32_t read();

  private:
    void clear();
    int32_t readCounter();

    struct strEncoder
    {
        uint8_t lastDirection;
        int16_t diffrenceBlur;
        uint16_t resolution = 24;
        uint8_t speed = 0;
        int16_t globalDiff = 0;
    } encoder;

    static constexpr float ENCODER_PPR = 96.0f;
    static constexpr int MAX_16BIT = 0xFFFF;
    static constexpr int MAX_16BIT_FOR_CALC = 0x10000;

    volatile uint32_t readReg;
};

#endif
