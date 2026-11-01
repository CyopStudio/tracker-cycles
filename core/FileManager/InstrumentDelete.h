#ifndef FILEMANAGER_INSTRUMENTDELETE_H_
#define FILEMANAGER_INSTRUMENTDELETE_H_

#include "FileManagerOperation.h"

#include <stdint.h>

class InstrumentDelete : public FileManagerOperation
{
  public:
    virtual void update() override;

    bool deleteInstruments(uint8_t instrumentSlotFrom, uint8_t instrumentSlotTo);

  private:
    enum InstrumentDeleteStep
    {
        initStep,
        deleteSampleFiles,
        manageMemory,
        finishStep,
    };

    InstrumentDeleteStep operationStep;

    uint8_t deleteStartIdx;
    uint8_t deleteEndIdx;

    void deleteInstrumentInit();
    void deleteSamples();
    void deleteInstrumentFinish();
};

#endif /* FILEMANAGER_INSTRUMENTDELETE_H_ */
