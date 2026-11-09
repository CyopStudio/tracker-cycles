// Throwaway on-device audio profiler (only with -DAUDIO_PROFILE): times
// each stage of the audio callback and each node of the audio graph with
// the Cortex-M cycle counter, and writes /audio_profile.txt. Map node
// addresses to names with core/tidal/audio_profile.py. See README.md.
#pragma once
#include <stdint.h>

class Platform;

#ifdef AUDIO_PROFILE
// DWT->CYCCNT (enabled at boot by ClockCyclesCounter on MK66)
#define AUDIO_PROFILE_CYCLES() (*(volatile uint32_t*)0xE0001004u)
#define AUDIO_PROFILE_MARK(v) const uint32_t v = AUDIO_PROFILE_CYCLES()

// Audio interrupt: one graph node's update() took `cycles` (node = the
// AudioStream object, index = its position in the update list).
void audioProfileNode(int index, const void* node, uint32_t cycles);
// Audio interrupt, end of each callback: cycles per stage.
void audioProfileBlock(uint32_t midi, uint32_t seq, uint32_t players, uint32_t graph);
// Main loop: schedules the captures, writes the report.
void audioProfileUpdate(Platform* platform);
#else
#define AUDIO_PROFILE_MARK(v)
inline void audioProfileUpdate(Platform*) {}
#endif
