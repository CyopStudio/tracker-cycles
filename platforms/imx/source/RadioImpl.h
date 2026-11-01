#ifndef SOURCE_RADIOIMPL_H_
#define SOURCE_RADIOIMPL_H_

#include "Platform.h"
#ifndef TRACKER_MINI
#include "Si4703.h"
#include "fsl_gpio.h"
#endif

class I2cDevice;

class RadioImpl : public Radio
{
  public:
    virtual void enableLineInput(bool enabled) override;
    virtual void enable(bool enabled) override;
    virtual bool setFrequency(float freq) override;
    virtual void setSeekCallback(void (*seekCallback)(void)) override;
    virtual void seek(SeekDirection direction) override;
    virtual void updateSeek() override;
    virtual bool updateRDS() override;
    virtual float getFrequency() override;
    virtual float getBaseFrequency() override;
    virtual float getSpacing() override;
    virtual bool isinitialized() override;
    virtual void clearRDS() override;
    virtual void setRegion(Region region) override;
    virtual bool isRegionChanged() override;
    virtual char* getStationName() override;

#ifndef TRACKER_MINI
    struct Pins
    {
        uint32_t resetPin;
        GPIO_Type* resetBase;
        uint32_t i2cDataPin;
        GPIO_Type* i2cDataBase;
        std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t> i2cDataMux;
        uint32_t lineInSourceSelectPin;
        GPIO_Type* lineInSourceSelectBase;
    };

    void init(Pins* pins);
    I2cDevice* getI2cDevice();

  private:
    Si4703 radioDriver;
    uint32_t lineInSourceSelectPin;
    GPIO_Type* lineInSourceSelectBase;
#endif
};

#endif /* SOURCE_RADIOIMPL_H_ */
