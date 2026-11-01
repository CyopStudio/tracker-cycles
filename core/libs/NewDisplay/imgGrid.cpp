#include "FT812.h"

#include "display.h"
#include "imgGridControl.h"

#include <string.h>

ImgGrid::ImgGrid(strControlProperties* properties)
  : cDisplayControl(controlIdImgGrid, 1)
{
    colorsCount = 6;
    colors = display.getTheme().imgGridColors;

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

    data = (strImgGridData*)(properties->data);

    width = properties->w;
    height = properties->h;

    setStyle(properties->style);
}

ImgGrid::~ImgGrid()
{
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void ImgGrid::setStyle(uint32_t style)
{
    this->style = style;

    textStyle = (style & controlStyleCenterX ? OPT_CENTERX : 0) |
                (style & controlStyleCenterY ? OPT_CENTERY : 0) |
                (style & controlStyleRightX ? OPT_RIGHTX : 0);

    int8_t textFont = FONT_INDEX_FROM_STYLE;
    textFont = (textFont >= 0) ? textFont : 0;
    font = &fonts[textFont];
}

void ImgGrid::setText(const char* text)
{
    this->text = text;
}

void ImgGrid::setValue(int value)
{
    this->value = value;
}

void ImgGrid::setColors(const uint32_t* colors)
{
    this->colors = colors;
}

void ImgGrid::setData(void* data)
{
    this->data = (strImgGridData*)(data);
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void ImgGrid::update()
{
    if (data == nullptr)
    {
        return;
    }

    int colorIdx = -1;
    int boxW = (width - (data->columnsCount + 1)) / data->columnsCount;
    int boxH = (height - (data->rowsCount + 1)) / data->rowsCount;
    int frameX = posX;
    int frameY = posY;

    API_SAVE_CONTEXT();

    // squares

    API_BEGIN(RECTS);

    for (int column = 0; column < data->columnsCount; column++)
    {
        for (int row = 0; row < data->rowsCount; row++)
        {
            int currentBoxIdx = row * data->columnsCount + column;
            int curentColorIdx = currentBoxIdx == value ? 3 : 4;

            if (colorIdx != curentColorIdx)
            {
                colorIdx = curentColorIdx;
                API_COLOR(colors[colorIdx]);
            }

            int boxX = posX + boxW / 2 + boxW * column - data->imgW / 2;
            int boxY = posY + boxH / 2 + boxH * row - data->imgH / 2;

            API_VERTEX2F(boxX, boxY);
            API_VERTEX2F(boxX + data->imgW, boxY + data->imgH);

            if (currentBoxIdx == data->selection)
            {
                frameX = posX + boxW * column;
                frameY = posY + boxH * row + 8;
            }
        }
    }

    API_END();

    // images
    if (data->imgList != nullptr)
    {
        for (int column = 0; column < data->columnsCount; column++)
        {
            for (int row = 0; row < data->rowsCount; row++)
            {
                int currentBoxIdx = row * data->columnsCount + column;
                int curentColorIdx = currentBoxIdx == value ? 1 : 2;

                if (colorIdx != curentColorIdx)
                {
                    colorIdx = curentColorIdx;
                    API_COLOR(colors[colorIdx]);
                }

                int currentImgIdx = row * data->columnsCount + column;

                int8_t idx = data->imgList[currentImgIdx];

                if (idx < 0 || idx >= displayBitmapsCount)
                {
                    continue;
                }

                int imgPosX = posX + boxW * column + (boxW / 2) - (bitmaps[idx].width / 2);
                int imgPosY = posY + boxH * row + (boxH / 2) - (bitmaps[idx].height / 2);

                API_BITMAP_SOURCE(bitmaps[idx].address);
                API_BITMAP_LAYOUT(
                  bitmaps[idx].format, bitmaps[idx].linestride, bitmaps[idx].height);
                API_BITMAP_SIZE(NEAREST, BORDER, BORDER, bitmaps[idx].width, bitmaps[idx].height);

                API_BEGIN(BITMAPS);
                API_VERTEX2F(imgPosX, imgPosY);
                API_END();
            }
        }
    }

    // text

    if (data->textList != nullptr)
    {
        colorIdx = 0;
        API_COLOR(colors[colorIdx]);

        for (int column = 0; column < data->columnsCount; column++)
        {
            for (int row = 0; row < data->rowsCount; row++)
            {
                int currentBoxIdx = row * data->columnsCount + column;

                if (data->textList[currentBoxIdx] == nullptr)
                {
                    continue;
                }

                int textX = posX + boxW * column + boxW / 2;
                int textY = posY + boxH * row + boxH - font->height + 3;

                char* text = data->textList[currentBoxIdx];

                API_CMD_TEXT(textX, textY, font->handle, textStyle, text);
            }
        }
    }

    // frame

    if (data->showSelection)
    {
        API_COLOR(colors[5]);
        API_BEGIN(LINE_STRIP);

        API_VERTEX2F(frameX, frameY);
        API_VERTEX2F(frameX + boxW, frameY);
        API_VERTEX2F(frameX + boxW, frameY + boxH);
        API_VERTEX2F(frameX, frameY + boxH);
        API_VERTEX2F(frameX, frameY);

        API_END();
    }

    API_RESTORE_CONTEXT();
}
