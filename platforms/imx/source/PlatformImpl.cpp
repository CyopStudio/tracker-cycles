#include "PlatformImpl.h"
#include "polyassert.h"

#ifdef DEBUG
#define DEBUG_TEXT " debug"
#else
#define DEBUG_TEXT
#endif

PlatformImpl::SystemInfo PlatformImpl::systemInfo = { BUILD_ID DEBUG_TEXT };

extern void disableHardware();

void PlatformImpl::init()
{
    uartMidi.init();
    fileSystem.init();
    configManager.init(&fileSystem);
}

void PlatformImpl::update()
{
    fileSystem.update();
}

void PlatformImpl::setPowerMode(bool mode)
{
    if (!mode)
    {
        disableHardware();
        powerControl.turnPowerOff();
    }
}

bool PlatformImpl::getPowerMode()
{
    return true;
}

PlatformImpl::PowerButtonCallback PlatformImpl::getPowerButtonCallback()
{
    return powerButtonCallback;
}

void PlatformImpl::resetMCU()
{

    getDisplayDriver()->setPowerState(false);

    __NVIC_SystemReset();

    for (;;)
    {
        __asm volatile("nop");
    }
}

void PlatformImpl::runBootloader(char* fileName)
{
    if (fileSystem.exists(kFirmwareInfoFilePath))
    {
        fileSystem.remove(kFirmwareInfoFilePath);
    }

    auto [isOpen, fileHandle] =
      fileSystem.openFile(kFirmwareInfoFilePath, FileSystem::FileOpenMode::kWriteCreateNew);

    if (!isOpen)
    {
        return;
    }

    fileSystem.writeFile(fileName, strlen(fileName), fileHandle);
    fileSystem.closeFile(fileHandle);

    constexpr char kTurnOnFileData[] = "start";
    auto [isOpen2, fileHandle2] =
      fileSystem.openFile(kTurnOnFilePath, FileSystem::FileOpenMode::kWriteCreateNew);

    if (isOpen2)
    {
        fileSystem.writeFile(kTurnOnFileData, strlen(kTurnOnFileData), fileHandle2);
        fileSystem.closeFile(fileHandle2);
    }

    resetMCU();
}

bool PlatformImpl::readForceStartUp()
{
    fileSystem.init();

    if (fileSystem.exists(kTurnOnFilePath))
    {
        fileSystem.remove(kTurnOnFilePath);
        return true;
    }

    return false;
}

uint8_t PlatformImpl::readStartState()
{
    uint8_t startState = 1;

    return startState;
}

void PlatformImpl::saveStartState(uint8_t state)
{
}

void PlatformImpl::setPowerButtonCallback(void (*powerButtonChangeFunc)(int))
{
    this->powerButtonCallback = powerButtonChangeFunc;
}

bool PlatformImpl::getPowerButtonState()
{
    return powerControl.getPowerButtonState();
}

Sgtl5000* PlatformImpl::getCodecAudio()
{
    return &codec;
}

Radio* PlatformImpl::getRadio()
{
    return (Radio*)&radio;
}

SwitchGroup* PlatformImpl::getButtons()
{
    return &buttons;
}

SwitchGroup* PlatformImpl::getPads()
{
    return &pads;
}

Leds* PlatformImpl::getLeds()
{
    return (Leds*)&leds;
}

Battery* PlatformImpl::getBattery()
{
    return (Battery*)&battery;
}

Encoder* PlatformImpl::getEncoder()
{
    return (Encoder*)&encoder;
}

Timer* PlatformImpl::getTimer()
{
    return (Timer*)&timer;
}

DisplayDriver* PlatformImpl::getDisplayDriver()
{
    return (DisplayDriver*)&displayDriver;
}

UartMidi* PlatformImpl::getUartMidi()
{
    return (UartMidi*)&uartMidi;
}

UsbMidi* PlatformImpl::getUsbMidi()
{
    return (UsbMidi*)&usbMidi;
}

UsbAudio* PlatformImpl::getUsbAudio()
{
    return (UsbAudio*)&usbAudio;
}

ConfigManager* PlatformImpl::getConfigManager()
{
    return (ConfigManager*)&configManager;
}

FileSystem* PlatformImpl::getFileSystem()
{
    return (FileSystem*)&fileSystem;
}

CrcCalculator* PlatformImpl::getCrcCalculator()
{
    return &crcCalculator;
}

PlatformImpl::SystemInfo* PlatformImpl::getSystemInfo()
{
    return &systemInfo;
}

Usb* PlatformImpl::getUsb()
{
    return (Usb*)&usb;
}
