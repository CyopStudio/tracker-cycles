#include "ButtonsImpl.h"
#include "i2cBusController.h"

ButtonsImpl* ButtonsImpl::instance;
#ifndef TRACKER_MINI
const uint8_t ButtonsImpl::convertToControlButtons[80] = {
    8,   9,   10,  11,  12,  255, 255, 255, 255, 255, 13,  14,  15,  16,  17,  255,
    255, 255, 255, 255, 18,  19,  20,  21,  22,  255, 255, 255, 255, 255, 23,  24,
    25,  26,  27,  255, 255, 255, 255, 255, 28,  29,  30,  31,  32,  255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 7,   6,   5,   4,   3,   2,   1,   0,   255, 255
};
#else
const uint8_t ButtonsImpl::convertToControlButtons[80] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 18, 19, 20, 21, 16, 17, 18, 19,
    20, 26, 32, 23, 28, 25, 39, 34, 28, 29, 30, 31, 33, 25, 34, 35, 40, 37, 38, 39,
    40, 41, 30, 27, 24, 45, 41, 47, 48, 49, 50, 51, 31, 36, 37, 38, 42, 57, 58, 59,
    60, 61, 35, 29, 29, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79
};
#endif
// interrupt pin and interrupt is configured using configuration tool (mex file)
void ButtonsImpl::init()
{
    instance = this;
#ifndef TRACKER_MINI
    uint8_t config = Keys::KE_IEN | Keys::OVR_FLOW_IEN | Keys::INT_CFG | Keys::OVR_FLOW_M;
    buttonsDriver.begin(255, 511, config, (uint8_t*)convertToControlButtons);
#else
    uint8_t config = (Keys::KE_IEN | Keys::OVR_FLOW_IEN | Keys::INT_CFG | Keys::OVR_FLOW_M);
    buttonsDriver.begin(127, 255, config, (uint8_t*)convertToControlButtons);
#endif
    buttonsDriver.enableInterrupt(handleInterrupt, Keys::KeyType::kButtons);
}

void ButtonsImpl::handleInterrupt()
{
    instance->buttonsDriver.keyInt = 1;
}
