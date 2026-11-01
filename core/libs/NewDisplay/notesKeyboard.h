#ifndef LIBS_NEWDISPLAY_NOTESKEYBOARD_H_
#define LIBS_NEWDISPLAY_NOTESKEYBOARD_H_

#include "displayControls.h"

struct strcNotesKeyboardData
{
    char** labels;
    bool editButtonsVisible = false;
};

class cNotesKeyboard : public cDisplayControl
{
  public:
    cNotesKeyboard(strControlProperties* properties = nullptr);
    virtual ~cNotesKeyboard();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

  private:
    uint16_t textStyle;
    int16_t textFont;
    strcNotesKeyboardData* data;
};

#endif
