#include "AudioHardware.h"
#include "Drivers/SpiBus.h"
#include "ExternalSdram.h"
#include "FT812.h"
#include "PlatformImpl.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "i2cBus.h"
#include "i2cBusControllerNXP.h"
#include "log.h"
#include "pin_mux.h"

void onPotChange(uint8_t n, int16_t value);
void onButtonPush(uint8_t n);
void onButtonRelease(uint8_t n);
void onButtonHold(uint8_t n);
void onPadPush(uint8_t n);
void onPadRelease(uint8_t n);
void onPadHold(uint8_t n);
void onBatteryCharging();
void onBatteryEmpty();
void onBatteryLow();
bool isBootloaderResetButtonPressed();

static PlatformImpl* platformImpl;

void initHardware(PlatformImpl* platform,
                  void (*outputFunct)(void*, void*),
                  void (*inputFunct)(void*),
                  void (*startDisplayFunc)(Platform*))
{
    platformImpl = platform;

    platform->getPowerControl()->init(BOARD_INITPINS_POWER_SET_PIN,
                                      BOARD_INITPINS_POWER_SET_GPIO,
                                      BOARD_INITPINS_POWER_RESET_PIN,
                                      BOARD_INITPINS_POWER_RESET_GPIO,
                                      BOARD_INITPINS_POWER_BUTTON_PIN,
                                      BOARD_INITPINS_POWER_BUTTON_GPIO,
                                      0);

    spiBus4.init(LPSPI4);

    DisplayDriverImpl* displayDriver = (DisplayDriverImpl*)platform->getDisplayDriver();
    displayDriver->init(&spiBus4);

    i2cBus4.init(&i2cBusControllerNXP4);

#ifdef TRACKER_MINI
    BatteryImpl* battery = (BatteryImpl*)platform->getBattery();
    i2cBus4.addDevice(battery->getDriverI2c());

    bool batteryConnectedAndEmpty = battery->begin();
    bool batteryCharging = battery->getCharging();

    if (batteryConnectedAndEmpty && !batteryCharging)
    {
        platform->setPowerMode(false);
        return;
    }

    battery->setChargingAction(onBatteryCharging);
    battery->setEmptyBatteryAction(onBatteryEmpty);
    battery->setLowBatteryAction(onBatteryLow);
#endif

    // handle quick activation of display, with logo shown
    // requires the display and battery(only in Mini) drivers to be activated in advance
    if (startDisplayFunc != nullptr)
    {
        startDisplayFunc(platform);
    }
    i2cBus2.init(&i2cBusControllerNXP2);

#ifndef TRACKER_MINI
    RadioImpl* radio = (RadioImpl*)platform->getRadio();

    i2cBus2.addDevice(radio->getI2cDevice());

    RadioImpl::Pins pins;
    pins.i2cDataBase = GPIO2;
    pins.i2cDataPin = 5;
    pins.i2cDataMux = { IOMUXC_GPIO_B0_05_GPIO2_IO05 };
    pins.lineInSourceSelectBase = BOARD_INITPINS_LINE_IN_SOURCE_SELECT_PERIPHERAL;
    pins.lineInSourceSelectPin = BOARD_INITPINS_LINE_IN_SOURCE_SELECT_CHANNEL;
    pins.resetBase = BOARD_INITPINS_RADIO_RESET_PERIPHERAL;
    pins.resetPin = BOARD_INITPINS_RADIO_RESET_CHANNEL;
    radio->init(&pins);
#endif

    ButtonsImpl* buttons = (ButtonsImpl*)platform->getButtons();
    i2cBus2.addDevice(buttons->getButtonsDriverI2c());

    Sgtl5000Impl* codec = (Sgtl5000Impl*)platform->getCodecAudio();
    i2cBus2.addDevice(codec->getDriverI2c());

    audioHardware.init(codec, outputFunct, inputFunct);

    buttons->setActionCallback(ButtonsImpl::ActionType::kPress, onButtonPush);
    buttons->setActionCallback(ButtonsImpl::ActionType::kRelease, onButtonRelease);
    buttons->setActionCallback(ButtonsImpl::ActionType::kHold, onButtonHold);
    buttons->init();

#ifndef TRACKER_MINI
    PadsImpl* pads = (PadsImpl*)platform->getPads();
    i2cBus4.addDevice(pads->getButtonsDriverI2c());

    pads->setActionCallback(PadsImpl::ActionType::kPress, onPadPush);
    pads->setActionCallback(PadsImpl::ActionType::kRelease, onPadRelease);
    pads->setActionCallback(PadsImpl::ActionType::kHold, onPadHold);

    pads->init();

    LedsImpl* leds = (LedsImpl*)platform->getLeds();
    i2cBus4.addDevice(leds->getI2cDevice());

    leds->init();

    EncoderImpl* encoder = (EncoderImpl*)platform->getEncoder();
    encoder->setChangeCallback(onPotChange);
    encoder->init();
#endif
}

void updateHardware(void* unusedParam)
{
    (void)unusedParam;
    i2cBus4.update();
    i2cBus2.update();
    externalRAM.update();
    platformImpl->getPowerControl()->update(platformImpl);
#ifdef TRACKER_MINI
    ((BatteryImpl*)platformImpl->getBattery())->update();
#else
    ((EncoderImpl*)platformImpl->getEncoder())->update();
#endif
    if (isBootloaderResetButtonPressed())
    {
        platformImpl->resetMCU();
    }
}

// startup muting
void muteAudioOutByKey()
{
    audioHardware.enableOutput(false);
}

void unmuteAudioOutByKey()
{
    audioHardware.enableOutput(true);
}

uint8_t isAudioOutMuted()
{
    return !audioHardware.isOutputEnabled();
}

void disableHardware()
{
    platformImpl->getDisplayDriver()->setPowerState(false);
}

bool isBootloaderResetButtonPressed()
{
    return !GPIO_PinRead(BOARD_INITPINS_BOOTLOADER_RESET_GPIO, BOARD_INITPINS_BOOTLOADER_RESET_PIN);
}
