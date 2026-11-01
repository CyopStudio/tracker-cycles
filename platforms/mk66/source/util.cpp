#include "Drivers/ClockCyclesCounter.h"
#include "core_pins.h"
#include "usb_serial.h"

void _debuglog(const char* msg)
{
    Serial.println(msg);
}

void _debuglogf(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    Serial.vprintf(format, args);

    va_end(args);
}

uint32_t cpuCycles()
{
    uint32_t clockCycles = clockCyclesCounter.clockCycles();
    return clockCycles;
}

void assert_func_play(const char*, int)
{
    asm("bkpt #0");
    while (1)
    {
    }
}

void __assert_func(const char* file, int line, const char*, const char*)
{
    assert_func_play(file, line);
    while (1)
    {
    }
}
