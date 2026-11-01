#ifndef SOURCE_PADSIMPL_H_
#define SOURCE_PADSIMPL_H_
#include "ButtonsBaseImpl.h"

class PadsImpl : public ButtonsBaseImpl
{
  public:
    void init();

  private:
    static void handleInterrupt();

    static PadsImpl* instance;
    static const uint8_t convertToGridKey4x12[80];
};

#endif /* SOURCE_PADSIMPL_H_ */
