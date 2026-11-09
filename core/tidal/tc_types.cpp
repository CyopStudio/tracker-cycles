// Builds tidal/core/types.cpp into the firmware (only with -DTIDAL_BENCH or -DTIDAL_TICK for
// now; see core/tidal/README.md).
#if defined(TIDAL_BENCH) || defined(TIDAL_TICK)
#include "tidal_config.h"
#include "../../tidal/core/types.cpp"
#endif
