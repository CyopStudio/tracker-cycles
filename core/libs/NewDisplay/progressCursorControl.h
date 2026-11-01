#ifndef _PROGRESS_CURSOR_CONTROL_H_
#define _PROGRESS_CURSOR_CONTROL_H_

#include "displayControls.h"

//--------------------------------------------------------------------
class cProgressCursor : public cDisplayControl
{
  public:
    cProgressCursor(strControlProperties* properties);
    virtual ~cProgressCursor();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

  private:
};

#endif
