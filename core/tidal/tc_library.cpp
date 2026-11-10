// Builds tidal/core/library.cpp into every Original Tracker firmware (core/tidal/README.md).
#ifndef PLATFORM_IMX  // the Tidal runtime is part of every Original Tracker build
#include "tidal_config.h"
#include "../../tidal/core/library.cpp"
#endif
