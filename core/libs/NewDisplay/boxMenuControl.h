#ifndef LIBS_NEWDISPLAY_BOXMENUCONTROL_H_
#define LIBS_NEWDISPLAY_BOXMENUCONTROL_H_

#include "displayControls.h"

struct strBoxMenuData
{
    uint8_t columnsCount;
    uint8_t rowsCount;

    char** textList;

    struct Modes
    {
        uint8_t count;
        uint8_t current;
    };

    Modes** modes;
};

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cBoxMenu : public cDisplayControl
{
  public:
    cBoxMenu(strControlProperties* properties = nullptr);
    virtual ~cBoxMenu();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

  protected:
    strBoxMenuData* data;

    uint16_t textStyle;
};

#endif
