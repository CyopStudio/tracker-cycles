#ifndef LIBS_NEWDISPLAY_GRIDEDITOR_H_
#define LIBS_NEWDISPLAY_GRIDEDITOR_H_

#include "displayControls.h"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------

struct strPadGridEditor
{
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
};

struct strPadNamesGridEditor
{
    uint8_t length;
    char** noteName;
    char (*finetuneName)[5];
    // Nietypowy zapis - wskaznik na tablicę 5-cio elementową typu char,
    // Czyli finetuneName + 1 (lub finetuneName[1]) przeskoczy o 5 adresow - więc mogę do niego
    // przypisac tablice dwuwymiarowa
};

class cGridEditorPopup : public cDisplayControl
{
  public:
    cGridEditorPopup(strControlProperties* properties = nullptr);
    virtual ~cGridEditorPopup();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    void fillpadMap();
    strPadGridEditor padMap[48];
    uint16_t textStyle;
    int16_t textFont;
    uint8_t keyboardShift = 0;

    strPadNamesGridEditor* padNames;

    char* noteCharPointer = nullptr;
};

#endif
