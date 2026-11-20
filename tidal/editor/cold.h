// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Size-optimised editor code. The editor files that only run once per key
// press (building edits and menu items: controls, edits, wrap, pads, nav,
// keyboard) are compiled at -Os on ARM: ~19 KB less flash. The paths that run
// every frame or many times per key (layout, cursor tokens, jog, menu
// checking, the edit machinery) stay at the build's -O2. Define TC_EDITOR_O2
// to build everything at -O2 (timing comparisons).
//
//   #include "cold.h"
//   TC_COLD_BEGIN
//   ... definitions ...
//   TC_COLD_END
//
// push / pop keep it to the file even in the device's unity build
// (core/tidal/tc_editor.cpp). Put TC_COLD_BEGIN after the includes, so
// inline functions from headers keep the build's options.
#pragma once

#if defined(__arm__) && !defined(TC_EDITOR_O2)
#define TC_COLD_BEGIN _Pragma("GCC push_options") _Pragma("GCC optimize(\"Os\")")
#define TC_COLD_END _Pragma("GCC pop_options")
#else
#define TC_COLD_BEGIN
#define TC_COLD_END
#endif
