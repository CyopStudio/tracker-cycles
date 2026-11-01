#ifndef SOURCE_DRIVERS_CLOCKCYCLESCOUNTER_H_
#define SOURCE_DRIVERS_CLOCKCYCLESCOUNTER_H_
#include "stdint.h"

class ClockCyclesCounter
{
  public:
    void init();
    void enable(bool enable);
    void reset();
    uint32_t clockCycles();

  private:
};

extern ClockCyclesCounter clockCyclesCounter;

#endif /* SOURCE_DRIVERS_CLOCKCYCLESCOUNTER_H_ */
