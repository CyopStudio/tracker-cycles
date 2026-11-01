#include "i2cBusControllerNXP.h"
#include "TimeControl.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "fsl_lpi2c.h"
#include "math.h"
#include "peripherals.h"

void I2cBusControllerImplBase::setTimeout(uint32_t timeoutUs)
{
    this->timeoutUs = timeoutUs;

    // timeout is set by project definition I2C_RETRY_TIMES not to modify i2c library
}

bool I2cBusControllerImplBase::isBusIdle()
{
    assert(baseAddr != nullptr);
    return !(LPI2C_MasterGetStatusFlags((LPI2C_Type*)baseAddr) &
             _lpi2c_master_flags::kLPI2C_MasterBusBusyFlag);
}

I2cBusController::OperationResult I2cBusControllerImplBase::sendData(uint8_t slaveAddress,
                                                                     uint32_t reg,
                                                                     uint32_t regSize,
                                                                     void* data,
                                                                     uint32_t dataSize)
{
    lpi2c_master_transfer_t masterXfer;
    memset(&masterXfer, 0, sizeof(masterXfer));

    masterXfer.slaveAddress = slaveAddress;
    masterXfer.direction = kLPI2C_Write;
    masterXfer.subaddress = reg;
    masterXfer.subaddressSize = regSize;
    masterXfer.data = data;
    masterXfer.dataSize = dataSize;
    masterXfer.flags = kLPI2C_TransferDefaultFlag;

    assert(baseAddr != nullptr);

    OperationResult result =
      LPI2C_MasterTransferBlocking((LPI2C_Type*)baseAddr, &masterXfer) == kStatus_Success
        ? OperationResult::kOK
        : OperationResult::kError;

    return result;
}

I2cBusController::OperationResult I2cBusControllerImplBase::receiveData(uint8_t slaveAddress,
                                                                        uint32_t reg,
                                                                        uint32_t regSize,
                                                                        void* data,
                                                                        uint32_t dataSize)
{
    lpi2c_master_transfer_t masterXfer;
    memset(&masterXfer, 0, sizeof(masterXfer));
    masterXfer.slaveAddress = slaveAddress;
    masterXfer.direction = kLPI2C_Read;
    masterXfer.subaddress = reg;
    masterXfer.subaddressSize = regSize;
    masterXfer.data = data;
    masterXfer.dataSize = dataSize;
    masterXfer.flags = kLPI2C_TransferDefaultFlag;

    assert(baseAddr != nullptr);

    return LPI2C_MasterTransferBlocking((LPI2C_Type*)baseAddr, &masterXfer) == kStatus_Success
             ? OperationResult::kOK
             : OperationResult::kError;
}

void I2cBusControllerImpl4::init()
{
    baseAddr = (void*)LPI2C4;
    IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B0_12_LPI2C4_SCL, 1U);
    IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B0_13_LPI2C4_SDA, 1U);

    LPI2C_MasterInit(LPI2C4_PERIPHERAL, &LPI2C4_masterConfig, LPI2C4_CLOCK_FREQ);

    LPI2C4_PERIPHERAL->MCCR0 = LPI2C_MCCR0_CLKHI(0x02) | LPI2C_MCCR0_CLKLO(0x0B) |
                               LPI2C_MCCR0_SETHOLD(0x04) | LPI2C_MCCR0_DATAVD(0x02);
}

void I2cBusControllerImpl4::unblockBus()
{
    int kForcedEdgesCount = 16;

    gpio_pin_config_t gpio_config;
    gpio_config.direction = kGPIO_DigitalOutput;
    gpio_config.outputLogic = 0;

    IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B0_12_GPIO1_IO12, 0U);

    GPIO_PinInit(GPIO1, 12, &gpio_config);

    int kHalfPeriodDelayUs = ceilf(((1.0f / LPI2C4_masterConfig.baudRate_Hz) * 1000000.0f));

    for (int bit = 0; bit < kForcedEdgesCount; bit++)
    {
        timeControl.delayMicroseconds(kHalfPeriodDelayUs);
        GPIO_PinWrite(GPIO1, 12, bit & 1);
    }
}

void I2cBusControllerImpl2::init()
{
    baseAddr = (void*)LPI2C2;
    IOMUXC_SetPinMux(IOMUXC_GPIO_B0_04_LPI2C2_SCL, 1U);
    IOMUXC_SetPinMux(IOMUXC_GPIO_B0_05_LPI2C2_SDA, 1U);

    LPI2C_MasterInit(LPI2C2_PERIPHERAL, &LPI2C2_masterConfig, LPI2C2_CLOCK_FREQ);

    // Values from reference manual IMXRT1050 - LPI2C Example Timing Configurations - for 8 MHz
    // input clock and 400 kbps baund rate. Values calculated by lpi2c library are different and it
    // doesn't work.

    LPI2C2_PERIPHERAL->MCCR0 = LPI2C_MCCR0_CLKHI(0x02) | LPI2C_MCCR0_CLKLO(0x0B) |
                               LPI2C_MCCR0_SETHOLD(0x04) | LPI2C_MCCR0_DATAVD(0x02);
}

void I2cBusControllerImpl2::unblockBus()
{
    int kForcedEdgesCount = 16;

    gpio_pin_config_t gpio_config;
    gpio_config.direction = kGPIO_DigitalOutput;
    gpio_config.outputLogic = 0;

    IOMUXC_SetPinMux(IOMUXC_GPIO_B0_04_GPIO2_IO04, 0U);

    GPIO_PinInit(GPIO2, 4, &gpio_config);

    int kHalfPeriodDelayUs = ceilf(((1.0f / LPI2C2_masterConfig.baudRate_Hz) * 1000000.0f));

    for (int bit = 0; bit < kForcedEdgesCount; bit++)
    {
        timeControl.delayMicroseconds(kHalfPeriodDelayUs);
        GPIO_PinWrite(GPIO2, 4, bit & 1);
    }
}

I2cBusControllerImpl4 i2cBusControllerNXP4;
I2cBusControllerImpl2 i2cBusControllerNXP2;
