#include "TimerImpl.h"
#include "TimeControl.h"
#include "fsl_common.h"
#include "fsl_qtmr.h"
#include "interruptsConfig.h"

namespace
{
volatile bool s_qtmrInitialized = false;
volatile uint32_t s_ipgClkMhz = 180U;
void (*volatile s_midiClockCallback)() = nullptr;
} // namespace

extern "C" void TMR4_IRQHandler(void)
{
    QTMR_StopTimer(TMR4, kQTMR_Channel_0);
    QTMR_DisableInterrupts(TMR4, kQTMR_Channel_0, kQTMR_CompareInterruptEnable);
    QTMR_ClearStatusFlags(TMR4, kQTMR_Channel_0, kQTMR_CompareFlag | kQTMR_Compare1Flag);
    void (*cb)() = s_midiClockCallback;
    s_midiClockCallback = nullptr;
    if (cb != nullptr)
    {
        cb();
    }
    SDK_ISR_EXIT_BARRIER;
}

uint32_t TimerImpl::millis()
{
    return ::millis();
}

uint32_t TimerImpl::micros()
{
    return ::micros();
}

void TimerImpl::delay(int ms)
{
    ::delay(ms);
}

uint32_t TimerImpl::clockCycles()
{
    return timeControl.clockCycles();
}

uint32_t TimerImpl::clockCyclesPerUs()
{
    return timeControl.clockCyclesPerUs();
}

void TimerImpl::scheduleMidiClockUs(uint32_t delayUs, void (*callback)())
{
    if (!s_qtmrInitialized)
    {
        uint32_t primask = DisableGlobalIRQ();
        if (!s_qtmrInitialized)
        {
            uint32_t ipgHz = CLOCK_GetFreq(kCLOCK_IpgClk);
            if (ipgHz > 0U)
            {
                s_ipgClkMhz = ipgHz / 1000000U;
            }
            qtmr_config_t qtmrConfig;
            QTMR_GetDefaultConfig(&qtmrConfig);
            // At 180 MHz IPG clock (720 MHz CPU / 4), Divide_16 = 11.25 MHz (88.9 ns/tick, max 5825 us)
            qtmrConfig.primarySource = kQTMR_ClockDivide_16;
            QTMR_Init(TMR4, kQTMR_Channel_0, &qtmrConfig);
            NVIC_SetPriority(TMR4_IRQn, kMidiJackInUARTPriorityLevel);
            EnableIRQ(TMR4_IRQn);
            s_qtmrInitialized = true;
        }
        EnableGlobalIRQ(primask);
    }

    uint32_t primask = DisableGlobalIRQ();
    void (*prevCb)() = s_midiClockCallback;
    if (prevCb != nullptr)
    {
        QTMR_StopTimer(TMR4, kQTMR_Channel_0);
        QTMR_DisableInterrupts(TMR4, kQTMR_Channel_0, kQTMR_CompareInterruptEnable);
        QTMR_ClearStatusFlags(TMR4, kQTMR_Channel_0, kQTMR_CompareFlag | kQTMR_Compare1Flag);
        s_midiClockCallback = nullptr;
    }
    EnableGlobalIRQ(primask);

    if (prevCb != nullptr)
    {
        prevCb();
    }

    if (delayUs <= 2U)
    {
        if (callback != nullptr)
        {
            callback();
        }
        return;
    }

    // ticks = delayUs * (IPG_MHz / 16)
    uint32_t ticks = (delayUs * s_ipgClkMhz + 8U) / 16U;
    if (ticks < 1U)
    {
        ticks = 1U;
    }
    else if (ticks > 0xFFFFU)
    {
        ticks = 0xFFFFU;
    }

    primask = DisableGlobalIRQ();
    s_midiClockCallback = callback;
    QTMR_StopTimer(TMR4, kQTMR_Channel_0);
    TMR4->CHANNEL[kQTMR_Channel_0].CNTR = 0U;
    TMR4->CHANNEL[kQTMR_Channel_0].LOAD = 0U;
    QTMR_SetTimerPeriod(TMR4, kQTMR_Channel_0, static_cast<uint16_t>(ticks));
    QTMR_ClearStatusFlags(TMR4, kQTMR_Channel_0, kQTMR_CompareFlag | kQTMR_Compare1Flag);
    QTMR_EnableInterrupts(TMR4, kQTMR_Channel_0, kQTMR_CompareInterruptEnable);
    QTMR_StartTimer(TMR4, kQTMR_Channel_0, kQTMR_PriSrcRiseEdge);
    EnableGlobalIRQ(primask);
}

void TimerImpl::cancelMidiClock()
{
    if (!s_qtmrInitialized)
    {
        return;
    }
    uint32_t primask = DisableGlobalIRQ();
    QTMR_StopTimer(TMR4, kQTMR_Channel_0);
    QTMR_DisableInterrupts(TMR4, kQTMR_Channel_0, kQTMR_CompareInterruptEnable);
    QTMR_ClearStatusFlags(TMR4, kQTMR_Channel_0, kQTMR_CompareFlag | kQTMR_Compare1Flag);
    s_midiClockCallback = nullptr;
    EnableGlobalIRQ(primask);
}
