// Throwaway on-device reverb bench (only with -DREVERB_BENCH): times the
// stock Polyverb reverb and cheaper variants (buffers in internal RAM,
// input diffusers in internal RAM, half sample rate) inside the audio
// interrupt, on a test signal (output discarded), and writes
// /reverb_bench.txt. See README.md.
#pragma once
#include <stdint.h>

class Platform;

#ifdef REVERB_BENCH
// Audio interrupt, after the callback's own work (outside the Profiler's
// audio-load measurement): runs one variant per block while capturing.
void reverbBenchBlock();
// Main loop: schedules the captures, writes the report.
void reverbBenchUpdate(Platform* platform);
#else
inline void reverbBenchBlock() {}
inline void reverbBenchUpdate(Platform*) {}
#endif
