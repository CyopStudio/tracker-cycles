#ifndef TIMECONTROL_H_
#define TIMECONTROL_H_

#include <stdint.h>

class TimeControl
{
  public:
    void init();

    uint32_t micros();
    uint32_t millis();

    void delayMicroseconds(uint32_t usec);
    void delay(uint32_t ms);

    uint32_t clockCycles();
    uint32_t clockCyclesPerUs() { return cpuCyclesInOneUs; };

  private:
    uint32_t cpuCyclesInOneMs;
    uint32_t cpuCyclesInOneUs;
};

extern TimeControl timeControl;

uint32_t millis();
uint32_t micros();
void delay(uint32_t valueMs);
void delayMicroseconds(uint32_t valueUs);

#endif /* TIMECONTROL_H_ */
