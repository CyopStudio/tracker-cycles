#ifndef LIBS_NEWDISPLAY_NOTEPOPOUT_H_
#define LIBS_NEWDISPLAY_NOTEPOPOUT_H_

#include "displayControls.h"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------

struct strPad
{
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
};

struct strPadNames
{
    uint8_t length;
    char** name;
};

constexpr uint8_t PADMAP_SIZE = 48;

class cNotePopout : public cDisplayControl
{
  public:
    cNotePopout(strControlProperties* properties = nullptr);
    virtual ~cNotePopout();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    void fillpadMap();
    strPad padMap[PADMAP_SIZE];
    uint16_t textStyle;
    int16_t textFont;
    uint8_t keyboardShift = 0;

    strPadNames* padNames;

    char* noteCharPointer = nullptr;
};

#endif
