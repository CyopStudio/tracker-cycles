// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Firmware settings for the shared Tidal engine (tidal/core), included
// before each of its sources by the tc_*.cpp wrappers.
#pragma once
#include "cr_section_macros.h"

// The event arena (212 KB) is too big for internal RAM on the MK66.
#ifndef TC_ENGINE_ARENA_ATTR
#define TC_ENGINE_ARENA_ATTR __NOINIT(EXTERNAL_RAM)
#endif

// The type checker (~27 KB) runs on evaluate only: SDRAM keeps internal
// RAM for the stack (tick v4 had only 1.9 KB of stack margin with it there).
#ifndef TC_CHECKER_ATTR
#define TC_CHECKER_ATTR __NOINIT(EXTERNAL_RAM)
#endif

// The editor bench's scratch text (internal RAM is the heap's)
#ifndef TC_EDIT_BENCH_ATTR
#define TC_EDIT_BENCH_ATTR __NOINIT(EXTERNAL_RAM)
#endif

// The timing bench and the tick test measure the evaluator's stack depth.
#if defined(TIDAL_BENCH) || defined(TIDAL_TICK)
#define TC_TRACK_STACK
#endif
