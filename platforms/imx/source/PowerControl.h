#ifndef SOURCE_POWERCONTROL_H_
#define SOURCE_POWERCONTROL_H_

#include "fsl_gpio.h"

#include <stdint.h>

class PlatformImpl;

class PowerControl
{
  public:
    void init(int powerSetPin,
              GPIO_Type* powerSetGpio,
              int powerResetPin,
              GPIO_Type* powerResetGpio,
              int powerButtonPin,
              GPIO_Type* powerButtonGpio,
              int voltageReadPin);
    void update(PlatformImpl* platformImpl);

    bool getPowerButtonState();
    void turnPowerOff();
    void setLowVoltageCallback(void (*lowVoltageCallback)(void));
    void setLowVoltageThreshold(float lowVoltageThreshold);
    bool isLowVoltage();

  private:
    static constexpr float kUsbVoltage = 5.0f;
    static constexpr float kResistorDivider = 2.0f;
    static constexpr float kMeasurmentVoltage = kUsbVoltage / kResistorDivider;
    static constexpr int kUpdateTimeMs = 10;
    static constexpr int kAnalogReadResolution = 12;
    static constexpr int kReadingsForMeasurement = 4;
    static constexpr float kLowVoltageThreshold = 4.4f;

    int measure();

    int powerSetPin;
    int powerResetPin;
    int powerButtonPin;
    GPIO_Type* powerSetGpio;
    GPIO_Type* powerResetGpio;
    GPIO_Type* powerButtonGpio;
    int voltageReadPin;

    bool powerButtonLastState;

    void (*lowVoltageCallback)(void);
    int threshold;
    int maxVolgateValue;
    uint32_t updateTime = 0;
};

#endif /* SOURCE_POWERCONTROL_H_ */
