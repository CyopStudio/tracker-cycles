#ifndef CONFIG_H_
#define CONFIG_H_
#include "stdint.h"
// turn-on mode depending on build and device
#if defined(DEBUG) || defined(TRACKER_MINI)
static constexpr bool START_STATE_SAVE = false;
#else // RELEASE
static constexpr bool START_STATE_SAVE = true;
#endif

const uint32_t START_STATE_EEPROM_ADRESS = 0; // 1st byte = start_state
const uint32_t CONFIG_EEPROM_ADRESS = 10;     // 9 bytes backup, then config

static constexpr int kAudioBlockSamples = 128; // sync w config.inc

#endif /* CONFIG_CONFIG_H_ */
