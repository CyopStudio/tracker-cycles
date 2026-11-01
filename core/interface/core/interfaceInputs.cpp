#include "core/interface.h"
#include "core/interfacePopups.h"

#include "Platform.h"

#ifdef TRACKER_MINI
bool fnButtonShiftPressed[4] = { false, false, false, false };

// Tracks currently active directional/middle button; 0 means none locked.
static uint8_t activeNavButton = 0;

static inline bool isNavButton(uint8_t b)
{
    return b == interfaceButtonMiniMiddle || b == interfaceButtonUp || b == interfaceButtonDown ||
           b == interfaceButtonLeft || b == interfaceButtonRight;
}
#endif

//=======================================================================
void Interface::potChange(uint8_t n, int16_t value)
{
    ip->processPotsInput(n, value);
}

//=======================================================================
void Interface::buttonChange(uint8_t n, uint8_t value)
{
#ifdef TRACKER_MINI
    if (isNavButton(n))
    {
        // If another nav button is locked, ignore this event until it releases.
        if (activeNavButton != 0 && n != activeNavButton)
        {
            return;
        }

        if (value == buttonPress)
        {
            activeNavButton = n;
        }
        else if (value == buttonRelease && n == activeNavButton)
        {
            activeNavButton = 0;
        }
    }
#endif

    if (value == buttonHold &&
        ((n >= 25 && n <= 27) || (n >= 30 && n <= 32) || n == interfaceButtonMiniMiddle))
    {
        ip->processButtonsInput(n, buttonPress);
    }
    else
    {
        ip->processButtonsInput(n, value);
    }
}

//=======================================================================
bool Interface::powerButtonChange(uint8_t state)
{
    if (ip->isPowerButtonBlocked())
    {
        return false;
    }

    if (state == 1 && getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift))
    {
        display.scheduleScreenShot(1);

        return true;
    }

    return ip->processPowerButton(state);
}

//=======================================================================
void Interface::padPressed(uint8_t n, int8_t, int8_t, uint8_t velo)
{
    ip->processPadsInput(n, 1, velo);
}

//=======================================================================
void Interface::padReleased(uint8_t n)
{
    ip->processPadsInput(n, 0, 0);
}

//=======================================================================
void Interface::padHold(uint8_t n)
{
    ip->processPadsInput(n, 2, 0);
}

//=======================================================================
#ifdef TRACKER_MINI
static void functMiniFn1(uint8_t state)
{
    if (state == buttonPress)
    {
        fnButtonShiftPressed[0] =
          mtInterface.getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift);
    }

    if (fnButtonShiftPressed[0])
    {
        mtInterface.ip->processButtonsInput(
          InterfaceMenuPopup::modules[mtConfig.interface.functionKeysBinding[4]].button, state);
    }
    else
    {
        mtInterface.ip->processButtonsInput(
          InterfaceMenuPopup::modules[mtConfig.interface.functionKeysBinding[0]].button, state);
    }

    if (state == buttonRelease)
    {
        fnButtonShiftPressed[0] = false;
    }
}

static void functMiniFn2(uint8_t state)
{
    if (state == buttonPress)
    {
        fnButtonShiftPressed[1] =
          mtInterface.getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift);
    }

    if (fnButtonShiftPressed[1])
    {
        mtInterface.ip->processButtonsInput(
          InterfaceMenuPopup::modules[mtConfig.interface.functionKeysBinding[5]].button, state);
    }
    else
    {
        mtInterface.ip->processButtonsInput(
          InterfaceMenuPopup::modules[mtConfig.interface.functionKeysBinding[1]].button, state);
    }

    if (state == buttonRelease)
    {
        fnButtonShiftPressed[1] = false;
    }
}

static void functMiniFn3(uint8_t state)
{
    if (state == buttonPress)
    {
        fnButtonShiftPressed[2] =
          mtInterface.getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift);
    }

    if (fnButtonShiftPressed[2])
    {
        mtInterface.ip->processButtonsInput(
          InterfaceMenuPopup::modules[mtConfig.interface.functionKeysBinding[6]].button, state);
    }
    else
    {
        mtInterface.ip->processButtonsInput(
          InterfaceMenuPopup::modules[mtConfig.interface.functionKeysBinding[2]].button, state);
    }

    if (state == buttonRelease)
    {
        fnButtonShiftPressed[2] = false;
    }
}

static void functMiniFn4(uint8_t state)
{
    if (state == buttonPress)
    {
        fnButtonShiftPressed[3] =
          mtInterface.getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift);
    }

    if (fnButtonShiftPressed[3])
    {
        mtInterface.ip->processButtonsInput(
          InterfaceMenuPopup::modules[mtConfig.interface.functionKeysBinding[7]].button, state);
    }
    else
    {
        mtInterface.ip->processButtonsInput(
          InterfaceMenuPopup::modules[mtConfig.interface.functionKeysBinding[3]].button, state);
    }

    if (state == buttonRelease)
    {
        fnButtonShiftPressed[3] = false;
    }
}

static void functMiniPlus10(uint8_t state)
{
    if (mtInterface.getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift))
    {
        if (state == buttonPress || state == buttonHold)
        {
            mtInterface.changeMasterVolume(10);
            return;
        }
        return;
    }

    if (state == buttonPress || state == buttonHold)
    {
        mtInterface.ip->processPotsInput(interfacePot0, 10, state);
    }
    else if (state == buttonRelease)
    {
        mtInterface.ip->processPotsInput(interfacePot0, 0, state);
    }
}

static void functMiniPlus1(uint8_t state)
{
    if (mtInterface.getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift))
    {
        if (state == buttonPress || state == buttonHold)
        {
            mtInterface.changeMasterVolume(1);
            return;
        }
        return;
    }

    if (state == buttonPress || state == buttonHold)
    {
        mtInterface.ip->processPotsInput(interfacePot0, 1, state);
    }
    else if (state == buttonRelease)
    {
        mtInterface.ip->processPotsInput(interfacePot0, 0, state);
    }
}

static void functMiniMinus1(uint8_t state)
{
    if (mtInterface.getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift))
    {
        if (state == buttonPress || state == buttonHold)
        {
            mtInterface.changeMasterVolume(-1);
            return;
        }
        return;
    }

    if (state == buttonPress || state == buttonHold)
    {
        mtInterface.ip->processPotsInput(interfacePot0, -1, state);
    }
    else if (state == buttonRelease)
    {
        mtInterface.ip->processPotsInput(interfacePot0, 0, state);
    }
}

static void functMiniMinus10(uint8_t state)
{
    if (mtInterface.getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift))
    {
        if (state == buttonPress || state == buttonHold)
        {
            mtInterface.changeMasterVolume(-10);
            return;
        }
        return;
    }

    if (state == buttonPress || state == buttonHold)
    {
        mtInterface.ip->processPotsInput(interfacePot0, -10, state);
    }
    else if (state == buttonRelease)
    {
        mtInterface.ip->processPotsInput(interfacePot0, 0, state);
    }
}
#endif

void Interface::initSpecialMiniTrackerButtons()
{
#ifdef TRACKER_MINI
    ip->setButtonObj(interfaceButtonMiniFn1, functMiniFn1);
    ip->setButtonObj(interfaceButtonMiniFn2, functMiniFn2);
    ip->setButtonObj(interfaceButtonMiniFn3, functMiniFn3);
    ip->setButtonObj(interfaceButtonMiniFn4, functMiniFn4);

    ip->setButtonObj(interfaceButtonMiniPlus10, functMiniPlus10);
    ip->setButtonObj(interfaceButtonMiniPlus1, functMiniPlus1);
    ip->setButtonObj(interfaceButtonMiniMinus1, functMiniMinus1);
    ip->setButtonObj(interfaceButtonMiniMinus10, functMiniMinus10);
#endif
}
