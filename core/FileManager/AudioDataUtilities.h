#ifndef FILEMANAGER_AUDIODATAUTILITY_H_
#define FILEMANAGER_AUDIODATAUTILITY_H_

#include "wavHeaderReader.h"

#include <stdint.h>

struct ConversionConfig
{
    uint8_t* inputData;
    uint32_t inputSize;
    int16_t* outPtrL;
    int16_t* outPtrR;

    uint16_t sourceChannelCount;
    uint16_t sourceBitsPerSample;
    uint32_t sourceSampleRate;
    uint16_t sourceFormat;
    uint32_t destSampleRate;
};

uint32_t convertAudioData(ConversionConfig& conversionConfig);

bool checkSampleValid(const strWavFileHeader& sampleHead);

#endif /* FILEMANAGER_AUDIODATAUTILITY_H_ */
