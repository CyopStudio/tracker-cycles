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
    virtual OperationResult sendData(uint8_t slaveAddress,
                                     uint32_t reg,
                                     uint32_t regSize,
                                     void* data,
                                     uint32_t dataSize) = 0;
    virtual OperationResult receiveData(uint8_t slaveAddress,
                                        uint32_t reg,
                                        uint32_t regSize,
                                        void* data,
                                        uint32_t dataSize) = 0;
    // switch to gpio mode and force several clock cycles
    virtual void unblockBus() = 0;
};

#endif /* SOURCE_I2CBUSCONTROLLER_H_ */
