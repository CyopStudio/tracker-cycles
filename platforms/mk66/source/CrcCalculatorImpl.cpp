#include "CrcCalculatorImpl.h"

#include "FastCRC.h"

uint32_t CrcCalculatorImpl::calculate32bitCrc(const uint8_t* data, const uint16_t datalen)
{
    FastCRC32 crcCalc;

    return crcCalc.crc32(data, datalen);
}
