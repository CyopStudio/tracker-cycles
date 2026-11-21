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

#ifdef __cplusplus
}
#endif

#endif
