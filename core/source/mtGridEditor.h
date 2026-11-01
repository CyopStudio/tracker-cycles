#ifndef SOURCE_MTGRIDEDITOR_H_
#define SOURCE_MTGRIDEDITOR_H_
#include "mtStructs.h"
#include "stdint.h"
class mtGridEditor
{
  public:
    enum enChangePadDirection
    {
        enChangePadDirectionUp,
        enChangePadDirectionDown,
        enChangePadDirectionLeft,
        enChangePadDirectionRight
    };

    void open();
    void close();
    void changeSelectedPad(enChangePadDirection dir);
    void changeSelectedPad(uint8_t pad);
    void confirmSelectedPad();
    void cancelSelectedPad();
    void changeNote(int16_t val);
    void changeMicrotune(int16_t val);
    void changeLedEnable(int16_t val);
    bool getIsConfirmedPad();
    uint8_t getSelectedPad();

  private:
    uint8_t selectedPad;
    bool isConfirmedPad;
};

extern mtGridEditor gridEditor;
extern strGrid mtGrid;

#endif /* SOURCE_MTGRIDEDITOR_H_ */
