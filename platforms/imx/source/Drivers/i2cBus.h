#ifndef SOURCE_I2CBUS_H_
#define SOURCE_I2CBUS_H_

#include "TimeControl.h"

#include <vector>
#include <stdio.h>

class I2cDevice;
class I2cBusController;

class I2cBus
{
  public:
    void init(I2cBusController* busController);
    void update();
    // call this function before init device
    void addDevice(I2cDevice* device);
    void reinitBus();
    void unblockBus();

  private:
    static constexpr size_t kMaxDevices = 4;
    static constexpr uint32_t kRefreshTimeUs = 500;
    static constexpr uint32_t kTimeoutUs = 10000;
    I2cBusController* busController;
    std::vector<I2cDevice*> devices;
    uint32_t updateTime;
    uint32_t timeoutTime;
    uint8_t updateDeviceIdx;
};

extern I2cBus i2cBus4;
extern I2cBus i2cBus2;

#endif /* SOURCE_I2CBUS_H_ */
