// Builds tidal/core/types.cpp into the firmware (only with -DTIDAL_BENCH for
// now; see core/tidal/README.md).
#ifdef TIDAL_BENCH
#include "tidal_config.h"
#include "../../tidal/core/types.cpp"
#endif
