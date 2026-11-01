#ifndef LIBS_NEWDISPLAY_TEXTBOXCONTROL_H_
#define LIBS_NEWDISPLAY_TEXTBOXCONTROL_H_

#include "displayControls.h"

struct strTextBoxData
{
    char* text = { nullptr };
};

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cTextBox : public cDisplayControl
{
  public:
    cTextBox(strControlProperties* properties = nullptr);
    virtual ~cTextBox();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    int16_t getNextLineLength();

  protected:
    strTextBoxData* data;
    uint16_t textStyle;
    uint32_t textLength;
    const char* position;
};

#endif
