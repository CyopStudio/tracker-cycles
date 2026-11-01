#ifndef LIBS_NEWDISPLAY_SIMPLETEXTCONTROL_H_
#define LIBS_NEWDISPLAY_SIMPLETEXTCONTROL_H_

#include "displayControls.h"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cSimpleText : public cDisplayControl
{
  public:
    cSimpleText(strControlProperties* properties = nullptr);
    virtual ~cSimpleText();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    uint16_t textStyle;
    int16_t textFont;
};

#endif /* LIBS_NEWDISPLAY_SIMPLETEXTCONTROL_H_ */
