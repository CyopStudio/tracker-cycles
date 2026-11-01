#include "ExternalSdram.h"
#include "TimeControl.h"
#include "fsl_debug_console.h"
#include "fsl_semc.h"
#include "log.h"
#include <random>

//#define RAM_TEST

enum ResultType
{
    kSuccessful = 0,
    kSequentialError,
    kRandomError,
};

void ExternalSDRAM::init()
{
    // sdram is initialized in the common function: BOARD_InitBootPeripherals() generated from the
    // .mex file
}

void ExternalSDRAM::update()
{
#ifdef RAM_TEST
    static uint32_t updateTime = timeControl.millis();
    uint32_t kUpdateTimeMs = 2000;
    if ((timeControl.millis() - updateTime) > kUpdateTimeMs)
    {
        test();
        updateTime = timeControl.millis();
    }
#endif
}

void ExternalSDRAM::setLowPowerMode(bool)
{
    // sdram low power is handled by hardware (without using uC)
}

bool ExternalSDRAM::test()
{
    int* const kExternalRamAddr = (int*)0x80000000;

    bool result = testRamBlock(0, kExternalRamAddr);

    return result;
}

bool ExternalSDRAM::testRamBlock(int blockIdx, int* const startAddress)
{
    int testResult = kSuccessful;
    std::minstd_rand rand;

    rand.seed(timeControl.micros());
    std::uniform_int_distribution<> distrib(0, kRamSize - 1);

    __disable_irq();

    for (int idx = 0; idx < kRamSize; idx++)
    {
        startAddress[idx] = idx;
    }

    for (int idx = 0; idx < kRamSize; idx++)
    {
        if (startAddress[idx] != idx)
        {
            testResult |= kSequentialError;
            break;
        }
    }

    for (int testNum = 0; testNum < kRamSize; testNum++)
    {

        int idx = distrib(rand);
        startAddress[idx] = idx;

        idx = distrib(rand);
        if (startAddress[idx] != idx)
        {
            testResult |= kRandomError;
            break;
        }
    }

    __enable_irq();
#ifdef DEBUG
    int blockIdxToPrint = blockIdx + 1;
    LOGF("**************************************\n");
    LOGF("Start Test RAM %d\n", blockIdxToPrint);
    LOGF("Start addr: %x\n", (unsigned int)startAddress);

    if (testResult == kSuccessful)
    {
        LOGF("Test RAM %d successful\n", blockIdxToPrint);
    }
    else
    {
        LOGF("Test RAM %d failed on: %s%s\n",
             blockIdxToPrint,
             testResult & kSequentialError ? "-sequential read" : "",
             testResult & kRandomError ? " -random read/write" : "");
    }
#else
    (void)blockIdx;
#endif

    return testResult == kSuccessful;
}

ExternalSDRAM externalRAM;
