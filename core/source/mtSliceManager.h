#ifndef SOURCE_MTSLICEMANAGER_H_
#define SOURCE_MTSLICEMANAGER_H_
#include "mtStructs.h"

class mtSliceManager
{
  public:
    void addSlice(strInstrument* instr);
    void removeSlice(strInstrument* instr);
    void adjustSlice(strInstrument* instr, int32_t val);
    void autoSlice(strInstrument* instr);
    void equalSlice(strInstrument* instr, uint8_t n);

    uint8_t getAutoSliceProgress();
    uint8_t getAutoSliceState();
};

extern mtSliceManager sliceManager;

#endif /* SOURCE_MTSLICEMANAGER_H_ */
