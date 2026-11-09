// Device tick test build (plan step 4b on the MK66; only with -DTIDAL_TICK):
// the Tidal scheduler (tidal/core/scheduler.*) plays test patterns on the
// project's sample instruments. Queries run on the main loop, the audio
// interrupt only pops due entries (tidalTick) and calls the voices. Writes
// the interrupt / main-loop costs to /tidal_tick.txt. See README.md.
#pragma once

class Platform;

#ifdef TIDAL_TICK
// Main loop: starts the test, runs the scheduler's queries, writes reports.
void tidalTickUpdate(Platform* platform);
// Audio interrupt, once per block (where sequencer.tick runs).
void tidalTick();
#else
inline void tidalTickUpdate(Platform*) {}
inline void tidalTick() {}
#endif
