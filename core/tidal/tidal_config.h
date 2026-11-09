// Firmware settings for the shared Tidal engine (tidal/core), included
// before each of its sources by the tc_*.cpp wrappers.
#pragma once
#include "cr_section_macros.h"

// The event arena (212 KB) is too big for internal RAM on the MK66.
#ifndef TC_ENGINE_ARENA_ATTR
#define TC_ENGINE_ARENA_ATTR __NOINIT(EXTERNAL_RAM)
#endif
