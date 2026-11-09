// Builds tidal/core/scheduler.cpp into the firmware (only with -DTIDAL_TICK
// for now; see core/tidal/README.md).
#ifdef TIDAL_TICK
#include "tidal_config.h"
#include "../../tidal/core/scheduler.cpp"
#endif
