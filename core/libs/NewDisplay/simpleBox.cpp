#include "simpleBox.h"
#include "FT812.h"
#include "display.h"

SimpleBox::SimpleBox(strControlProperties* properties)
  : cDisplayControl(controlIdSimpleBox, 1)
{
    colorsCount = 1;
    colors = &display.getTheme().red;

    if (properties == nullptr)
    {
        posX = 0;
        posY = 0;
        text = nullptr;
        value = 0;
        width = 0;
        height = 0;
        style = 0;
        return;
    }

    colors = (properties->colors == nullptr) ? colors : properties->colors;

    posX = properties->x;
    posY = properties->y;

    text = properties->text;
    value = properties->value;

    width = properties->w;
    height = properties->h;

    setStyle(properties->style);
}

SimpleBox::~SimpleBox()
{
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void SimpleBox::setStyle(uint32_t style)
{
    this->style = style;

    textStyle = (style & controlStyleCenterX ? OPT_CENTERX : 0) |
                (style & controlStyleCenterY ? OPT_CENTERY : 0) |
                (style & controlStyleRightX ? OPT_RIGHTX : 0);

    textFont = FONT_INDEX_FROM_STYLE;
    textFont = (textFont >= 0) ? textFont : 0;
    textFont = fonts[textFont].handle;
}

void SimpleBox::setText(const char* text)
{
    this->text = text;
}

void SimpleBox::setValue(int value)
{
    this->value = value;
}

void SimpleBox::setColors(const uint32_t* colors)
{
    this->colors = colors;
}

void SimpleBox::setData(void* data)
{
    (void)data;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void SimpleBox::update()
{
    API_COLOR(colors[0]);

    API_BEGIN(RECTS);
    API_VERTEX2F(posX, posY);
    API_VERTEX2F(posX + width, posY + height);
    API_END();
}
