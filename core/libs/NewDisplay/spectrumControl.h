
#ifndef _SPECTRUM_CONTROL_H_
#define _SPECTRUM_CONTROL_H_

#include "displayControls.h"

struct strTrackerSpectrum
{
    uint8_t spectrumType;
    int8_t upperData[600];
    int8_t lowerData[600];

    uint16_t loadProgress;

    uint32_t width;
    enum PointsLimit
    {
        k200,
        k300,
        k600
    } pointsLimit;

    void init(uint32_t w = 0, PointsLimit limit = PointsLimit::k300)
    {
        spectrumType = 0;
        for (int i = 0; i < 600; i++)
        {
            upperData[i] = 0;
            lowerData[i] = 0;
        }
        loadProgress = 0;
        width = w;
        pointsLimit = limit;
    }
};

//--------------------------------------------------------------------
class cSpectrum : public cDisplayControl
{
  public:
    cSpectrum(strControlProperties* properties);
    virtual ~cSpectrum();

    virtual void update();
    virtual void update2();
    virtual void update3();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

  private:
    strTrackerSpectrum* spectrum;

    uint16_t ramPartSize[5] = { 0 };
    uint8_t refreshStep;
};

#endif
