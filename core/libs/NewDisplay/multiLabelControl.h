#ifndef LIBS_NEWDISPLAY_MULTILABELCONTROL_H_
#define LIBS_NEWDISPLAY_MULTILABELCONTROL_H_

struct strMultiLabelData
{
    uint16_t xOffset = 0;
    uint16_t yOffset = 0;

    uint8_t textLinesCount = 1;

    uint8_t selected = 0;
    uint32_t style[5] = { 0 };
    char* text[5] = { nullptr };

    uint16_t yForFxTextHole = 0;
};

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cMultiLabel : public cDisplayControl
{
  public:
    cMultiLabel(strControlProperties* properties = nullptr);
    virtual ~cMultiLabel();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    uint16_t textStyle;
    strMultiLabelData* data;
};

#endif
