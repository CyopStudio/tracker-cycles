#include "Keys.h"
#include "TimeControl.h"
#include "fsl_gpio.h"
#include "i2cBusController.h"
#include "pin_mux.h"
#include <map>

#define ENABLE_DEBUG_LOG 0

const uint16_t Keys::holdTimeStepDiff[Keys::holdTimeStepsCount] = { 0, 100, 190, 235, 255, 270 };

void (*buttonsInterruptHandler)(void) = nullptr;
void (*gridInterruptHandler)(void) = nullptr;

extern "C"
{
    void GPIO2_Combined_16_31_IRQHandler()
    {
        uint32_t pins_flags = GPIO_GetPinsInterruptFlags(GPIO2);

        if (pins_flags && BOARD_INITPINS_BUTTONS_INTERRUPT_GPIO_PIN_MASK)
        {
            if (buttonsInterruptHandler != nullptr)
            {
                buttonsInterruptHandler();
            }
        }

        GPIO_ClearPinsInterruptFlags(GPIO2, pins_flags);
    }

    void GPIO2_Combined_0_15_IRQHandler()
    {
        uint32_t pins_flags = GPIO_GetPinsInterruptFlags(GPIO2);

        if (pins_flags && BOARD_INITPINS_PADS_INTERRUPT_GPIO_PIN_MASK)
        {
            if (gridInterruptHandler != nullptr)
            {
                gridInterruptHandler();
            }
        }

        GPIO_ClearPinsInterruptFlags(GPIO2, pins_flags);
    }
}

void Keys::begin(uint8_t rows, uint16_t cols, uint8_t config, uint8_t* table)
{
    i2cAddress = 0x34;
    convertTable = table;
    this->rows = rows;
    this->cols = cols;
    this->config = config;
    setupKeys(rows, cols, config, false);
}

void Keys::setupKeys(uint8_t rows, uint16_t cols, uint8_t config, bool ignoreErrors)
{
    writeByte(KP_GPIO1, rows, ignoreErrors);
    writeByte(KP_GPIO2, (0xff & cols), ignoreErrors);
    writeByte(KP_GPIO3, (0x03 & (cols >> 8)), ignoreErrors);

    currentConfig = config | AI;
    writeByte(CFG, currentConfig, ignoreErrors);
    writeByte(INT_STAT, 0x0F, ignoreErrors); // clear interrupt
}

uint8_t Keys::getRowMask(int rowIdx)
{
    return 1 << rowIdx;
}

uint16_t Keys::getColumnMask(int columnIdx)
{
    return 1 << columnIdx;
}

void Keys::writeByte(uint8_t reg, uint8_t data, bool ignoreError)
{
    I2cBusController::OperationResult status =
      i2cBusController->sendData(i2cAddress, (uint32_t)reg, 1, &data, 1);

    if (!ignoreError && status == I2cBusController::OperationResult::kError)
    {
        refreshConfig();
    }
}

bool Keys::readByte(uint8_t reg, uint8_t* data, bool ignoreError)
{
    I2cBusController::OperationResult status =
      i2cBusController->receiveData(i2cAddress, (uint32_t)reg, 1, data, 1);

    if (status == I2cBusController::OperationResult::kError)
    {
        if (!ignoreError)
        {
            refreshConfig();
        }
        return false;
    }

    return true;
}

void Keys::enableInterrupt(void (*selfCheckFunction)(void), KeyType keyType)
{
    if (keyType == KeyType::kButtons)
    {
        buttonsInterruptHandler = selfCheckFunction;
    }
    else if (keyType == KeyType::kPads)
    {
        gridInterruptHandler = selfCheckFunction;
    }
}

void Keys::disableInterrupt(KeyType keyType)
{
    if (keyType == KeyType::kButtons)
    {
        buttonsInterruptHandler = nullptr;
    }
    else if (keyType == KeyType::kPads)
    {
        gridInterruptHandler = nullptr;
    }
}

uint8_t Keys::getKeyEvent()
{
    uint8_t event = 0;
    if (!readByte(KEY_EVENT_A, &event))
        return 0;
    return (event);
}

uint8_t Keys::getKeyEventCount()
{
    uint8_t count = 0;
    if (!readByte(KEY_LCK_EC, &count))
        return 0;
    return (count & 0x0F);
}

#if defined(ENABLE_DEBUG_LOG) && ENABLE_DEBUG_LOG == 1
uint32_t checkTimer = 0;
uint8_t read = 0;
uint8_t read2 = 0;
#endif

bool Keys::update()
{
#if defined(ENABLE_DEBUG_LOG) && ENABLE_DEBUG_LOG == 1
    if (checkTimer + 1000 < millis())
    {
        readByte(CFG, &read, true);
        readByte(INT_STAT, &read2, true);
        printf("cfg: %d, int: %d\n", read, read2);
        checkTimer = millis();
    }
#endif

    bool result = false;

    if (keyInt)
    {
        result = true;
        uint8_t eventCount = getKeyEventCount();
        if (eventCount > 0)
        {
            for (uint8_t i = 0; i < eventCount; i++)
            {
                uint8_t key = getKeyEvent();
                if (key > 0)
                {
                    uint8_t keyNumber = key & 0x7F;
                    uint8_t keyState = key & 0x80;

                    uint8_t button = convertTable[keyNumber - 1];

                    if (keyState)
                    {
                        holdTimeMs[button] = millis();
                        holdFactor[button] = 0;
                        onPush(button);
                        buttonPush[button] = 1;
                    }
                    else
                    {
                        onRelease(button);
                        buttonPush[button] = 0;
                    }
                }
            }
        }
        keyInt = 0;
        writeByte(INT_STAT, 0x0F); // clear interrupt
    }

    for (uint8_t i = 0; i < BUTTON_MAX; i++)
    {
        if (buttonPush[i] &&
            ((millis() - holdTimeMs[i]) > (uint32_t)(holdTime - holdTimeStepDiff[holdFactor[i]])))
        {
            if (holdFactor[i] < holdTimeStepsCount - 1)
            {
                holdFactor[i]++;
            }

            holdTimeMs[i] = millis();
            onHold(i);
        }
    }

    return result;
}

void Keys::setOnPush(void (*funct)(uint8_t))
{
    onPush = funct;
}
void Keys::setOnRelease(void (*funct)(uint8_t))
{
    onRelease = funct;
}
void Keys::setOnHold(void (*funct)(uint8_t))
{
    onHold = funct;
}

uint8_t Keys::isButtonPressed(uint8_t n)
{
    return buttonPush[n];
}

void Keys::refreshConfig()
{
    i2cBusController->unblockBus();
    i2cBusController->init();

    uint8_t cfg = 0;
    readByte(CFG, &cfg, true);
    //	cppserial_print_val(cfg);
    //	cppserial_print(" - ");
    //	cppserial_print_val(currentConfig);

    if (cfg != currentConfig)
    {
        for (int button = 0; button < BUTTON_MAX; button++)
        {
            if (buttonPush[button])
            {
                onRelease(button);
                buttonPush[button] = 0;
            }
        }

        setupKeys(rows, cols, config, true);

        //		 cppserial_print(" setup keys ");
    }
    //
    //	cppserial_println("");
}
