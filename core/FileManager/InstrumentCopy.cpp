#include "InstrumentCopy.h"

#include "VoiceStreamManager.h"
#include "fileManager.h"
#include "fileManagerDefs.h"
#include "fileTransfer.h"

#include "log.h"

#include <cstring>

void InstrumentCopy::update()
{
    InstrumentCopyStep nextStep = operationStep;

    switch (operationStep)
    {
        case InstrumentCopyStep::initStep:
        {
            copyInstrumentInit();
            nextStep = InstrumentCopyStep::manageMemory;
            break;
        }
        case InstrumentCopyStep::manageMemory:
        {
            newFileManager.moveSampleMemory(
              newFileManager.calcFirstSlotToMoveInMemory(copyDestSlot + 1),
              newSamplesSize - oldSamplesSize); // TODO

            mtProject.used_memory += (newSamplesSize - oldSamplesSize);

            copyInstrumentSampleMemory(copySrcSlot, copyDestSlot);

            nextStep = InstrumentCopyStep::copyInstrumentFiles;
            break;
        }
        case InstrumentCopyStep::copyInstrumentFiles:
        {
            bool result = copyInstruments(copySrcSlot, copyDestSlot, copyStep);
            if (result)
            {
                if (mtProject.instrument[copyDestSlot].isActive == 0)
                {
                    mtProject.used_memory -= newSamplesSize;
                }
                memoryToCopyDone += fileTransfer.getBytesComplited();
                fileTransfer.clearBytesComplited();
                nextStep = InstrumentCopyStep::finishStep;
            }
            break;
        }
        case InstrumentCopyStep::finishStep:
        {
            copyInstrumentFinish();
            break;
        }
        default: stopOperationWithError(fmCopyingInstrumentsError); break;
    }

    operationStep = nextStep;
}

float InstrumentCopy::getProgress()
{
    float progress = 0;

    switch (operationStep)
    {
        case initStep:
        {
            progress = 0.02;
            break;
        }
        case manageMemory:
        {
            progress = 0.1;
            break;
        }
        case copyInstrumentFiles:
        {
            float complited = memoryToCopyDone + fileTransfer.getBytesComplited();
            float memoryProgress =
              memoryToCopyTotal > 0 ? (complited / (float)memoryToCopyTotal * 0.9f) : 0;
            progress = 0.1 + memoryProgress;
            break;
        }
        default: break;
    }

    // LOGF(
    //  "ICopy op: %d, pr: %.2f | Done: %0f / %d \n", operationStep, progress, 0,
    //  memoryToCopyTotal);

    return progress < 1.0f ? progress : 1.0f;
}

//=================================================================================================
//=================================================================================================
void InstrumentCopy::copyInstrumentInit()
{
    copyStep = 0;
    oldSamplesSize = 0;

    if (mtProject.instrument[copyDestSlot].isActive == 1)
    {
        oldSamplesSize = newFileManager.getSampleMemorySize(copyDestSlot, copyDestSlot); // TODO
    }

    mtProject.instrument[copyDestSlot] = mtProject.instrument[copySrcSlot]; // TODO

    bool srcStereo = newFileManager.isInstrumentStereo(copySrcSlot);

    newFileManager.setInstrumentSampleAddress(
      copyDestSlot, srcStereo ? 2 : 1, mtProject.instrument[copySrcSlot].sample.length);

    if (mtProject.sampleMemory[copySrcSlot].isStreamed)
    {
        voiceStreamManager.copyInstrumentMetadata(copySrcSlot, copyDestSlot);
        if (srcStereo)
        {
            mtProject.sampleMemory[copyDestSlot].address[strSampleMemory::Channel::kRight] =
              mtProject.sampleMemory[copyDestSlot].address[strSampleMemory::Channel::kLeft] +
              mtProject.sampleMemory[copyDestSlot].ramFrames;
        }
    }

    newSamplesSize = newFileManager.getSampleMemorySize(copyDestSlot, copyDestSlot); // TODO

    memoryToCopyTotal = mtProject.instrument[copySrcSlot].sample.length * (srcStereo ? 4 : 2);
    memoryToCopyDone = 0;
}

void InstrumentCopy::copyInstrumentSampleMemory(int srcIdx, int destIdx)
{
    uint8_t* srcAddr = (uint8_t*)mtProject.instrument[srcIdx].sample.address;
    uint8_t* destAddr = (uint8_t*)mtProject.instrument[destIdx].sample.address;
    uint32_t ramFrames =
      mtProject.sampleMemory[destIdx].isStreamed
        ? mtProject.sampleMemory[destIdx].ramFrames
        : (mtProject.sampleMemory[destIdx].ramFrames > 0
             ? mtProject.sampleMemory[destIdx].ramFrames
             : mtProject.instrument[destIdx].sample.length);
    uint32_t size = ramFrames * 2;

    if (newFileManager.isInstrumentStereo(destIdx))
    {
        size *= 2;
    }

    memcpy(destAddr, srcAddr, size);
}

bool InstrumentCopy::copyInstruments(int srcIdx, int destIdx, uint8_t& copyState)
{
    (void)srcIdx;

    if (copyState == 0)
    {
        if (newFileManager.saveInstrument(destIdx) == 0)
        {
            newFileManager.completeLoadedInstrumentStruct(destIdx, false);
            stopOperationWithError(fmCopyError);
            return false;
        }
        copyState = 1;
    }

    if (copyState == 1)
    {
        if (newFileManager.saveSampleDataToWorkspace(destIdx))
        {
            return true;
        }
    }

    return false;
}

void InstrumentCopy::copyInstrumentFinish()
{
    newFileManager.setStatus(fmCopyingInstrumentsEnd);
    newFileManager.setCurrentOperation(fmNoOperation);
}

//=================================================================================================
//=================================================================================================
// ograniczone do 1 instrumentu
bool InstrumentCopy::copyInstrumentsInWorkspace(uint8_t copyInstrSrc,
                                                uint8_t copyInstrCount,
                                                uint8_t copyInstrDest)
{
    if (newFileManager.getStatus() != fmIdle)
        return false;
    if (newFileManager.getCurrentOperation() != fmNoOperation)
        return false;

    if (!mtProject.instrument[copyInstrSrc].isActive)
        return false;
    if (copyInstrSrc == copyInstrDest)
        return false;

    copySrcSlot = copyInstrSrc;
    copyDestSlot = copyInstrDest;
    copySlotsCount = copyInstrCount;

    firstSlotToMoveInMemory = newFileManager.calcFirstSlotToMoveInMemory(copyInstrDest + 1);

    newFileManager.setStatus(fmCopyingInstrumentsInWorkspace);
    newFileManager.setCurrentOperation(fmCopyInstrumentsInWorkspace);
    operationStep = InstrumentCopyStep::initStep;
    return true;
}

//=================================================================================================
//=================================================================================================

//=================================================================================================
//=================================================================================================
