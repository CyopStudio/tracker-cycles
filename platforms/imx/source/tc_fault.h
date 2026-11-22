// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
//
// The last hard fault's registers, kept across the reset (tc_fault.c).

#ifndef TC_FAULT_H
#define TC_FAULT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kTcFaultMagic 0x7C0FA017u

typedef struct {
    uint32_t magic;  // kTcFaultMagic: a record (else garbage from power-up)
    uint32_t pc, lr, psr, cfsr, hfsr, mmfar, bfar, sp;
    uint32_t check;  // tcFaultSum
} TcFaultRecord;

extern volatile TcFaultRecord tcFault;
uint32_t tcFaultSum(const volatile TcFaultRecord* r);

// How far the boot got (set by the editor; kept with the fault): 0 not yet,
// 1 reading the code, 2 parsing / checking it, 3 loaded (first frames),
// 4 the first frame drawn, 5 the boot marker removed.
extern volatile uint8_t tcBootStage;

// The same fault in the SNVS low-power general purpose registers 4-7, which
// keep their value while the Mini is off (a reset there powers it off: RC3
// lost the SRAM record). pc, lr, cfsr; check = kTcSnvsMagic16 << 16 |
// stage << 8 | 8-bit sum. tcSnvsRead: 1 if a record was there (then cleared).
#define kTcSnvsMagic16 0xC7F1u
int tcSnvsRead(uint32_t* pc, uint32_t* lr, uint32_t* cfsr, uint8_t* stage);

#ifdef __cplusplus
}
#endif

#endif
