#include "Drivers/ClockCyclesCounter.h"
#include "MK66F18.h"

ClockCyclesCounter clockCyclesCounter;

void ClockCyclesCounter::init()
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    enable(true);
    reset();
}

void ClockCyclesCounter::enable(bool enable)
{
    if (enable)
    {
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
    else
    {
        DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    }
}

void ClockCyclesCounter::reset()
{
    DWT->CYCCNT = 0;
}

uint32_t ClockCyclesCounter::clockCycles()
{
    return DWT->CYCCNT;
}
