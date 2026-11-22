// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Builds tidal/core/mapping.cpp into every Original Tracker firmware (core/tidal/README.md).
#if !defined(PLATFORM_IMX) || defined(TIDAL_EDITOR)  // the Tidal runtime: every Original Tracker build, and the Tracker Mini editor
#include "tidal_config.h"
#include "../../tidal/core/mapping.cpp"
#endif
