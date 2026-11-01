#include "MAX17262.h"
#include "PlatformImpl.h"
#include "TimeControl.h"
#include "log.h"
#include <BatteryImpl.h>
#include <string>

MAX17262 gauge;

void print_binary(uint16_t number);

//#define BATTERY_DEBUG
//#define BATTERY_SD_LOG

extern PlatformImpl platform;

//------------------------------------------------------------------------
// returns true if battery is empty and not charging
bool BatteryImpl::begin()
{
    if (!gauge.begin(kBatteryCapacity, kTerminationVoltage, kTerminationCurrent))
    {
#ifdef BATTERY_DEBUG
        LOG("Error: Unable to communicate with battery driver.");
        LOG("       Check wiring and try again.");
#endif
        return false;
    }
#ifdef BATTERY_DEBUG
    LOG("Connected to battery driver!");
#endif

    readLevel();

    low_battery_state = false;
    empty_battery_state = false;

    initialized = true;

    last_charging_state = readCharging();

    if (isEmpty())
    {
        return true;
    }

    return false;
}

void printBatteryStats()
{
    unsigned int soc = gauge.getSOC();
    unsigned int volts = gauge.getVoltage();
    int current = gauge.getCurrent();
    unsigned int emptyVoltage = gauge.getVEmpty();
    unsigned int recoveryVoltage = gauge.getVRecovery();
    unsigned int fullCap = gauge.getFullCapacity();
    unsigned int capacity = gauge.getCurrentCapacity();
    int timeMin = gauge.getTimeToEmpty();

    std::string toPrint = "[" + std::to_string(millis() / 1000) + "] ";
    toPrint += std::to_string(soc) + "% | ";
    toPrint += std::to_string(volts) + " mV | ";
    toPrint += std::to_string(current) + " mA | ";
    toPrint += std::to_string(emptyVoltage) + " mV | ";
    toPrint += std::to_string(recoveryVoltage) + " mV | ";
    toPrint += std::to_string(capacity) + " mAh | ";
    toPrint += std::to_string(fullCap) + " mAh | ";
    toPrint += std::to_string(timeMin) + " min | ";

    if (gauge.isCharging())
    {
        toPrint += " CHARG";
    }

    toPrint += "\n";

    LOG(toPrint.c_str());
    LOGF(" ");
    print_binary(gauge.getStatus());
    LOG("");
}

void print_binary(uint16_t number)
{
    for (int i = 15; i >= 0; i--)
    {
        LOGF("%c", (number >> i) & 1 ? '1' : '0');
    }
}

void BatteryImpl::update()
{
    if (millis() - intervalTimeMs < kBatteryMonitoringIntervalMs)
    {
        return;
    }
    intervalTimeMs = millis();

    if (!initialized)
    {
        begin();
        return;
    }

    if (!monitoring)
    {
        return;
    }

    if (!gauge.verify())
    {
        initialized = false;
        return;
    }

#ifdef BATTERY_DEBUG
    printBatteryStats();
#endif

    readLevel();

    bool temp_charging_state = readCharging();
    if (temp_charging_state != last_charging_state)
    {
        last_charging_state = temp_charging_state;
        if (last_charging_state && chargingAction != nullptr)
            chargingAction();
    }

    if (!last_charging_state)
    {
        if (last_level_read < kBatteryLowLevelThreshold && !low_battery_state)
        {
            low_battery_state = true;
            if (lowBatteryAction != nullptr)
                lowBatteryAction();
        }

        if (last_voltage_read <= (kTerminationVoltage / 1000.0f) && !empty_battery_state)
        {
            empty_battery_state = true;
            if (emptyBatteryAction != nullptr)
                emptyBatteryAction();
        }
    }
    else
    {
        if (last_level_read > kBatteryLowLevelThreshold + 1 && low_battery_state)
        {
            low_battery_state = false;
        }
    }

#ifdef BATTERY_SD_LOG

    bool timeToPrint = lastTimePrinted + 300000 < millis();

    if (last_level_read != lastLevelPrinted || timeToPrint)
    {
        lastTimePrinted = millis();

        auto [isOpen, fileHandle] = platform.getFileSystem()->openFile(
          "battLog.txt", FileSystemImpl::FileOpenMode::kWriteAppend, nullptr);

        if (isOpen)
        {
            char buffer[100];
            snprintf(buffer,
                     100,
                     "%d \% | %f mV | %d mA | %d sec | %d mAh | %d mAh | %s %s\n",
                     last_level_read,
                     last_voltage_read,
                     gauge.getCurrent(),
                     millis() / 1000,
                     gauge.getCurrentCapacity(),
                     gauge.getFullCapacity(),
                     last_charging_state ? " ch" : "",
                     timeToPrint ? "- timer" : "");

            platform.getFileSystem()->writeFile(buffer, strlen(buffer), fileHandle);
            platform.getFileSystem()->closeFile(fileHandle);
        }

        lastLevelPrinted = last_level_read;
    }

#endif
}

//------------------------------------------------------------------------
void BatteryImpl::setChargingAction(void (*funct)(void))
{
    chargingAction = funct;
}

void BatteryImpl::setLowBatteryAction(void (*funct)(void))
{
    lowBatteryAction = funct;
}

void BatteryImpl::setEmptyBatteryAction(void (*funct)(void))
{
    emptyBatteryAction = funct;
}

I2cDevice* BatteryImpl::getDriverI2c()
{
    return &gauge;
}

//------------------------------------------------------------------------
void BatteryImpl::readLevel()
{
    uint16_t lvl = gauge.getSOC();
    last_level_read = lvl > 100 ? 100 : lvl;

    uint16_t voltage = gauge.getVoltage();
    last_voltage_read = (voltage > 5000 ? 5000 : voltage) / 1000.0;
}

bool BatteryImpl::readCharging()
{
    return gauge.isCharging();
}

//------------------------------------------------------------------------

// 0-100
int BatteryImpl::getLevel()
{
    return (int)last_level_read;
}

float BatteryImpl::getVoltage()
{
    return last_voltage_read;
}

bool BatteryImpl::getCharging()
{
    return last_charging_state;
}

int BatteryImpl::getTimeTo()
{
    return last_charging_state ? gauge.getTimeToFull() : gauge.getTimeToEmpty();
}

void BatteryImpl::enableMonitoring()
{
    monitoring = true;
}

void BatteryImpl::disableMonitoring()
{
    monitoring = false;
}

bool BatteryImpl::isEmpty()
{
    return (last_voltage_read * 1000) <= (kTerminationVoltage + (last_charging_state ? 300 : 0));
}
