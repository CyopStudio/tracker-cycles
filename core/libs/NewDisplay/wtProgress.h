#ifndef _WT_PROGRESS_H_
#define _WT_PROGRESS_H_

#include "displayControls.h"

typedef struct
{
    uint32_t position;
    uint32_t window;
    uint32_t positionMax;

} wt_position_data_t;

//--------------------------------------------------------------------
class cWtProgress : public cDisplayControl
{
  public:
    cWtProgress(strControlProperties* properties);
    virtual ~cWtProgress();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

  private:
    wt_position_data_t* wt_data = nullptr;
};

#endif
