

#include "FT812.h"
#include <string.h>

#include "displayControls.h"
#include "log.h"

DisplayDriver* cDisplayControl::displayDriver;

uint16_t getTextWidth(const strFont* font, const char* text, uint16_t length)
{
    uint16_t width = 0;

    for (uint16_t i = 0; i < length && text[i] != 0; i++)
    {
        width += font->data[(uint8_t)text[i]];
    }

    return width;
}

uint16_t getMaxTextLengthThatFitSpecificWidth(const strFont* font,
                                              const char* text,
                                              uint16_t widthLimit)
{
    int strLen = strlen(text);
    int sumWidth = 0;
    int length = 0;

    for (uint16_t i = 0; i < strLen && text[i] != 0; i++)
    {
        sumWidth += font->data[(uint8_t)text[i]];

        if (sumWidth <= widthLimit)
        {
            length++;
        }
    }

    return length;
}

void cDisplayControl::string2Bitmaps(int16_t x,
                                     int16_t y,
                                     const char* string,
                                     int8_t length,
                                     bool constWidth)
{
    uint8_t strPtr = 0;

    if (font == nullptr)
        return;

    API_SAVE_CONTEXT();

    API_BITMAP_HANDLE(font->handle);
    API_BEGIN(BITMAPS);

    unsigned char ch;
    for (uint8_t i = 0; i < length; i++)
    {
        ch = string[strPtr++];
        if (ch == 0)
            return;

        if (0 <= x && x <= 799 && 0 <= y && y <= 479)
        {
            if ((x > 511 || y > 511) && font->data[ch])
            {
                API_CELL(ch);
                API_VERTEX2F(x, y);
            }
            else if (font->data[ch])
            {
                API_VERTEX2II(x, y, font->handle, ch);
            }
        }
        if (constWidth)
        {
            x += font->width;
        }
        else
        {
            x += font->data[ch];
        }
    }

    API_END();

    API_RESTORE_CONTEXT();
}
