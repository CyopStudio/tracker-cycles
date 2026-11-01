#ifndef SOURCE_BUTTONSIMPL_H_
#define SOURCE_BUTTONSIMPL_H_
#include "ButtonsBaseImpl.h"

class ButtonsImpl : public ButtonsBaseImpl
{
  public:
    void init(int interruptPin, I2cBusController* i2cController, void (*resti2c)(void));

  private:
    static void handleInterrupt();
    static ButtonsImpl* instance;
    static const uint8_t convertToControlButtons[80];
};

#endif /* SOURCE_BUTTONSIMPL_H_ */
