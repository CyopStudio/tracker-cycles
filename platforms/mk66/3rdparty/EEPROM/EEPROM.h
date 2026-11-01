#ifndef __EEPROM_h__
#define __EEPROM_h__

#include <avr/eeprom.h>
#include <stdint.h>

struct cEEPROM
{
    void begin();
    void readData(int idx, void* data, uint16_t size);
    void writeData(int idx, void* data, uint16_t size);
};

extern cEEPROM EEPROM;

#endif
