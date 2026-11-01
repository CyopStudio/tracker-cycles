#ifndef SOURCE_TIMERIMPL_H_
#define SOURCE_TIMERIMPL_H_

#include "Platform.h"

class TimerImpl : public Timer
{
  public:
    virtual uint32_t millis() override;
    virtual uint32_t micros() override;
    virtual void delay(int ms) override;
    // implementation based on millis only to provide compatibility with Tracker API
    virtual uint32_t clockCycles() override;
    virtual uint32_t clockCyclesPerUs() override;
    virtual void scheduleMidiClockUs(uint32_t delayUs, void (*callback)()) override;
    virtual void cancelMidiClock() override;
};

#endif /* SOURCE_TIMERIMPL_H_ */
