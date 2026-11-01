#include "i2cBus.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "i2cBusController.h"
#include "i2cDevice.h"
#include "polyassert.h"

I2cBus i2cBus4;
I2cBus i2cBus2;

void I2cBus::init(I2cBusController* busController)
{
    assert(devices.size() == 0);
    assert(busController != nullptr);

    devices.reserve(kMaxDevices);

    this->busController = busController;

    busController->init();
    busController->setTimeout(kTimeoutUs);
}

void I2cBus::update()
{
    assert(devices.size() > 0);
    assert(devices.size() < kMaxDevices);

    if (timeControl.micros() - updateTime > kRefreshTimeUs)
    {
        updateTime = timeControl.micros();

        if (busController->isBusIdle())
        {
            if (!devices[updateDeviceIdx]->update())
            {
                updateDeviceIdx++;
                if (updateDeviceIdx >= devices.size())
                    updateDeviceIdx = 0;
            }

            timeoutTime = timeControl.micros();
        }
        else if (timeControl.micros() - timeoutTime > kTimeoutUs)
        {
            unblockBus();
            reinitBus();
            timeoutTime = timeControl.micros();
        }
    }
}

void I2cBus::addDevice(I2cDevice* device)
{
    devices.push_back(device);
    assert(devices.size() < kMaxDevices);

    device->setController(busController);
}

void I2cBus::reinitBus()
{
    assert(busController != nullptr);
    busController->init();
}

void I2cBus::unblockBus()
{
    assert(busController != nullptr);
    busController->unblockBus();
}
