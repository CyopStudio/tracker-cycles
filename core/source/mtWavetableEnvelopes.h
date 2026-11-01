#pragma once

#include "Platform.h"
#include "config.h"
#include <stdint.h>

class FileSystem;

constexpr int kWtEnvCount = 256;
constexpr int kWtEnvFactoryCount = 128;
constexpr int kWtEnvLength = 128;

extern int16_t g_wtEnvelopeBank[kWtEnvCount][kWtEnvLength];

void initWavetableEnvelopes();
void loadUserWavetablesFromSD(FileSystem* fs);

AUDIO_CORE_FLASH int8_t unpackWarpX(uint8_t wtBend);
AUDIO_CORE_FLASH int8_t unpackWarpY(uint8_t wtBend);
AUDIO_CORE_FLASH uint8_t packWarpXY(int8_t warpX, int8_t warpY);

AUDIO_CORE_FLASH float evalWavetableEnvelopeFloat(float phase01, uint8_t shapeIdx, uint8_t wtBend);
AUDIO_CORE_FLASH int32_t evalWavetableEnvelopeFixed30(uint32_t phaseQ16, uint8_t shapeIdx, uint8_t wtBend);
