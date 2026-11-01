#ifndef SOURCE_BATTERYIMPL_H_
#define SOURCE_BATTERYIMPL_H_

#include "Platform.h"
#include <stdint.h>

class I2cDevice;

class BatteryImpl : public Battery
{
  public:
    virtual void enableMonitoring() override;
    virtual int getLevel() override;
    virtual float getVoltage() override;
    virtual bool getCharging() override;
    virtual int getTimeTo() override;
    virtual bool getState() override { return (initialized && monitoring); }
    virtual bool isEmpty() override;

    bool begin();
    void update();

    void disableMonitoring();

    void setChargingAction(void (*funct)(void));
    void setLowBatteryAction(void (*funct)(void));
    void setEmptyBatteryAction(void (*funct)(void));

    I2cDevice* getDriverI2c();

  private:
    // design capacity of battery in mAh.
    static constexpr uint16_t kBatteryCapacity = 4310;
    // lowest operational voltage in mV
    static constexpr uint16_t kTerminationVoltage = 3000;
    // end of charge current in mA
    static constexpr uint16_t kTerminationCurrent = 150; // 10% of charging current

    static constexpr uint32_t kBatteryMonitoringIntervalMs = 1000;
    static constexpr int kBatteryLowLevelThreshold = 20;

    bool readCharging();
    float readVoltage();
    void readLevel();

    bool initialized = false;
    bool monitoring = false;
    bool last_charging_state = false;

    bool low_battery_state = false;
    bool empty_battery_state = false;

    int16_t last_pre_level_read = 250;
    int8_t last_level_read = 50;
    float last_voltage_read = 0.0f;
    void (*chargingAction)(void);
    void (*lowBatteryAction)(void);
    void (*emptyBatteryAction)(void);

    uint32_t intervalTimeMs;

    int8_t lastLevelPrinted = 0;
    uint32_t lastTimePrinted = 0;
};

#endif /* SOURCE_BATTERYIMPL_H_ */
