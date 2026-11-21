// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
//
// Hard fault record (Tracker Mini): semihost_hardfault.c jumps here with the
// stacked exception frame instead of spinning (a crash looked like a freeze
// with nothing on the screen). The registers go into SRAM_ITC no-init
// memory, which a reset leaves alone, and the MCU resets; the next boot
// reports them (core/tidal/tidal_editor.cpp, Workspace/tidal_fault.txt).

#include <stdint.h>

#include "tc_fault.h"

__attribute__((section(".noinit.$SRAM_ITC"))) volatile TcFaultRecord tcFault;

uint32_t tcFaultSum(const volatile TcFaultRecord* r) {
    return r->pc ^ (r->lr * 3u) ^ (r->psr * 5u) ^ (r->cfsr * 7u) ^ (r->hfsr * 11u) ^ (r->mmfar * 13u) ^
           (r->bfar * 17u) ^ (r->sp * 19u) ^ 0x5A5AA5A5u;
}

__attribute__((used, noreturn)) void tcFaultRecord(const uint32_t* frame) {
    tcFault.pc = frame[6];
    tcFault.lr = frame[5];
    tcFault.psr = frame[7];
    tcFault.sp = (uint32_t)frame;
    tcFault.cfsr = *(volatile uint32_t*)0xE000ED28u;
    tcFault.hfsr = *(volatile uint32_t*)0xE000ED2Cu;
    tcFault.mmfar = *(volatile uint32_t*)0xE000ED34u;
    tcFault.bfar = *(volatile uint32_t*)0xE000ED38u;
    tcFault.check = tcFaultSum(&tcFault);
    tcFault.magic = kTcFaultMagic;
    __asm volatile("dsb");
    *(volatile uint32_t*)0xE000ED0Cu = 0x05FA0004u;  // AIRCR: SYSRESETREQ
    __asm volatile("dsb");
    for (;;) {
    }
}
