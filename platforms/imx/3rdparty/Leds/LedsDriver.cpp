#include "Leds/LedsDriver.h"

#include "TimeControl.h"
#include "i2cBusController.h"
#include <stdint.h>
#include <string.h>

#ifndef _swap_int16_t
#define _swap_int16_t(a, b)                                                                        \
    {                                                                                              \
        int16_t t = a;                                                                             \
        a = b;                                                                                     \
        b = t;                                                                                     \
    }
#endif

const uint8_t gamma255[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   2,   2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   4,
    4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,   8,
    9,   9,   9,   10,  10,  10,  11,  11,  11,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,
    16,  17,  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  24,  24,  25,  25,  26,
    27,  27,  28,  29,  29,  30,  31,  32,  32,  33,  34,  35,  35,  36,  37,  38,  39,  39,  40,
    41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  50,  51,  52,  54,  55,  56,  57,  58,  59,
    60,  61,  62,  63,  64,  66,  67,  68,  69,  70,  72,  73,  74,  75,  77,  78,  79,  81,  82,
    83,  85,  86,  87,  89,  90,  92,  93,  95,  96,  98,  99,  101, 102, 104, 105, 107, 109, 110,
    112, 114, 115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142, 144,
    146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175, 177, 180, 182, 184,
    186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213, 215, 218, 220, 223, 225, 228, 231,
    233, 236, 239, 241, 244, 247, 249, 252, 255

};

mtLED::mtLED()
{
    memset(ledState, 0, 144);
    memset(ledBits, 0, 18);
    memset(ledPWM, 0, 144);
}

void mtLED::begin(uint8_t addr)
{
    _i2caddr = addr;
    _frame = 0;
    // shutdown
    writeRegister8(ISSI_BANK_FUNCTIONREG, ISSI_REG_SHUTDOWN, 0x00);

    timeControl.delay(10);

    // out of shutdown
    writeRegister8(ISSI_BANK_FUNCTIONREG, ISSI_REG_SHUTDOWN, 0x01);

    // picture mode
    writeRegister8(ISSI_BANK_FUNCTIONREG, ISSI_REG_CONFIG, ISSI_REG_CONFIG_PICTUREMODE);

    displayFrame(_frame);

    // all LEDs on & 0 PWM
    setLEDOnOffBytes(ledBits, _frame);

    clear(); // set each led to 0 PWM

    audioSync(false);
}

bool mtLED::update()
{
    if (leds_need_update)
    {
        checkAndRepair();

        I2cBusController::OperationResult status =
          i2cBusController->sendData(_i2caddr, (uint32_t)ISSI_COMMANDREGISTER, 1, &_frame, 1);

        if (status == I2cBusController::OperationResult::kError)
        {
            isDeadResetCheck();
            return 1;
        }

        uint8_t commonBuffer[sizeof(ledBits) + 18 + sizeof(ledPWM)];
        uint32_t start = 0;

        for (uint32_t i = start; i < sizeof(ledBits); i++) // 0x11
        {
            commonBuffer[i] = ledBits[i - start];
        }
        start += sizeof(ledBits);

        for (uint32_t i = start; i < start + 18; i++) // 0x11
        {
            commonBuffer[i] = 0;
        }

        start += 18;
        for (uint32_t i = start; i < start + sizeof(ledPWM); i++)
        {
            commonBuffer[i] = ledPWM[i - start];
        }

        i2cBusController->sendData(
          _i2caddr, (uint32_t)0x00, 1, &commonBuffer, sizeof(commonBuffer));

        leds_need_update = 0;

        return true;
    }

    return false;
}

void mtLED::setLED(uint8_t num, uint8_t state, uint8_t gamma_pwm)
{

    if (gamma_pwm > 31)
        gamma_pwm = 31;

    num += 4 * (num / 12); // special for tracker 12 x 4 grid

    ledPWM[num] = gamma32[gamma_pwm];

    if (state)
        ledBits[num / 8] |= (1 << num % 8);
    else
        ledBits[num / 8] &= ~(1 << num % 8);
    leds_need_update = 1;
}

void mtLED::clear(void)
{
    selectBank(_frame);

    for (uint8_t i = 0; i < 6; i++)
    {
        uint8_t emptyData[18] = {};
        i2cBusController->sendData(
          _i2caddr, (uint32_t)0x24 + i * 24, 1, &emptyData, sizeof(emptyData));
    }
}

void mtLED::setAllLEDPWM(uint8_t* arr, uint8_t bank)
{
    i2cBusController->sendData(_i2caddr, (uint32_t)ISSI_COMMANDREGISTER, 1, &bank, 1);
    i2cBusController->sendData(_i2caddr, (uint32_t)0x24, 1, arr, 144);
}

void mtLED::displayFrame(uint8_t f)
{
    if (f > 7)
        f = 0;
    writeRegister8(ISSI_BANK_FUNCTIONREG, ISSI_REG_PICTUREFRAME, f);
}

void mtLED::selectBank(uint8_t b)
{
    i2cBusController->sendData(_i2caddr, (uint32_t)ISSI_COMMANDREGISTER, 1, &b, 1);
}

void mtLED::writeRegister8(uint8_t b, uint8_t reg, uint8_t data)
{

    I2cBusController::OperationResult status =
      i2cBusController->sendData(_i2caddr, (uint32_t)ISSI_COMMANDREGISTER, 1, &b, 1);

    if (status == I2cBusController::OperationResult::kError)
    {
        return;
    }

    i2cBusController->sendData(_i2caddr, (uint32_t)reg, 1, &data, 1);

    if (b == ISSI_BANK_FUNCTIONREG)
    {
        functionRegBuff[reg] = data;
    }
}

uint8_t mtLED::readRegister8(uint8_t bank, uint8_t reg)
{

    I2cBusController::OperationResult status =
      i2cBusController->sendData(_i2caddr, (uint32_t)ISSI_COMMANDREGISTER, 1, &bank, 1);

    if (status == I2cBusController::OperationResult::kError)
    {
        return 0;
    }

    uint8_t result = 0;
    i2cBusController->receiveData(_i2caddr, (uint32_t)reg, 1, &result, 1);

    return result;
}

bool mtLED::isDeadResetCheck(void)
{
    bool x;
    if (readRegister8(ISSI_BANK_FUNCTIONREG, ISSI_REG_SHUTDOWN) == 1)
    {
        x = 0;
    }
    else
    {
        i2cBusController->unblockBus();

        if (readRegister8(ISSI_BANK_FUNCTIONREG, ISSI_REG_CONFIG) != ISSI_REG_CONFIG_PICTUREMODE)
        {
            begin(_i2caddr);
        }
    }

    return x;
}

bool mtLED::checkAndRepairReg(uint8_t reg)
{
    if (readRegister8(ISSI_BANK_FUNCTIONREG, reg) != functionRegBuff[reg])
    {
        writeRegister8(ISSI_BANK_FUNCTIONREG, reg, functionRegBuff[reg]);
        return true;
    }

    return false;
}

void mtLED::checkAndRepair(void)
{

    for (uint8_t reg = 0; reg <= 0x0D; reg++)
    {
        if (reg == 0x07)
            continue;

        checkAndRepairReg(reg);
    }
}

void mtLED::setLEDOnOffBytes(uint8_t* arr, uint8_t bank)
{
    i2cBusController->sendData(_i2caddr, (uint32_t)ISSI_COMMANDREGISTER, 1, &bank, 1);
    i2cBusController->sendData(_i2caddr, (uint32_t)0x00, 1, arr, 18);
}

void mtLED::audioSync(bool sync)
{
    if (sync)
    {
        writeRegister8(ISSI_BANK_FUNCTIONREG, ISSI_REG_AUDIOSYNC, 0x1);
    }
    else
    {
        writeRegister8(ISSI_BANK_FUNCTIONREG, ISSI_REG_AUDIOSYNC, 0x0);
    }
}
