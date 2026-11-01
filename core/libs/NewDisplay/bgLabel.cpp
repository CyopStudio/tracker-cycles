#include "FT812.h"

#include "commonControls.h"
#include "display.h"
#include "displayControls.h"

#include <string.h>

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
cBgLabel::cBgLabel(strControlProperties* properties)
  : cDisplayControl(controlIdBgLabel, 1)
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

    data = (strBgLabelData*)(properties->data);

    width = properties->w;
    height = properties->h;

    setStyle(properties->style);
}

cBgLabel::~cBgLabel()
{
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cBgLabel::setStyle(uint32_t style)
{
    this->style = style;
}

void cBgLabel::setText(const char* text)
{
    this->text = text;
}

void cBgLabel::setValue(int value)
{
    this->value = value;
}

void cBgLabel::setColors(const uint32_t* colors)
{
    this->colors = colors;
}

void cBgLabel::setData(void* data)
{
    this->data = (strBgLabelData*)(data);
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cBgLabel::update()
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
        if (value & (1 << i))
        {
            API_VERTEX2F((width / 8) * i + posX, posY + height / 4);
            API_VERTEX2F((width / 8) * i + posX, posY + height);
        }
    }

    API_END();

    //------------------------------------------

    if (style & controlStyleBorder)
    {
        API_COLOR(colors[2]);
        API_LINE_WIDTH(24);

        API_BEGIN(LINE_STRIP);
        API_VERTEX2F(border_x, border_y);
        API_VERTEX2F(border_x + width, border_y);
        API_VERTEX2F(border_x + width, border_y + height);
        API_VERTEX2F(border_x, border_y + height);
        API_VERTEX2F(border_x, border_y);
        API_END();
    }

    API_LINE_WIDTH(16);
}
