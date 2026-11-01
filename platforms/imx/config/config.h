#ifndef CONFIG_H_
#define CONFIG_H_

#define CPU_FREQ 720000000
#define SPREAD_SPECTRUM

#define AUDIO_CORE_FLASH __attribute__((section(".audio_flash_functions"), used))
#define AUDIO_CORE_RAM   __attribute__((section(".audio_ram_functions"), used))

static constexpr int kAudioBlockSamples = 128; // sync w config.inc

static constexpr int kFileBufferSize = 1024;
static constexpr int kFileOperationsExternalBufferSize = 1024 * 10;

static constexpr int kMaxPathLength = 255;

static constexpr char kConfigFilePath[] = "/trackerConfig";

static constexpr char kFirmwareInfoFilePath[] = "/Firmware/_fwinfo";
static constexpr char kTurnOnFilePath[] = "/Firmware/_turnon";

#endif /* CONFIG_CONFIG_H_ */
