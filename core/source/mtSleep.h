#ifndef SOURCE_MTSLEEP_H_
#define SOURCE_MTSLEEP_H_

#include "elapsedMillis.h"

#include <stdint.h>

const uint16_t TURN_OFF_TIME = 1500;

enum enShutdownState
{
    shutdownStateNone,
    shutdownStateStart,
    shutdownStateSleep,
};

class mtSleep
{
  public:
    void startPowerOffSequence();
    void stopPowerOffSequence();
    uint16_t getTimeLeft();
    uint8_t getShutdownProgress();
    inline uint8_t getLowPowerState() { return shutdownState; }
    inline void setLowPowerState(uint8_t state) { shutdownState = state; }
    void goLowPower();
    void wakeUp();
    void resetMCU();

  private:
    uint8_t shutdownState = shutdownStateNone;
    elapsedMillis shutdownTimer;
};

extern mtSleep lowPower;

#endif /* SOURCE_MTSLEEP_H_ */
