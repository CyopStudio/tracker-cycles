#ifndef LIBS_BAR_CONTROL_H_
#define LIBS_BAR_CONTROL_H_

#include "displayControls.h"

struct strBarData
{
    uint8_t changingColor = 0;
    int value = 0xFFFFFF;
};

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cBar : public cDisplayControl
{
  public:
    cBar(strControlProperties* properties = nullptr);
    virtual ~cBar();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    strBarData* data;

    uint16_t textStyle;
    int16_t textFont;
};

#endif
