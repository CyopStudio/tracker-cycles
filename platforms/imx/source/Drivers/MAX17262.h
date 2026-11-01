#ifndef LIBS_MAX17262_MAX17262_H_
#define LIBS_MAX17262_MAX17262_H_

#include "i2cDevice.h"
#include <stdint.h>

const uint8_t MAX1704X_ADDR = 0x36;

class MAX17262 : public I2cDevice
{
  public:
    bool begin(uint16_t designCap, uint16_t vEmpty, uint16_t ichgTerm);
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
    void setIchgTerm(uint16_t mA);
    void setDesignCap(uint16_t mAh);

    virtual bool update() override;

  private:
    int writeRegister(uint8_t reg, uint16_t value);
    int readRegister(uint8_t reg, uint8_t len = 2);
    void writeAndVerifyRegister(char RegisterAddress, int RegisterValueToWrite);

    uint16_t designCap = 4310;
    uint16_t vEmpty = 3000;
    uint16_t ichgTerm = 150;

    enum regs
    {
        regStatus = 0x00,
        regVAlrtTh = 0x01,
        regTAlrtTh = 0x02,
        regSAlrtTh = 0x03,
        regIAlrtTh = 0x04,
        regRepCap = 0x05,
        regRepSOC = 0x06,
        regTemp = 0x08,
        regVCell = 0x09,
        regCurrent = 0x0A,
        regAvgCurrent = 0x0B,

        regFullCapRep = 0x10,
        regTTE = 0x11,
        regAvgTA = 0x16,
        regCycles = 0x17,
        regDesignCap = 0x18,
        regAvgVCell = 0x19,
        regMaxMinTemp = 0x1A,
        regMaxMinVolt = 0x1B,
        regMaxMinCurr = 0x1C,
        regIchgTerm = 0x1E,

        regTTF = 0x20,
        regDevName = 0x21,
        regFullCapNom = 0x23,

        regDieTemp = 0x34,
        regVEmpty = 0x3A,
        regFStat = 0x3d,

        regdQAcc = 0x45,
        regdPAcc = 0x46,

        regCommand = 0x60,

        regStatus2 = 0xB0,
        regPower = 0xB1,
        regAvgPower = 0xB3,
        regHibCfg = 0xBA,

        regModelCfg = 0xDB,

    };
};

#endif /* LIBS_MAX17262_MAX17262_H_ */
