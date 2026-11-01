#include "FT812.h"

#include "notesKeyboard.h"

#include "display.h"
#include "displayControls.h"

#include <string.h>

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
cNotesKeyboard::cNotesKeyboard(strControlProperties* properties)
  : cDisplayControl(controlIdNotesKeyboardLabel, 1)
{
    colorsCount = 3;
    colors = display.getTheme().bottomButtonsBgColors;

    if (properties == nullptr)
    {
        posX = 0;
        posY = 0;
        text = nullptr;
        data = nullptr;
        width = 0;
        height = 0;
        style = 0;
        value = 0;
        return;
    }

    colors = (properties->colors == nullptr) ? colors : properties->colors;

    posX = properties->x;
    posY = properties->y;

    text = properties->text;

    value = properties->value;

    data = (strcNotesKeyboardData*)(properties->data);

    width = properties->w;
    height = properties->h;

    setStyle(properties->style);
}

cNotesKeyboard::~cNotesKeyboard()
{
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cNotesKeyboard::setStyle(uint32_t style)
{
    this->style = style;

    textStyle = (style & controlStyleCenterX ? OPT_CENTERX : 0) |
                (style & controlStyleCenterY ? OPT_CENTERY : 0) |
                (style & controlStyleRightX ? OPT_RIGHTX : 0);

    textFont = FONT_INDEX_FROM_STYLE;
    textFont = (textFont >= 0) ? textFont : 0;
    textFont = fonts[textFont].handle;
}

void cNotesKeyboard::setText(const char* text)
{
    this->text = text;
}

void cNotesKeyboard::setValue(int value)
{
    this->value = value;
}

void cNotesKeyboard::setColors(const uint32_t* colors)
{
    this->colors = colors;
}

void cNotesKeyboard::setData(void* data)
{
    this->data = (strcNotesKeyboardData*)(data);
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cNotesKeyboard::update()
{
    int16_t border_x = posX;
    int16_t border_y = posY;

    if (style & controlStyleCenterX)
    {
        border_x = posX - (width / 2);
    }
    if (style & controlStyleCenterY)
    {
        border_y = posY - (height / 2);
    }

    //------------------------------------------

    // background
    API_COLOR(colors[1]);

    API_BEGIN(RECTS);
    API_VERTEX2F(border_x + 1, border_y);
    API_VERTEX2F(border_x + width - 2, border_y + height);
    API_END();

    API_COLOR(colors[0]);
    API_LINE_WIDTH(8);

    API_BEGIN(LINES);

    for (uint8_t i = 1; i < 8; i++)
    {
        API_VERTEX2F((width / 8) * i + posX, posY + height / 4);
        API_VERTEX2F((width / 8) * i + posX, posY + height);
    }

    API_END();

    //------------------------------------------

    API_LINE_WIDTH(16);

    for (uint8_t i = 0; i < 8; i++)
    {
        if (data->editButtonsVisible && (i <= 1 || i >= 6))
        {
            API_COLOR(colors[2]);
        }
        else
        {
            API_COLOR(colors[0]);
        }

        API_CMD_TEXT(posX + i * (width / 8) + (width / 16),
                     posY + height / 2,
                     textFont,
                     OPT_CENTER,
                     data->labels[i]);
    }
}
