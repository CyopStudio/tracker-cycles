#include "TimeControl.h"
#include "clock_config.h"

#include "interruptsConfig.h"

volatile uint32_t msTicks;
volatile uint32_t cyclesTicks;
uint32_t syncTicks;

#define NEW_USEC_CALCULATION 1

void TimeControl::init()
{
    SysTick_Config(SystemCoreClock / 1000);

#if NEW_USEC_CALCULATION
    __NVIC_SetPriority(SysTick_IRQn, kTimeControlPriorityLevel);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    cyclesTicks = DWT->CYCCNT;
    cpuCyclesInOneMs = 0xFFFFFFFF / (uint32_t)(SystemCoreClock / 1000000);
#endif
    cpuCyclesInOneUs = SystemCoreClock / 1000000;
}

uint32_t TimeControl::micros()
{
#if NEW_USEC_CALCULATION
    uint32_t smc, scc;
    do
    {
        __LDREXW(&syncTicks);
        smc = msTicks;
        scc = cyclesTicks;
    } while (__STREXW(1, &syncTicks));
    uint32_t cyccnt = DWT->CYCCNT;
    asm volatile("" : : : "memory");
    uint32_t ccdelta = cyccnt - scc;
    uint32_t frac = ((uint64_t)ccdelta * cpuCyclesInOneMs) >> 32;
    if (frac > 1000)
    {
        frac = 1000;
    }
    uint32_t usec = 1000 * smc + frac;
    return usec;
#else
    uint32_t count, current, istatus;

    __disable_irq();
    current = SysTick->VAL;
    count = msTicks;
    istatus = SCB->ICSR;
    __enable_irq();

    if ((istatus & SCB_ICSR_PENDSTSET_Msk) && current > 50)
    {
        count++;
    }
    current = ((SystemCoreClock / 1000) - 1) - current;

    return count * 1000 + current / (SystemCoreClock / 1000000);
#endif
}

uint32_t TimeControl::millis()
{
    volatile uint32_t ret = msTicks;
    return ret;
}

void TimeControl::delayMicroseconds(uint32_t usec)
{
    volatile uint32_t n = usec * 528;

    if (n == 0)
    {
        return;
    }

    __asm__ volatile("L_%=_delayMicroseconds:"
                     "\n\t"
                     "subs   %0, #1"
                     "\n\t"
                     "bne    L_%=_delayMicroseconds"
                     "\n"
                     : "+r"(n)
                     :);
}

void TimeControl::delay(uint32_t ms)
{
    uint32_t start = micros();

    if (ms > 0)
    {
        while (1)
        {
            while ((micros() - start) >= 1000)
            {
                ms--;
                if (ms == 0)
                {
                    return;
                }
                start += 1000;
            }
        }
    }
}

uint32_t TimeControl::clockCycles()
{
    return DWT->CYCCNT;
}

uint32_t millis()
{
    return timeControl.millis();
}

uint32_t micros()
{
    return timeControl.micros();
}

void delay(uint32_t valueMs)
{
    timeControl.delay(valueMs);
}

void delayMicroseconds(uint32_t valueUs)
{
    timeControl.delayMicroseconds(valueUs);
}

extern "C"
{
    void SysTick_Handler()
    {
        msTicks++;
        cyclesTicks = DWT->CYCCNT;
    }
}
