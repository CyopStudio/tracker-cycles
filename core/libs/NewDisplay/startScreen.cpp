
#include "FT812.h"

#include "display.h"
#include "displayControls.h"
#include "startScreenControl.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
cStartScreen::cStartScreen(strControlProperties* properties)
  : cDisplayControl(controlIdStartScreen, 1)
{
    colorsCount = 2;
    colors = display.getTheme().startScreenColors;

    if (properties == nullptr)
    {
        posX = 0;
        posY = 0;
        text = nullptr;
        data = nullptr;
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

    data = (char*)(properties->data);

    setStyle(properties->style);
}

cStartScreen::~cStartScreen()
{
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cStartScreen::setStyle(uint32_t style)
{
    this->style = style;

    textStyle = (style & controlStyleCenterX ? OPT_CENTERX : 0) |
                (style & controlStyleCenterY ? OPT_CENTERY : 0) |
                (style & controlStyleRightX ? OPT_RIGHTX : 0);

    textFont = FONT_INDEX_FROM_STYLE;
    textFont = (textFont >= 0) ? textFont : 0;
    textFont = fonts[textFont].handle;
}

void cStartScreen::setText(const char* text)
{
    this->text = text;
}

void cStartScreen::setValue(int value)
{
    this->value = value;
}

void cStartScreen::setColors(const uint32_t* colors)
{
    this->colors = colors;
}

void cStartScreen::setData(void* data)
{
    data = (char*)(data);
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cStartScreen::update()
{
#define bitmap_index 0

    API_SAVE_CONTEXT();
    // API_BITMAP_HANDLE(0);
    API_BITMAP_SOURCE(bitmaps[bitmap_index].address);
    API_BITMAP_LAYOUT(
      bitmaps[bitmap_index].format, bitmaps[bitmap_index].linestride, bitmaps[bitmap_index].height);
    API_BITMAP_SIZE(
      NEAREST, BORDER, BORDER, bitmaps[bitmap_index].width, bitmaps[bitmap_index].height);

    API_BEGIN(BITMAPS);
    API_VERTEX2F((800 / 2) - (bitmaps[bitmap_index].width / 2), 100);
    API_END();

    uint16_t barWidth = (5 * width) / 7;
    uint16_t barX = posX + (width / 7);

    uint16_t barHeight = 3;
    uint16_t barY = posY + ((3 * height) / 6);

    uint16_t textX = posX + (width / 2);
    uint16_t textY = posY + ((2 * height) / 7);

    if (value < 0)
        value = 0;
    else if (value > 100)
        value = 100;

    uint16_t barFillY = (barWidth * value) / 100;

    API_BLEND_FUNC(SRC_ALPHA, ZERO);

    API_COLOR(colors[1]);

    API_LINE_WIDTH(16);
    API_BEGIN(RECTS);
    API_VERTEX2F(barX + 1, barY + 1);
    API_VERTEX2F(barX + barFillY - 1, barY + barHeight - 1);
    API_END();

    API_BLEND_FUNC(SRC_ALPHA, ONE_MINUS_SRC_ALPHA);

    API_COLOR(colors[0]);
    if (text != nullptr)
    {
        API_CMD_TEXT(textX, textY, textFont, OPT_CENTER, text);
    }
    if (data != nullptr)
    {
        API_CMD_TEXT(10, 457, textFont, 0, "v");
        API_CMD_TEXT(22, 457, textFont, 0, data);
    }

    API_RESTORE_CONTEXT();
}
