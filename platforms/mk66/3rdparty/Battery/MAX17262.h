#ifndef LIBS_MAX17262_MAX17262_H_
#define LIBS_MAX17262_MAX17262_H_

#include <stdint.h>

const uint8_t MAX1704X_ADDR = 0x36;

class MAX17262
{
  public:
    bool begin(uint16_t designCap, uint16_t vEmpty);
    bool verify(bool extend = false);

    int getVoltage();
    int getSOC();
    int getCurrent();
    bool isCharging();
    bool isBatteryPresent();
    int getVersion();
    int getStatus();
    int getVEmpty();
    uint16_t getVRecovery();
    uint16_t getFullCapacity();
    uint16_t getCurrentCapacity();
    uint16_t getTimeToEmpty();
    uint16_t getTimeToFull();

    void setVEmpty(uint16_t mV);
    void setDesignCap(uint16_t mAh);

  private:
    int writeRegister(uint8_t reg, uint16_t value);
    int readRegister(uint8_t reg, uint8_t len = 2);
    void writeAndVerifyRegister(char RegisterAddress, int RegisterValueToWrite);

    uint16_t designCap;
    uint16_t vEmpty;
};

#endif /* LIBS_MAX17262_MAX17262_H_ */
