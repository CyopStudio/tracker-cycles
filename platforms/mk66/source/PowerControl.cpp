#include "PowerControl.h"
#include "PlatformImpl.h"
#include "config.h"
#include "core_pins.h"
#include "pinout.h"

#include <stdint.h>

bool powerButtonLastState;

void checkPowerState(PlatformImpl* platformImpl)
{
    TactSwitchInit();

    if (START_STATE_SAVE)
    {
        if (platformImpl->readStartState() == 0)
        {
            platformImpl->setPowerMode(false);
            while (1)
            {
                TactSwitchRead(platformImpl);
                asm("nop");
            }
        }
        else
        {
            platformImpl->saveStartState(0);
        }
    }
}

void TactSwitchInit()
{
    pinMode(TrackerPinout::kPowerButton, INPUT);
#ifndef TRACKER_MINI
    powerButtonLastState = digitalRead(TrackerPinout::kPowerButton);
#else
    pinMode(TrackerPinout::kPowerReset, INPUT);
    pinMode(TrackerPinout::kPowerSet, INPUT);
    powerButtonLastState = !digitalRead(TrackerPinout::kPowerButton);
#endif
}

bool getTactSwitchState()
{
#ifndef TRACKER_MINI
    return digitalRead(TrackerPinout::kPowerButton);
#else
    return !digitalRead(TrackerPinout::kPowerButton);
#endif
}

void TactSwitchRead(PlatformImpl* platformImpl)
{
    bool state = getTactSwitchState();

    if (state == powerButtonLastState)
    {
        return;
    }

    if (!platformImpl->getPowerMode())
    {
        if (state == false)
        {
            platformImpl->setPowerMode(true);
        }
        powerButtonLastState = state;
        return;
    }

    if (state == false)
    {
        platformImpl->getPowerButtonCallback()(1);
        powerButtonLastState = false;
    }
    else if (state == true)
    {
        powerButtonLastState = true;
        platformImpl->getPowerButtonCallback()(0);
    }
}
