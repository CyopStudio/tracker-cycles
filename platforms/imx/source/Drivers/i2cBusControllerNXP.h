#ifndef SOURCE_I2CBUSCONTROLLERNXP_H_
#define SOURCE_I2CBUSCONTROLLERNXP_H_

#include "i2cBusController.h"

class I2cBusControllerImplBase : public I2cBusController
{
  public:
    virtual void setTimeout(uint32_t timeoutUs) override;
    virtual bool isBusIdle() override;
    virtual OperationResult sendData(uint8_t slaveAddress,
                                     uint32_t reg,
                                     uint32_t regSize,
                                     void* data,
                                     uint32_t dataSize) override;
    virtual OperationResult receiveData(uint8_t slaveAddress,
                                        uint32_t reg,
                                        uint32_t regSize,
                                        void* data,
                                        uint32_t dataSize) override;

  protected:
    uint32_t timeoutUs;

    void* baseAddr = nullptr;
};

class I2cBusControllerImpl4 : public I2cBusControllerImplBase
{
    virtual void init() override;
    virtual void unblockBus() override;
};

class I2cBusControllerImpl2 : public I2cBusControllerImplBase
{
    virtual void init() override;
    virtual void unblockBus() override;
};

extern I2cBusControllerImpl4 i2cBusControllerNXP4;
extern I2cBusControllerImpl2 i2cBusControllerNXP2;

#endif /* SOURCE_I2CBUSCONTROLLERNXP_H_ */
