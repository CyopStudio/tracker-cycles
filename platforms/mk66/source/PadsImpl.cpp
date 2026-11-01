#include "PadsImpl.h"
#include "i2cBusController.h"

PadsImpl* PadsImpl::instance = nullptr;

const uint8_t PadsImpl::convertToGridKey4x12[80] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,  9,  12,  13,  14,  15,  16,  17,  18,  19,  20, 21,
    24,  25,  26,  27,  28,  29,  30,  31,  32, 33, 36,  37,  38,  39,  40,  41,  42,  43,  44, 45,
    255, 255, 255, 255, 255, 255, 255, 255, 11, 10, 255, 255, 255, 255, 255, 255, 255, 255, 23, 22,
    255, 255, 255, 255, 255, 255, 255, 255, 35, 34, 255, 255, 255, 255, 255, 255, 255, 255, 47, 46
};

void PadsImpl::init(int interruptPin, I2cBusController* i2cController, void (*resti2c)(void))
{
    instance = this;
    uint8_t config = Keys::KE_IEN | Keys::OVR_FLOW_IEN | Keys::INT_CFG | Keys::OVR_FLOW_M;
    buttonsDriver.begin(255, 1023, config, i2cController, (uint8_t*)convertToGridKey4x12, resti2c);
    buttonsDriver.enableInterrupt(interruptPin, handleInterrupt);
}

void PadsImpl::handleInterrupt()
{
    instance->buttonsDriver.keyInt = 1;
}
