#include "PowerControl.h"
#include "PlatformImpl.h"
#include "TimeControl.h"
#include "log.h"
#include "polyassert.h"
#include "util_math.h"

//#define LOW_VOLTAGE_DETECTION_ENABLED
//#define LOW_VOLTAGE_DETECTION_TEST

void PowerControl::init(int powerSetPin,
                        GPIO_Type* powerSetGpio,
                        int powerResetPin,
                        GPIO_Type* powerResetGpio,
                        int powerButtonPin,
                        GPIO_Type* powerButtonGpio,
                        int voltageReadPin)
{
    this->powerButtonPin = powerButtonPin;
    this->powerButtonGpio = powerButtonGpio;
    this->powerSetPin = powerSetPin;
    this->powerResetPin = powerResetPin;
    this->voltageReadPin = voltageReadPin;
    this->powerSetGpio = powerSetGpio;
    this->powerResetGpio = powerResetGpio;

    powerButtonLastState = getPowerButtonState();
    updateTime = 0;
}

bool PowerControl::getPowerButtonState()
{
    return GPIO_ReadPinInput(powerButtonGpio, powerButtonPin);
}

void PowerControl::update(PlatformImpl* platformImpl)
{
#ifdef LOW_VOLTAGE_DETECTION_ENABLED
    if (timeControl.millis() > updateTime + kUpdateTimeMs)
    {
        if (isLowVoltage())
        {
#ifdef LOW_VOLTAGE_DETECTION_TEST
            LOG("Threshold has been exceeded");
#endif
            if (lowVoltageCallback != nullptr)
            {
                lowVoltageCallback();
            }
        }
        updateTime = timeControl.millis();
    }
#endif

    bool state = getPowerButtonState();

    if (state == powerButtonLastState)
    {
        return;
    }
    else if (state == true)
    {
        platformImpl->getPowerButtonCallback()(1);
        powerButtonLastState = true;
    }
    else if (state == false)
    {
        powerButtonLastState = false;
        platformImpl->getPowerButtonCallback()(0);
    }
}

void PowerControl::setLowVoltageCallback(void (*lowVoltageCallback)(void))
{
    assert(lowVoltageCallback);
    this->lowVoltageCallback = lowVoltageCallback;
}

void PowerControl::turnPowerOff()
{
    // wait until power button released
    while (GPIO_PinRead(powerButtonGpio, powerButtonPin))
    {
    }

    // reset power switch
    GPIO_PinWrite(powerResetGpio, powerResetPin, 1);
    GPIO_PinWrite(powerSetGpio, powerSetPin, 0);
}

void PowerControl::setLowVoltageThreshold(float lowVoltageThreshold)
{
    assert(lowVoltageThreshold);
    assert(lowVoltageThreshold > 0.0f);

    int resolutionMax = (1 << (kAnalogReadResolution)) - 1;
    maxVolgateValue = map<float>(kUsbVoltage / kResistorDivider, 0.0f, 3.3f, 0, resolutionMax);
    threshold =
      (int)map<float>(lowVoltageThreshold, 0.0f, kUsbVoltage, 0.0f, (float)maxVolgateValue);
}

int PowerControl::measure()
{
    int sum = 0;

    for (int reading = 0; reading < kReadingsForMeasurement; reading++)
    {
        sum += 0; // analogRead(voltageReadPin); // TODO
    }

    sum /= kReadingsForMeasurement;
    return sum;
}

bool PowerControl::isLowVoltage()
{
    int measurment = measure();

#ifdef LOW_VOLTAGE_DETECTION_TEST
    LOGF("Voltage: %0.2f V\n",
         kResistorDivider * kMeasurmentVoltage * (float)measurment / maxVolgateValue);
#endif

    return measurment < threshold;
}
