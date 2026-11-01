#include "EEPROM.h"

cEEPROM EEPROM;

void cEEPROM::begin()
{
    eeprom_initialize();
}

void cEEPROM::readData(int idx, void* data, uint16_t size)
{
    int e = idx;
    uint8_t* ptr = (uint8_t*)data;
    for (; size > 0; --size, ++e)
    {
        *ptr++ = eeprom_read_byte((uint8_t*)e);
    }
}

void cEEPROM::writeData(int idx, void* data, uint16_t size)
{
    eeprom_write_block(data, (void*)idx, size);
}
