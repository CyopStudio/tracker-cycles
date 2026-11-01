#include "TimerImpl.h"
#include "Drivers/ClockCyclesCounter.h"
#include "IntervalTimer.h"
#include "core_pins.h"
#include "interruptsConfig.h"

namespace
{
IntervalTimer s_midiClockTimer;
void (*volatile s_midiClockCallback)() = nullptr;

void onMidiClockTimer()
{
    s_midiClockTimer.end();
    void (*cb)() = s_midiClockCallback;
    s_midiClockCallback = nullptr;
    if (cb != nullptr)
    {
        cb();
    }
}
} // namespace

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
    return clockCyclesCounter.clockCycles();
}

uint32_t TimerImpl::clockCyclesPerUs()
{
    return F_CPU / 1000000;
}

void TimerImpl::scheduleMidiClockUs(uint32_t delayUs, void (*callback)())
{
    uint32_t irq_state;
    __asm__ volatile("mrs %0, primask\n\tcpsid i" : "=r"(irq_state)::"memory");
    void (*prevCb)() = s_midiClockCallback;
    if (prevCb != nullptr)
    {
        s_midiClockTimer.end();
        s_midiClockCallback = nullptr;
    }
    __asm__ volatile("msr primask, %0" ::"r"(irq_state) : "memory");

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

    __asm__ volatile("mrs %0, primask\n\tcpsid i" : "=r"(irq_state)::"memory");
    s_midiClockCallback = callback;
    s_midiClockTimer.priority(kMidiJackInUARTPriorityLevel);
    bool started = s_midiClockTimer.begin(onMidiClockTimer, static_cast<unsigned int>(delayUs));
    if (!started)
    {
        s_midiClockCallback = nullptr;
    }
    __asm__ volatile("msr primask, %0" ::"r"(irq_state) : "memory");

    if (!started && callback != nullptr)
    {
        callback();
    }
}

void TimerImpl::cancelMidiClock()
{
    uint32_t irq_state;
    __asm__ volatile("mrs %0, primask\n\tcpsid i" : "=r"(irq_state)::"memory");
    s_midiClockTimer.end();
    s_midiClockCallback = nullptr;
    __asm__ volatile("msr primask, %0" ::"r"(irq_state) : "memory");
}
