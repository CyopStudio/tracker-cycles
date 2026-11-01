#ifndef FILEMANAGER_SAMPLEMEMORYSAVE_H_
#define FILEMANAGER_SAMPLEMEMORYSAVE_H_

#include "FileManagerOperation.h"

#include <stdint.h>

class SampleMemorySave : public FileManagerOperation
{
  public:
    virtual void update() override;

    bool saveSampleFromSampleEditor(int16_t* memoryAddressL,
                                    int16_t* memoryAddressR,
                                    uint32_t samplesCount,
                                    uint8_t instrumentIdx);

  private:
    enum SampleMemorySaveStep
    {
        initStep,
        saveInstrumentStructToWorkspace,
        saveInstrumentSampleToWorkspace,
        finishStep,
    };

    SampleMemorySaveStep operationStep;

    int currentInstrument;
    uint8_t* memoryAddressR;
    uint8_t* memoryAddressL;
    uint32_t memorySizeBytes;

    void sampleMemorySaveInit();
    bool saveStereoDataToWorkspace(int idx,
                                   uint8_t* dataSourceAddressL,
                                   uint8_t* dataSourceAddressR,
                                   uint32_t dataSizeBytes);
    void sampleMemorySaveFinish();
};

#endif /* FILEMANAGER_SAMPLEMEMORYSAVE_H_ */
