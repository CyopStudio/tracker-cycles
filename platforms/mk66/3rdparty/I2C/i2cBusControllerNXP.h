#ifndef SOURCE_I2CBUSCONTROLLERNXP_H_
#define SOURCE_I2CBUSCONTROLLERNXP_H_

#include "i2cBusController.h"

class I2cBusControllerImplBase : public I2cBusController
{
  public:
    virtual void setTimeout(uint32_t timeoutUs) override;
    virtual bool isBusIdle() override;
    virtual bool startTransmission(uint8_t address) override;
    virtual inline void sendByte(uint8_t data) { transmitQueue[transmitSize++] = data; }
    virtual OperationResult finishTransmission(bool sendStop = true, int* error = nullptr) override;
    virtual OperationResult sendTransmission(bool sendStop = true) override;
    virtual OperationResult request(uint8_t address, size_t size) override;
    virtual uint8_t read() override;
    virtual bool isDataToReceive() override;

    void reset();

    bool completionFlag = false;

  protected:
    static constexpr int kBufferMax = 256;
    enum I2CStatus
    {
        kIdle,
        kError,
        kStart,
        kSending,
        kReceiving,
    };

    uint32_t timeoutUs;
    int transmitSize = 0;
    int receiveSize = 0;
    int dataToReceiveIdx = 0;
    uint8_t transmitQueue[kBufferMax];
    uint8_t receiveQueue[kBufferMax];
    uint8_t currentSlaveAddress;
    volatile I2CStatus status;

    void* handle;

    void* baseAddr = nullptr;
};

class I2cBusControllerImpl0 : public I2cBusControllerImplBase
{
  public:
    virtual void init() override;
    virtual void unblockBus() override;
};

class I2cBusControllerImpl2 : public I2cBusControllerImplBase
{
  public:
    virtual void init() override;
    virtual void unblockBus() override;
};

extern I2cBusControllerImpl0 i2cBusControllerNXP0;
extern I2cBusControllerImpl2 i2cBusControllerNXP2;

#endif /* SOURCE_I2CBUSCONTROLLERNXP_H_ */
