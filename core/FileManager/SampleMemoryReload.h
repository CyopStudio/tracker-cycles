#ifndef FILEMANAGER_SAMPLEMEMORYRELOAD_H_
#define FILEMANAGER_SAMPLEMEMORYRELOAD_H_

#include "FileManagerOperation.h"

#include <stdint.h>

class SampleMemoryReload : public FileManagerOperation
{
  public:
    virtual void update() override;

    bool reload();

  private:
    enum SampleMemoryReloadStep
    {
        initStep,
        loadInstrumentStruct,
        loadInstrumentSample,
        finishStep,
    };

    SampleMemoryReloadStep operationStep;

    int currentInstrument;
    uint8_t sampleLoadStep;
    uint32_t currentInstrumentFileDataOffset;

    void sampleMemoryReloadInit();
    bool loadInstrStruct(int idx, uint32_t& instrumentFileDataOffsetOut);
    void sampleMemoryReloadFinish();
};

#endif /* FILEMANAGER_SAMPLEMEMORYRELOAD_H_ */
