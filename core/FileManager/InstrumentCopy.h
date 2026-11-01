#ifndef FILEMANAGER_INSTRUMENTCOPY_H_
#define FILEMANAGER_INSTRUMENTCOPY_H_

#include "FileManagerOperation.h"

#include <stdint.h>

class InstrumentCopy : public FileManagerOperation
{
  public:
    virtual void update() override;
    virtual float getProgress() override;

    bool copyInstrumentsInWorkspace(uint8_t copyInstrSrc,
                                    uint8_t copyInstrCount,
                                    uint8_t copyInstrDest);

  private:
    enum InstrumentCopyStep
    {
        initStep,
        manageMemory,
        copyInstrumentFiles,
        finishStep,
    };

    InstrumentCopyStep operationStep;

    uint8_t copyStep;
    uint8_t copySrcSlot;
    uint8_t copyDestSlot;
    uint8_t copySlotsCount;
    uint8_t firstSlotToMoveInMemory;
    int32_t oldSamplesSize;
    int32_t newSamplesSize;

    uint32_t memoryToCopyTotal;
    uint32_t memoryToCopyDone;

    void copyInstrumentInit();
    void copyInstrumentSampleMemory(int srcIdx, int destIdx);
    bool copyInstruments(int srcIdx, int destIdx, uint8_t& copyState);
    void copyInstrumentFinish();
};

#endif /* FILEMANAGER_INSTRUMENTCOPY_H_ */
