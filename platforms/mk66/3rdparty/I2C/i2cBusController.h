#ifndef SOURCE_I2CBUSCONTROLLER_H_
#define SOURCE_I2CBUSCONTROLLER_H_

#include <stdio.h>
#include <stdint.h>

class I2cBusController
{
  public:
    enum OperationResult
    {
        kOK,
        kError
    };

    virtual void init() = 0;
    virtual void setTimeout(uint32_t timeoutUs) = 0;
    virtual bool isBusIdle() = 0;
    virtual bool startTransmission(uint8_t address) = 0;
    virtual void sendByte(uint8_t data) = 0;
    virtual OperationResult finishTransmission(bool sendStop = true, int* error = nullptr) = 0;
    virtual OperationResult sendTransmission(bool sendStop = true) = 0;
    virtual OperationResult request(uint8_t address, size_t size) = 0;
    virtual uint8_t read() = 0;
    virtual bool isDataToReceive() = 0;
    // switch to gpio mode and force several clock cycles
    virtual void unblockBus() = 0;
};

#endif /* SOURCE_I2CBUSCONTROLLER_H_ */
