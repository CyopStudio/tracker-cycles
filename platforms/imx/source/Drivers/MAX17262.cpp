#include "MAX17262.h"
#include "TimeControl.h"
#include "i2cBusController.h"
#include "log.h"
#include "polyassert.h"

#define DEBUG_COMMUNICATION 0

bool MAX17262::update()
{
    return true;
}

bool MAX17262::begin(uint16_t designCap, uint16_t vEmpty, uint16_t ichgTerm)
{
    assert(i2cBusController);

    this->designCap = designCap;
    this->vEmpty = vEmpty;
    this->ichgTerm = ichgTerm;

    int devVersion = getVersion();
    if (devVersion != 0x4039)
    {
        return false;
    }

    verify(true);

    return true;
}

bool MAX17262::verify(bool extend)
{
    uint16_t status = getStatus();
    bool firstRun = status & 2;
    if (status < 0)
    {
        return false;
    }

    int tempVEmpty = getVEmpty();
    if (tempVEmpty < 0)
    {
        return false;
    }

#if DEBUG_COMMUNICATION
    LOG("");

    int cap = readRegister(regRepCap);
    LOGF("regRepCap: %d\n", cap);

    cap = readRegister(regFullCapRep);
    LOGF("regFullCapRep: %d\n", cap);

    cap = readRegister(regDesignCap);
    LOGF("regDesignCap: %d\n", cap);

    cap = readRegister(regFullCapNom);
    LOGF("regFullCapNom: %d\n", cap);

    cap = readRegister(regModelCfg);
    LOGF("regModelCfg: 0x%.4X\n", cap);
#endif

    if (firstRun || (extend && (tempVEmpty != vEmpty)))
    {
        int timeout = 0;

        while (readRegister(regFStat) & 1)
        {
            delay(1);
            timeout++;
            if (timeout > 200)
            {
                return false;
            }
        }

        setDesignCap(designCap);
        setVEmpty(vEmpty);
        setIchgTerm(ichgTerm);

        // writeRegister(regFullCapRep, designCap * 2);
        // writeRegister(regRepCap, designCap * 2);

        writeRegister(regModelCfg, 0x8000); // update modelCfg register to 4.2V

        // wait until ModelCFG.Refresh == 0
        while (readRegister(regModelCfg) & 0x8000)
        {
            delay(1);
        }

        //        int attempt = 0;
        //        do
        //        {
        //            writeRegister(regdQAcc, designCap / 2); // Write dQAcc
        //            writeRegister(regdPAcc, 0x0C80);        // Write dPAcc
        //            delay(10);
        //            // writeRegister(regFullCapNom, designCap); // Write FullCapNom
        //
        //        } while ((/*(readRegister(regFullCapNom) != designCap) |*/
        //                  (readRegister(regdQAcc) != designCap / 2) | (readRegister(regdPAcc) !=
        //                  0xC80)) &&
        //                 attempt++ < 3);
        //
        //        int Update_Capacity = int(readRegister(0xFF) / 25600.00 * designCap);
        //        writeRegister(0x0F, Update_Capacity); // Write MixCap
        //        writeRegister(0x1F, Update_Capacity); // Write AvCap

        status = getStatus();
        status &= ~2;
        writeRegister(regStatus, status); // update status
    }

    return true;
}

int MAX17262::getVoltage()
{
    return (readRegister(regVCell) * 1.25) / 16; // = mV
}

int MAX17262::getSOC()
{
    return readRegister(regRepSOC) >> 8;
}

int MAX17262::getCurrent()
{
    return (int16_t)readRegister(regCurrent) * 0.15625;
}

bool MAX17262::isCharging()
{
    return ((int16_t)readRegister(regCurrent)) > 200;
}

bool MAX17262::isBatteryPresent()
{
    return (getStatus() >> 3) & 1;
}

int MAX17262::getVersion()
{
    return readRegister(regDevName);
}

int MAX17262::getVEmpty()
{
    int read = readRegister(regVEmpty);

    if (read < 0)
    {
        return -1;
    }

    return (((read >> 7) & 0x1FF) * 10);
}

uint16_t MAX17262::getFullCapacity()
{
    return readRegister(regFullCapRep) * 0.5;
}

uint16_t MAX17262::getCurrentCapacity()
{
    return readRegister(regRepCap) * 0.5;
}

uint16_t MAX17262::getTimeToEmpty()
{
    return readRegister(regTTE) / 10.667f;
}

uint16_t MAX17262::getTimeToFull()
{
    return readRegister(regTTF) / 10.667f;
}

uint16_t MAX17262::getVRecovery()
{
    return ((readRegister(regVEmpty) & 0x7F) * 40);
}

void MAX17262::setVEmpty(uint16_t mV)
{
    uint16_t reg = ((mV / 10) << 7) | (readRegister(regVEmpty) & 0x7F);

    writeRegister(regVEmpty, reg);
}

void MAX17262::setIchgTerm(uint16_t mA)
{
    writeRegister(regIchgTerm, (mA * 100000) / 15625);
}

void MAX17262::setDesignCap(uint16_t mAh)
{
    writeRegister(regDesignCap, mAh / 0.5f);
}

int MAX17262::getStatus()
{
    return readRegister(regStatus);
}

//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
int MAX17262::writeRegister(uint8_t reg, uint16_t value)
{
    uint8_t data[2];
    data[0] = value & 0xff;
    data[1] = (value >> 8) & 0xff;

    I2cBusController::OperationResult result =
      i2cBusController->sendData(MAX1704X_ADDR, reg, 1, data, sizeof(data));

    return (int)result;
}

int MAX17262::readRegister(uint8_t reg, uint8_t len)
{
    bool repeatRead = true;
    int errorCnt = 0;
    uint16_t readRegister;
    uint8_t data[sizeof(readRegister)];

    while (repeatRead)
    {
        I2cBusController::OperationResult result =
          i2cBusController->receiveData(MAX1704X_ADDR, reg, 1, data, sizeof(data));
        if (result == I2cBusController::OperationResult::kError)
        {
#if DEBUG_COMMUNICATION
            LOGF("\nbattery readRegister i2c read error");
#endif
        }

        if (result != I2cBusController::OperationResult::kOK)
        {
            errorCnt++;

            if (errorCnt % 10 == 0)
            {
#if DEBUG_COMMUNICATION
                LOGF("reset i2c bus");
#endif
                i2cBusController->unblockBus();
                i2cBusController->init();
            }

            if (errorCnt > 100)
            {
#if DEBUG_COMMUNICATION
                LOGF("reset i2c bus x10 failed");
#endif
                return -1;
            }
        }
        else
        {
            repeatRead = false;
            break;
        }
    }

    readRegister = data[0] | (data[1] << 8);

    return readRegister;
}

void MAX17262::writeAndVerifyRegister(char RegisterAddress, int RegisterValueToWrite)
{
    int attempt = 0;
    int RegisterValueRead = 0;

    do
    {
        writeRegister(RegisterAddress, RegisterValueToWrite);
        delay(1); // 1ms
        RegisterValueRead = readRegister(RegisterAddress);
    } while (RegisterValueToWrite != RegisterValueRead && attempt++ < 3);
}
