#ifndef SOURCE_BATTERYIMPL_H_
#define SOURCE_BATTERYIMPL_H_

#include "Platform.h"
#include "core_pins.h"
#include <stdint.h>

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

  private:
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
};

#endif /* SOURCE_BATTERYIMPL_H_ */
