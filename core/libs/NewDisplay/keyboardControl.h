#ifndef LIBS_NEWDISPLAY_KEYBOARD_H_
#define LIBS_NEWDISPLAY_KEYBOARD_H_

#include "displayControls.h"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------

struct strKey
{
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
};

constexpr uint8_t KEYBOARD_SIZE = 42;

constexpr char charOnKeyboardSmall[KEYBOARD_SIZE] = { '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                                      '0', 0,   'q', 'w', 'e', 'r', 't', 'y', 'u',
                                                      'i', 'o', 'p', '-', '+', 'a', 's', 'd', 'f',
                                                      'g', 'h', 'j', 'k', 'l', '@', 1,   'z', 'x',
                                                      'c', 'v', 'b', 'n', 'm', 2 };

constexpr char charOnKeyboardBig[KEYBOARD_SIZE] = { '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                                    '0', 0,   'Q', 'W', 'E', 'R', 'T', 'Y', 'U',
                                                    'I', 'O', 'P', '-', '+', 'A', 'S', 'D', 'F',
                                                    'G', 'H', 'J', 'K', 'L', '@', 1,   'Z', 'X',
                                                    'C', 'V', 'B', 'N', 'M', 2 };

class cKeyboard : public cDisplayControl
{
  public:
    cKeyboard(strControlProperties* properties = nullptr);
    virtual ~cKeyboard();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    void fillKeyboard();
    strKey keyboard[KEYBOARD_SIZE];
    uint16_t textStyle;
    int16_t textFont;
    uint8_t keyboardShift = 0;
};

#endif
