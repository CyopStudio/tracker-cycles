#include "ButtonsImpl.h"
#include "LedsImpl.h"
#include "PadsImpl.h"
#include "Platform.h"
#include "core_pins.h"
#include "i2cBusControllerNXP.h"
#include "stdint.h"

static uint32_t i2cRefreshTimerUs;
static uint32_t i2cLedsRefreshTimerMs;
static uint32_t i2c2TimoutTimerUs;
static uint32_t i2c1TimoutTimerUs;

static uint8_t i2c_switch = 0;
static constexpr uint16_t I2C_TIMEOUT_US = 10000;

void restI2C2();
void restI2C0();

void updateI2C(Platform* platform)
{
    if ((micros() - i2cRefreshTimerUs) > 500)
    {
        i2cRefreshTimerUs = micros();

#ifndef TRACKER_MINI
        if (i2cBusControllerNXP2.isBusIdle())
        {
            if (i2c_switch == 0)
            {
                PadsImpl* pads = (PadsImpl*)platform->getPads();

                if (pads->update())
                {
                    i2c_switch++;
                }
            }
            if (i2c_switch == 1)
            {
                if ((millis() - i2cLedsRefreshTimerMs) > 30)
                {
                    LedsImpl* leds = (LedsImpl*)platform->getLeds();
                    if (!leds->update())
                    {
                        i2c_switch++;
                    }
                    i2cLedsRefreshTimerMs = millis();
                }
                else
                {
                    i2c_switch++;
                }
            }

            if (i2c_switch > 1)
            {
                i2c_switch = 0;
            }
            else
            {
                i2c_switch++;
            }

            i2c2TimoutTimerUs = micros();
        }
        else
        {
            if ((micros() - i2c2TimoutTimerUs) > I2C_TIMEOUT_US)
            {
                i2c2TimoutTimerUs = micros();
                restI2C2();
            }
        }
#endif
        if (i2cBusControllerNXP0.isBusIdle())
        {
            ButtonsImpl* buttons = (ButtonsImpl*)platform->getButtons();
            buttons->update();
            i2c1TimoutTimerUs = micros();
        }
        else
        {
            if ((micros() - i2c1TimoutTimerUs) > I2C_TIMEOUT_US)
            {
                i2c1TimoutTimerUs = micros();
                restI2C0();
            }
        }
    }
}

void restI2C2()
{
    i2cBusControllerNXP2.unblockBus();
    i2cBusControllerNXP2.init();
}

void restI2C0()
{
    i2cBusControllerNXP0.unblockBus();
    i2cBusControllerNXP0.init();
}
