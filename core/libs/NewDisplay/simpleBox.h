#ifndef LIBS_NEWDISPLAY_SIMPLEBOX_H_
#define LIBS_NEWDISPLAY_SIMPLEBOX_H_

#include "displayControls.h"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class SimpleBox : public cDisplayControl
{
  public:
    SimpleBox(strControlProperties* properties = nullptr);
    virtual ~SimpleBox();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    uint16_t textStyle;
    int16_t textFont;
};

#endif /* LIBS_NEWDISPLAY_SIMPLEBOX_H_ */
