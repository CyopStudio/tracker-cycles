// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Builds tidal/core/bench.cpp into the firmware (only with -DTIDAL_BENCH for
// now; see core/tidal/README.md).
#ifdef TIDAL_BENCH
#include "tidal_config.h"
#include "../../tidal/core/bench.cpp"
#endif
