#ifndef LIBS_NEWDISPLAY_LINEINDICATORCONTROL_H_
#define LIBS_NEWDISPLAY_LINEINDICATORCONTROL_H_

#include "progressCursorControl.h"

struct strLineIndicatorSelection
{
    uint16_t start;
    uint16_t end;
};

class cLineIndicator : public cProgressCursor
{
  public:
    cLineIndicator(strControlProperties* properties);
    ~cLineIndicator();
    virtual void update();
    virtual void setData(void* data);

  private:
    strLineIndicatorSelection* lineIndicatorSelection;
};

#endif /* LIBS_NEWDISPLAY_LINEINDICATORCONTROL_H_ */
