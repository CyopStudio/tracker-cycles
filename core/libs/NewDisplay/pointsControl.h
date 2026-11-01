#ifndef LIBS_NEWDISPLAY_POINTS_H_
#define LIBS_NEWDISPLAY_POINTS_H_

#include "displayControls.h"

enum
{
    selectStart = 0x01U,
    selectLoop1 = 0x02U,
    selectLoop2 = 0x04U,
    selectEnd = 0x08U,
};

enum
{
    startZerocrossed = 0x01U,
    loop1Zerocrossed = 0x02U,
    loop2Zerocrossed = 0x04U,
    endZerocrossed = 0x08U,
};

struct strTrackerPoints
{
    uint8_t pointsType = 0;
    int16_t startPoint;
    int16_t endPoint;
    int16_t loopPoint1;
    int16_t loopPoint2;

    uint8_t selected = 0;
    uint8_t zerocrossed = 0;
};

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cPoints : public cDisplayControl
{
  public:
    cPoints(strControlProperties* properties = nullptr);
    virtual ~cPoints();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

  private:
    strTrackerPoints* points;

    uint16_t textStyle;
    int16_t textFont;

    void drawPoint(int posX, int posY, int height, bool topMark, bool markSide);
};

#endif
