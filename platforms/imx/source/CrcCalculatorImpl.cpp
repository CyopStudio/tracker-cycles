#include "CrcCalculatorImpl.h"
#include "polyassert.h"

uint32_t CrcCalculatorImpl::calculate32bitCrc(const uint8_t* data, const uint16_t datalen)
{
    (void)data;
    (void)datalen;

    // imxrt1051 doesn't have full-configurable hardware crc generator, it generates only specific
    // crc variant and there isn't any possibility to generate crc like in MK66. We decided to
    // add fake implementation, and avoid checking crc in Tracker project. Maybe we'll get rid of
    // crc from both of projects in next step.

    return 0;
}
