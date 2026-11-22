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
volatile uint8_t tcBootStage = 0;

#define TC_CCM_CCGR5 (*(volatile uint32_t*)0x400FC07Cu)  // CG15 (bits 30-31): snvs_lp
#define TC_SNVS_LPGPR ((volatile uint32_t*)0x400D4100u)  // SNVS_LPGPR0 .. 7

static uint32_t snvsCheck(uint32_t pc, uint32_t lr, uint32_t cfsr, uint8_t stage) {
    uint32_t x = pc ^ (lr * 3u) ^ (cfsr * 5u) ^ stage ^ 0xA5u;
    x ^= x >> 16;
    x ^= x >> 8;
    return (kTcSnvsMagic16 << 16) | ((uint32_t)stage << 8) | (x & 0xFFu);
}

int tcSnvsRead(uint32_t* pc, uint32_t* lr, uint32_t* cfsr, uint8_t* stage) {
    TC_CCM_CCGR5 |= 3u << 30;
    const uint32_t p = TC_SNVS_LPGPR[4], l = TC_SNVS_LPGPR[5], c = TC_SNVS_LPGPR[6], k = TC_SNVS_LPGPR[7];
    const uint8_t st = (uint8_t)(k >> 8);
    if ((k >> 16) != kTcSnvsMagic16 || k != snvsCheck(p, l, c, st)) return 0;
    *pc = p;
    *lr = l;
    *cfsr = c;
    *stage = st;
    TC_SNVS_LPGPR[7] = 0;
    return 1;
}

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
    // the SNVS copy (survives the power-off a reset causes on the Mini)
    TC_CCM_CCGR5 |= 3u << 30;
    TC_SNVS_LPGPR[4] = tcFault.pc;
    TC_SNVS_LPGPR[5] = tcFault.lr;
    TC_SNVS_LPGPR[6] = tcFault.cfsr;
    TC_SNVS_LPGPR[7] = snvsCheck(tcFault.pc, tcFault.lr, tcFault.cfsr, tcBootStage);
    __asm volatile("dsb");
    *(volatile uint32_t*)0xE000ED0Cu = 0x05FA0004u;  // AIRCR: SYSRESETREQ
    __asm volatile("dsb");
    for (;;) {
    }
}
