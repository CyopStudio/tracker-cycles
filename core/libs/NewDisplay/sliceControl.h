#ifndef SLICE_CONTROL_H_
#define SLICE_CONTROL_H_

#include "displayControls.h"
#include "mtStructs.h"

struct strSlicePoints
{
    uint8_t sliceCount = 0;
    int16_t slice[MAX_SLICE_NUMBER] = { 0 };
    bool isSliceZerocrossed[MAX_SLICE_NUMBER];
    uint8_t selected = 0;
};

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cSlicePoints : public cDisplayControl
{
  public:
    cSlicePoints(strControlProperties* properties = nullptr);
    virtual ~cSlicePoints();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    strSlicePoints* points;

    uint16_t textStyle;
    int16_t textFont;
};

#endif
