#include "RadioImpl.h"
#ifdef TRACKER_MINI
void RadioImpl::enableLineInput(bool)
{
}

void RadioImpl::enable(bool)
{
}

bool RadioImpl::setFrequency(float)
{
    return false;
}

void RadioImpl::setSeekCallback(void (*)(void))
{
}

void RadioImpl::seek(SeekDirection)
{
}

void RadioImpl::updateSeek()
{
}

bool RadioImpl::updateRDS()
{
    return false;
}

float RadioImpl::getFrequency()
{
    return 0.0f;
}

float RadioImpl::getBaseFrequency()
{
    return 0.0f;
}

float RadioImpl::getSpacing()
{
    return 0.0f;
}

bool RadioImpl::isinitialized()
{
    return false;
}

void RadioImpl::clearRDS()
{
}

void RadioImpl::setRegion(Region)
{
}

bool RadioImpl::isRegionChanged()
{
    return false;
}

char* RadioImpl::getStationName()
{
    return nullptr;
}

#else

void RadioImpl::enableLineInput(bool enabled)
{
    if (enabled)
    {
        GPIO_PinWrite(lineInSourceSelectBase, lineInSourceSelectPin, 0);
    }
    else
    {
        GPIO_PinWrite(lineInSourceSelectBase, lineInSourceSelectPin, 1);
    }
}

void RadioImpl::enable(bool enabled)
{
    if (enabled)
    {
        radioDriver.powerOn();
    }
    else
    {
        radioDriver.powerOff();
    }
}

bool RadioImpl::setFrequency(float freq)
{
    return radioDriver.setFrequency(freq);
}

void RadioImpl::setSeekCallback(void (*seekCallback)(void))
{
    if (seekCallback == nullptr)
    {
        radioDriver.resetSeekCallback();
        return;
    }

    radioDriver.setSeekCallback(seekCallback);
}

void RadioImpl::seek(SeekDirection direction)
{
    if (direction == SeekDirection::kDown)
    {
        radioDriver.seekDown();
    }
    else if (direction == SeekDirection::kUp)
    {
        radioDriver.seekUp();
    }
}

void RadioImpl::updateSeek()
{
    radioDriver.stateMachineSeek();
}

bool RadioImpl::updateRDS()
{
    return radioDriver.update_RDS();
}

float RadioImpl::getFrequency()
{
    return radioDriver.getFrequency();
}

float RadioImpl::getBaseFrequency()
{
    return radioDriver.getRadioBottomBase();
}

float RadioImpl::getSpacing()
{
    return radioDriver.getRadioSpacing();
}

bool RadioImpl::isinitialized()
{
    return (bool)radioDriver.getInitializationStatus();
}

void RadioImpl::clearRDS()
{
    radioDriver.clearRDS();
}

void RadioImpl::setRegion(Region region)
{

    radioDriver.setRegion((radio_region_t)region, 1);
}

bool RadioImpl::isRegionChanged()
{
    return radioDriver.hasRegionChanged();
}

char* RadioImpl::getStationName()
{
    return radioDriver.rds_data.nazwaStacji;
}

void RadioImpl::init(Pins* pins)
{
    RadioImpl::lineInSourceSelectBase = pins->lineInSourceSelectBase;
    RadioImpl::lineInSourceSelectPin = pins->lineInSourceSelectPin;

    GPIO_PinWrite(lineInSourceSelectBase, lineInSourceSelectPin, 0);

    Si4703::Pins driverPins;
    driverPins.resetBase = pins->resetBase;
    driverPins.resetPin = pins->resetPin;
    driverPins.sdioBase = pins->i2cDataBase;
    driverPins.sdioPin = pins->i2cDataPin;
    driverPins.sdioMux = pins->i2cDataMux;

    radioDriver.init(&driverPins);
}

I2cDevice* RadioImpl::getI2cDevice()
{
    return &radioDriver;
}
#endif
