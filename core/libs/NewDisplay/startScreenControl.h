#ifndef LIBS_STARTSCREEN_CONTROL_H_
#define LIBS_STARTSCREEN_CONTROL_H_

#include "displayControls.h"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cStartScreen : public cDisplayControl
{
  public:
    cStartScreen(strControlProperties* properties = nullptr);
    virtual ~cStartScreen();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    char* data;

    uint8_t dots;
    uint16_t textStyle;
    int16_t textFont;
};

#endif
