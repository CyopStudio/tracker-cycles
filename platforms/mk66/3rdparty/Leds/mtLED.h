#ifndef _MTLED_H_
#define _MTLED_H_

#include <stdint.h>

#define ISSI_ADDR_DEFAULT 0x74

#define ISSI_REG_CONFIG 0x00
#define ISSI_REG_CONFIG_PICTUREMODE 0x00
#define ISSI_REG_CONFIG_AUTOPLAYMODE 0x08
#define ISSI_REG_CONFIG_AUDIOPLAYMODE 0x18

#define ISSI_CONF_PICTUREMODE 0x00
#define ISSI_CONF_AUTOFRAMEMODE 0x04
#define ISSI_CONF_AUDIOMODE 0x08

#define ISSI_REG_PICTUREFRAME 0x01

#define ISSI_REG_SHUTDOWN 0x0A
#define ISSI_REG_AUDIOSYNC 0x06

#define ISSI_COMMANDREGISTER 0xFD
#define ISSI_BANK_FUNCTIONREG 0x0B

#define I2C_MAX_WAITING 10000

///
//#define LEDMEDUSA_I2C_DMA
///

#define IS31FL3731_ADDR1 0b1110100
#define IS31FL3731_ADDR2 0b1110101
#define IS31FL3731_ADDR3 0b1110110
#define IS31FL3731_ADDR4 0b1110111

const uint8_t medusaPadsLeds[64] = { 0,   1,   2,   3,   4,   5,   6,   7,   16,  17,  18,  19, 20,
                                     21,  22,  23,  32,  33,  34,  35,  36,  37,  38,  39,  48, 49,
                                     50,  51,  52,  53,  54,  55,  64,  65,  66,  67,  68,  69, 70,
                                     71,  80,  81,  82,  83,  84,  85,  86,  87,  96,  97,  98, 99,
                                     100, 101, 102, 103, 112, 113, 114, 115, 116, 117, 118, 119 };

class mtLED
{
  public:
    mtLED();
    void begin(void (*resetFunc)(void), uint8_t addr = ISSI_ADDR_DEFAULT);
    uint8_t update_all_leds();
    void update_pads();
    void setLED(uint8_t num, uint8_t state, uint8_t gamma_pwm);
    void setLEDpwm(uint8_t num, uint8_t state, uint8_t pwm);
    void clearAllPadsLED();

    void clear(void);
    void fillAll(void);
    void setLEDPWM(uint8_t lednum, uint8_t pwm, uint8_t bank = 0);
    void fastSetLEDPWM(uint8_t lednum, uint8_t pwm, uint8_t bank = 0);
    void setLEDOnOff2Bytes(uint16_t* arr, uint8_t bank);
    void setLEDOnOffBytes(uint8_t* arr, uint8_t bank);
    void audioSync(bool sync);
    void setFrame(uint8_t b);
    void displayFrame(uint8_t frame);
    void print_i2c_status(void);
    void debug(void);
    void setAllLEDPWM(uint8_t* arr, uint8_t bank);
    bool isDeadResetCheck(void);

    uint8_t get_g32(uint8_t g);
    uint8_t readRegister8(uint8_t bank, uint8_t reg);

    void dropRegInfo(void);
    void checkAndRepair(void);

    // private:

    void selectBank(uint8_t bank);
    void writeRegister8(uint8_t bank, uint8_t reg, uint8_t data);
    bool checkAndRepairReg(uint8_t reg);

    uint8_t _i2caddr, _frame;
    uint16_t _count = 0;
    bool _debug = 0;
    void (*resetTransmissionFunc)(void) = nullptr;

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

    uint8_t ledPWM[144] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
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
