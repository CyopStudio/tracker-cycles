#include "InstrumentDelete.h"

#include "fileManager.h"
#include "fileManagerDefs.h"
#include "fileTransfer.h"

#include <cstring>

void InstrumentDelete::update()
{
    InstrumentDeleteStep nextStep = operationStep;

    switch (operationStep)
    {
        case InstrumentDeleteStep::initStep:
        {
            deleteInstrumentInit();
            nextStep = InstrumentDeleteStep::deleteSampleFiles;
            break;
        }
        case InstrumentDeleteStep::deleteSampleFiles:
        {
            deleteSamples();
            nextStep = InstrumentDeleteStep::manageMemory;
            break;
        }
        case InstrumentDeleteStep::manageMemory:
        {
            uint32_t deletedSamplesSize =
              newFileManager.getSampleMemorySize(deleteStartIdx, deleteEndIdx);

            newFileManager.moveSampleMemory(
              newFileManager.calcFirstSlotToMoveInMemory(deleteEndIdx + 1), -deletedSamplesSize);

            mtProject.used_memory -= deletedSamplesSize;

            nextStep = InstrumentDeleteStep::finishStep;
            break;
        }

        case InstrumentDeleteStep::finishStep:
        {
            deleteInstrumentFinish();
            break;
        }
        default: stopOperationWithError(fmDeleteInstrumentsError); break;
    }

    operationStep = nextStep;
}

//=================================================================================================
//=================================================================================================

void InstrumentDelete::deleteInstrumentInit()
{
    char instrFilePath[WORKSPACE_INSTRUMENTS_FORMAT_MAX_LENGTH];
    volatile int pathSize = sizeof(instrFilePath);

    for (uint8_t idx = deleteStartIdx; idx <= deleteEndIdx; idx++)
    {
        snprintf(instrFilePath, pathSize, cWorkspaceInstrumentFileFormatNew, idx + 1);

        if (fileSystem->exists(instrFilePath))
        {
            fileSystem->remove(instrFilePath);
        }
    }
}

void InstrumentDelete::deleteSamples()
{
    char sampleFilePath[WORKSPACE_SAMPLES_FORMAT_MAX_LENGTH];
    volatile int pathSize = sizeof(sampleFilePath);

    for (uint8_t idx = deleteStartIdx; idx <= deleteEndIdx; idx++)
    {
        snprintf(sampleFilePath, pathSize, cWorkspaceSamplesFilesFormat, idx + 1);

        if (fileSystem->exists(sampleFilePath))
        {
            fileSystem->remove(sampleFilePath);
        }
    }
}

void InstrumentDelete::deleteInstrumentFinish()
{
    for (uint8_t instr = deleteStartIdx; instr <= deleteEndIdx; instr++)
    {
        if (mtProject.instrument[instr].isActive == 1)
        {
            newFileManager.setDefaultActiveInstrument(&mtProject.instrument[instr]);
            newFileManager.completeLoadedInstrumentStruct(instr, false);
        }
    }

    newFileManager.setStatus(fmDeleteInstrumentsEnd);
    newFileManager.setCurrentOperation(fmNoOperation);
}

//=================================================================================================
//=================================================================================================

bool InstrumentDelete::deleteInstruments(uint8_t instrumentSlotFrom, uint8_t instrumentSlotTo)
{
    if (newFileManager.getStatus() != fmIdle)
        return false;
    if (newFileManager.getCurrentOperation() != fmNoOperation)
        return false;

    deleteStartIdx = instrumentSlotFrom;
    deleteEndIdx = instrumentSlotTo;

    newFileManager.setStatus(fmDeleteingInstruments);
    newFileManager.setCurrentOperation(fmDeleteInstruments);
    operationStep = InstrumentDeleteStep::initStep;
    return true;
}

//=================================================================================================
//=================================================================================================
