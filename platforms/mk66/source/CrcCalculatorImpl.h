#ifndef SOURCE_CRCCALCULATORIMPL_H_
#define SOURCE_CRCCALCULATORIMPL_H_
#include "Platform.h"

class CrcCalculatorImpl : public CrcCalculator
{
  public:
    virtual uint32_t calculate32bitCrc(const uint8_t* data, const uint16_t datalen) override;
};

#endif /* SOURCE_CRCCALCULATORIMPL_H_ */
