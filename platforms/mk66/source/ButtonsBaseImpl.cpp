#include <ButtonsBaseImpl.h>

void ButtonsBaseImpl::setHoldTime(int timeMs)
{
    buttonsDriver.setHoldTime(timeMs);
}

bool ButtonsBaseImpl::isButtonPressed(int buttonIdx)
{
    return buttonsDriver.isButtonPressed(buttonIdx);
}

void ButtonsBaseImpl::setActionCallback(ActionType action, void (*callback)(uint8_t))
{
    switch (action)
    {
        case ActionType::kPress:
            buttonsDriver.setOnPush(callback);
            break;
        case ActionType::kRelease:
            buttonsDriver.setOnRelease(callback);
            break;
        case ActionType::kHold:
            buttonsDriver.setOnHold(callback);
            break;
    }
}

void ButtonsBaseImpl::disableInterrupts()
{
    buttonsDriver.disableInterrupt();
}

bool ButtonsBaseImpl::update()
{
    return buttonsDriver.update();
}
