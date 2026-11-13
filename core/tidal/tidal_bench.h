// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Throwaway on-device timing build for the Tidal engine (CLAUDE.md plan
// step 2). Only with -DTIDAL_BENCH.
#pragma once

class Platform;

#ifdef TIDAL_BENCH
// Call from the main loop. Runs the engine bench 10 s after boot and again
// at 60 s (start a project playing in between), shows a popup, and writes
// /tidal_bench.txt on the SD card.
void tidalBenchUpdate(Platform* platform);
#else
inline void tidalBenchUpdate(Platform*) {}
#endif
