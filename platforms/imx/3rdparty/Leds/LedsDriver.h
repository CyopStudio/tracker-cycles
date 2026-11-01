#ifndef _MTLED_H_
#define _MTLED_H_

#include "i2cDevice.h"
#include <stdint.h>

#define ISSI_ADDR_DEFAULT 0x74

#define ISSI_REG_CONFIG               0x00
#define ISSI_REG_CONFIG_PICTUREMODE   0x00
#define ISSI_REG_CONFIG_AUTOPLAYMODE  0x08
#define ISSI_REG_CONFIG_AUDIOPLAYMODE 0x18

#define ISSI_CONF_PICTUREMODE   0x00
#define ISSI_CONF_AUTOFRAMEMODE 0x04
#define ISSI_CONF_AUDIOMODE     0x08

#define ISSI_REG_PICTUREFRAME 0x01

#define ISSI_REG_SHUTDOWN  0x0A
#define ISSI_REG_AUDIOSYNC 0x06

#define ISSI_COMMANDREGISTER  0xFD
#define ISSI_BANK_FUNCTIONREG 0x0B

#define I2C_MAX_WAITING 10000

///
//#define LEDMEDUSA_I2C_DMA
///

#define IS31FL3731_ADDR1 0b1110100
#define IS31FL3731_ADDR2 0b1110101
#define IS31FL3731_ADDR3 0b1110110
#define IS31FL3731_ADDR4 0b1110111

class mtLED : public I2cDevice
{
  public:
    mtLED();
    void begin(uint8_t addr = ISSI_ADDR_DEFAULT);
    virtual bool update() override;
    void setLED(uint8_t num, uint8_t state, uint8_t gamma_pwm);
    void setAllLEDPWM(uint8_t* arr, uint8_t bank);

    uint8_t ledPWM[144] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

  private:
    void audioSync(bool sync);
    void setLEDOnOffBytes(uint8_t* arr, uint8_t bank);
    void checkAndRepair(void);
    bool isDeadResetCheck(void);
    void displayFrame(uint8_t frame);
    void clear(void);
    void selectBank(uint8_t bank);
    uint8_t readRegister8(uint8_t bank, uint8_t reg);
    void writeRegister8(uint8_t bank, uint8_t reg, uint8_t data);
    bool checkAndRepairReg(uint8_t reg);

    uint8_t _i2caddr, _frame;
    uint16_t _count = 0;
    bool _debug = 0;

    uint8_t gamma32[32] = { 0,   1,   2,   4,   6,   10,  13,  18,  22,  28,  33,
                            39,  46,  53,  61,  69,  78,  86,  96,  106, 116, 126,
                            138, 149, 161, 173, 186, 199, 212, 226, 240, 255 };

    uint8_t ledState[144];
    uint8_t leds_need_update = 0;

    uint8_t ledBits[18] = { //---A----  ----B-----
                            0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
                            0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
                            0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000
    };

    // default values
    uint8_t functionRegBuff[0x0D] = {
        0x00, //  0x00
        0x00, //  0x01
        0x00, //  0x02
        0x00, //  0x03
        0x00, //  0x04
        0x00, //  0x05
        0x00, //  0x06
        0x00, //  0x07
        0x00, //  0x08
        0x00, //  0x09
        0x00, //  0x0A
        0x00, //  0x0B
        0x00  //  0x0C
    };
};

#endif
