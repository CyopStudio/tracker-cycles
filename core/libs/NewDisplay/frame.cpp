#include "FT812.h"

#include "commonControls.h"
#include "display.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
cFrame::cFrame(strControlProperties* properties)
  : cDisplayControl(controlIdFrame, 1)
{
    colorsCount = 2;
    colors = display.getTheme().frameColors;

    if (properties == nullptr)
    {
        posX = 0;
        posY = 0;
        text = nullptr;
        data = nullptr;
        width = 0;
        height = 0;
        style = 0;
        return;
    }

    colors = (properties->colors == nullptr) ? colors : properties->colors;

    posX = properties->x;
    posY = properties->y;

    text = properties->text;
    data = (strFrameData*)properties->data;

    width = properties->w;
    height = properties->h;

    setStyle(properties->style);
}

cFrame::~cFrame()
{
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cFrame::setStyle(uint32_t style)
{
    this->style = style;
}

void cFrame::setText(const char* text)
{
    this->text = text;
}

void cFrame::setValue(int value)
{
    this->value = value;
}

void cFrame::setColors(const uint32_t* colors)
{
    this->colors = colors;
}

void cFrame::setData(void* data)
{
    this->data = (strFrameData*)data;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cFrame::update()
{
    if (data == nullptr)
    {
        return;
    }

    if (data->placesCount == 0)
    {
        return;
    }

    if (value >= data->placesCount)
    {
        value = data->placesCount - 1;
    }
    else if (value < 0)
    {
        value = 0;
    }

    uint8_t loopCount = 0;
    uint8_t foundActive = 0;

    if (data->multiSelActiveNum == 0)
    {
        loopCount = 1;
    }
    else
    {
        loopCount = 8;
    }

    for (uint8_t i = 0; i < loopCount; i++)
    {
        if ((data->multisel[i].isActive) || (i == 0 && data->multiSelActiveNum == 0))
        {
            if ((foundActive == data->multiSelActiveNum) && (data->multiSelActiveNum != 0))
            {
                break;
            }

            foundActive++;
            uint8_t source;

            if (data->multiSelActiveNum)
            {
                source = data->multisel[i].frameNum;
            }
            else
            {
                source = value;
            }

            if (source >= data->placesCount || data->places[source] == nullptr)
            {
                continue;
            }
            int16_t border_x = *(data->places[source]);
            int16_t border_y = *((data->places[source]) + 1);
            int16_t border_w = *((data->places[source]) + 2);
            int16_t border_h = *((data->places[source]) + 3);

            if (style & controlStyleCenterX)
            {
                border_x = posX - (width / 2);
            }

            if (style & controlStyleBackground)
            {
                if (style & controlStyleRoundedBorder)
                {
                    API_LINE_WIDTH(32);
                }
                else
                {
                    API_LINE_WIDTH(16);
                }

                API_COLOR(colors[1]);

                API_BEGIN(RECTS);
                API_VERTEX2F(border_x, border_y);
                API_VERTEX2F(border_x + border_w, border_y + border_h);
                API_END();
            }

            API_COLOR(colors[0]);

            API_LINE_WIDTH(kRedFrameWidth);
            API_BEGIN(LINE_STRIP);
            API_VERTEX2F(border_x, border_y);
            API_VERTEX2F(border_x + border_w, border_y);
            API_VERTEX2F(border_x + border_w, border_y + border_h);
            API_VERTEX2F(border_x, border_y + border_h);
            API_VERTEX2F(border_x, border_y);
            API_END();
        }
    }
}
