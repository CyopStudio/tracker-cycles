#ifndef LIBS_NEWDISPLAY_BATTERYCONTROL_H_
#define LIBS_NEWDISPLAY_BATTERYCONTROL_H_

#include "displayControls.h"

//--------------------------------------------------------------------
class cBattery : public cDisplayControl
{
  public:
    cBattery(strControlProperties* properties = nullptr);
    virtual ~cBattery();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

  protected:
    uint16_t textStyle;
};

#endif
