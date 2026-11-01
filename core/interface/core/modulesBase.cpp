#include "core/modulesBase.h"
#include "core/interface.h"
#include <string.h>

InputProcessor* ModuleBase::ip;
ModuleBase::EventFunct ModuleBase::eventFunct;
Buttons* ModuleBase::buttons;
TitleBar* ModuleBase::titleBar;
Platform* ModuleBase::platform;
Synth* ModuleBase::synth;
mtKeyboardManager ModuleBase::keyboardManager;

static const uint16_t encoderBoost[50] = { 0,   1,   4,   12,  20,  32,  64,  100, 100, 100,
                                           100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
                                           100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
                                           100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
                                           200, 200, 200, 200, 200, 200, 200, 200, 200, 200 };

void InputProcessor::clearAll()
{
    clearGlobalPads();
    clearAllPots();
    clearAllButtons();
}

void InputProcessor::clearAllPots()
{
    if (potsCleared)
    {
        return;
    }

    memset((uint8_t*)pots, 0, sizeof(strPotObject) * potsCount);

    potsCleared = 1;
}

void InputProcessor::clearGlobalPads()
{
    padsGlobalFunct = nullptr;
    padsCleared = 1;
}

void InputProcessor::clearAllButtons()
{
    if (buttonsCleared)
    {
        return;
    }

#ifndef TRACKER_MINI
    const uint8_t buttonsToClear = buttonsCount;
#else
    const uint8_t buttonsToClear = interfaceButton34 + 1;
#endif

    memset((uint8_t*)buttons, 0, sizeof(strButtonObject) * buttonsToClear);
    memset((uint8_t*)(&buttons[interfaceButton43]), 0, sizeof(strButtonObject));

    buttonsCleared = 1;
}

void InputProcessor::clearButtonsRange(uint8_t from, uint8_t to)
{
    if (buttonsCleared)
    {
        return;
    }

    if (from >= buttonsCount || to >= buttonsCount || from > to)
    {
        return;
    }

    memset((uint8_t*)(buttons + from), 0, (sizeof(strButtonObject) * ((to + 1) - from)));
}

void InputProcessor::clearButton(uint8_t button)
{
    if (buttonsCleared)
    {
        return;
    }

    if (button >= buttonsCount)
    {
        return;
    }

    memset((uint8_t*)(buttons + button), 0, sizeof(strButtonObject));
}

//==================================================================================================================
void InputProcessor::blockAllInputs()
{
    memset((uint8_t*)blockButtons, 1, buttonsCount);
    memset((uint8_t*)blockPots, 1, potsCount);
    memset((uint8_t*)blockPads, 1, padsCount);
    blockPowerButton = 1;
}

void InputProcessor::blockAllInputsExcept(uint8_t btn)
{
    blockAllInputs();
    if (btn < buttonsCount)
    {
        blockButtons[btn] = 0;
    }
}

void InputProcessor::blockAllInputsExcept(uint8_t btn1, uint8_t btn2)
{
    blockAllInputs();
    if (btn1 < buttonsCount)
    {
        blockButtons[btn1] = 0;
    }
    if (btn2 < buttonsCount)
    {
        blockButtons[btn2] = 0;
    }
}

void InputProcessor::blockAllInputsExceptScreenButtons()
{
    blockAllInputs();
    memset((uint8_t*)blockButtons, 0, 8); // block first 8 (under screen)
}

void InputProcessor::unblockAllInputs()
{
    memset((uint8_t*)blockButtons, 0, buttonsCount);
    memset((uint8_t*)blockPots, 0, potsCount);
    memset((uint8_t*)blockPads, 0, padsCount);
    blockPowerButton = 0;
}

//==================================================================================================================
void InputProcessor::setPotObj(uint8_t objectID, void (*funct)(int16_t), bool shiftBoost)
{
    pots[objectID].funct1 = funct;
    pots[objectID].mode = 1;
    pots[objectID].boost = shiftBoost;
    potsCleared = 0;
}

void InputProcessor::setPotObj(uint8_t objectID, void (*funct)(int16_t, uint8_t), bool shiftBoost)
{
    pots[objectID].funct2 = funct;
    pots[objectID].mode = 2;
    pots[objectID].boost = shiftBoost;
    potsCleared = 0;
}

//==================================================================================================================
// przypisuje funkcje na okreslony stan przycisku bez argumentow
void InputProcessor::setButtonObj(uint8_t objectID, uint8_t state, void (*funct)(void))
{
    if (objectID >= buttonsCount)
    {
        return;
    }

    buttons[objectID].funct1 = funct;
    buttons[objectID].state = state;

    buttons[objectID].mode = 1;

    buttonsCleared = 0;
}

// przypisuje funkcje na okreslony stan przycisku z argumentem w postaci id przycisku
void InputProcessor::setButtonObj(uint8_t objectID, uint8_t state, void (*funct)(uint8_t))
{
    if (objectID >= buttonsCount)
    {
        return;
    }

    buttons[objectID].funct2 = funct;
    buttons[objectID].state = state;

    buttons[objectID].mode = 2;

    buttonsCleared = 0;
}

// przypisuje funkcje z argumentem w postaci stanu przycisku
void InputProcessor::setButtonObj(uint8_t objectID, void (*funct)(uint8_t))
{
    if (objectID >= buttonsCount)
    {
        return;
    }

    buttons[objectID].funct2 = funct;
    buttons[objectID].state = 255;

    buttons[objectID].mode = 3;

    buttonsCleared = 0;
}

// przypisuje funkcje z argumentami w postaci id i stanu przycisku
void InputProcessor::setButtonObj(uint8_t objectID, void (*funct)(uint8_t, uint8_t))
{
    if (objectID >= buttonsCount)
    {
        return;
    }

    buttons[objectID].funct3 = funct;
    buttons[objectID].state = 255;

    buttons[objectID].mode = 4;

    buttonsCleared = 0;
}

//==================================================================================================================
void InputProcessor::setPowerButtonObj(void (*funct)(uint8_t))
{
    powrButtonFunct = funct;
}

//==================================================================================================================
void InputProcessor::setPadsGlobal(void (*funct)(uint8_t, uint8_t, int16_t))
{
    padsGlobalFunct = funct;
    padsCleared = 0;
}

//==================================================================================================================
void InputProcessor::processButtonsInput(uint8_t button, uint8_t state)
{
    if (blockButtons[button])
    {
        return;
    }

    switch (buttons[button].mode)
    {
        case 1:
            if (buttons[button].funct1 != nullptr && state == buttons[button].state)
            {
                buttons[button].funct1();
            }
            break;
        case 2:
            if (buttons[button].funct2 != nullptr && state == buttons[button].state)
            {
                buttons[button].funct2(button);
            }
            break;
        case 3:
            if (buttons[button].funct2 != nullptr)
            {
                buttons[button].funct2(state);
            }
            break;
        case 4:
            if (buttons[button].funct3 != nullptr)
            {
                buttons[button].funct3(button, state);
            }
            break;
        default: break;
    }
}

void InputProcessor::processPotsInput(uint8_t pot, int16_t value, uint8_t state)
{
    if (blockPots[pot])
    {
        return;
    }

#ifndef TRACKER_MINI
    if (pots[pot].boost &&
        mtInterface.getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift))
    {
        bool upDir = value >= 0;
        int16_t boostVal = upDir ? value : -value;
        boostVal = boostVal > 49 ? 49 : boostVal;

        value = upDir ? encoderBoost[boostVal] : -encoderBoost[boostVal];
    }
#endif

    if (pots[pot].mode == 1 && state != buttonRelease)
    {
        if (pots[pot].funct1 != nullptr)
        {
            pots[pot].funct1(value);
        }
    }
    else if (pots[pot].mode == 2)
    {
        if (pots[pot].funct2 != nullptr)
        {
            pots[pot].funct2(value, state);
        }
    }
}

void InputProcessor::processPadsInput(uint8_t pad, uint8_t state, int16_t velo)
{
    if (blockPads[pad])
    {
        return;
    }

    if (padsGlobalFunct != nullptr)
    {
        padsGlobalFunct(pad, state, velo);
    }
}

bool InputProcessor::processPowerButton(uint8_t state)
{
    if (powrButtonFunct != nullptr)
    {
        powrButtonFunct(state);
        return true;
    }

    return false;
}

bool Buttons::isButtonPressed(int buttonIdx)
{
    bool pressed = switchGroup->isButtonPressed(buttonIdx);

    if (pressed && buttonIdx < interfaceButton8 && buttonIdx > interfaceButton22)
    {
        return pressed;
    }

#ifdef TRACKER_MINI

    bool shiftPressed = switchGroup->isButtonPressed(interfaceButtonShift);

    for (int fnbtn = 0; fnbtn < 8; fnbtn++)
    {
        if (InterfaceMenuPopup::modules[mtConfig.interface.functionKeysBinding[fnbtn]].button ==
            buttonIdx)
        {
            if (fnbtn < 4 && switchGroup->isButtonPressed(interfaceButtonMiniFn1 + fnbtn))
            {
                return true;
            }
            else if (fnbtn >= 4 && shiftPressed &&
                     switchGroup->isButtonPressed(interfaceButtonMiniFn1 + fnbtn - 4))
            {
                return true;
            }
        }
    }
#endif

    return pressed;
}
