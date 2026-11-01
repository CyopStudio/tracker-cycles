#ifndef LIBS_NEWDISPLAY_TEXTPOPUPCONTROL_H_
#define LIBS_NEWDISPLAY_TEXTPOPUPCONTROL_H_

struct strTextPopupData
{
    uint8_t textLinesCount = 1;

    uint32_t** multiLineColors = nullptr;
    uint32_t** multiLineStyle = nullptr;
    char** multiLineText = nullptr;
};

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cTextPopup : public cDisplayControl
{
  public:
    cTextPopup(strControlProperties* properties = nullptr);
    virtual ~cTextPopup();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    uint16_t textStyle;

    strTextPopupData* data;
};

#endif
