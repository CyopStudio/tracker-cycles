#include "MAX17262.h"
#include "WString.h"
#include "usb_serial.h"
#include <BatteryImpl.h>

// design capacity of battery in mAh.
const uint16_t BATTERY_CAPACITY = 4310;
// lowest operational voltage in mV
const uint16_t TERMINATE_VOLTAGE = 3000;
const uint8_t GPOUT_PIN = 3;

MAX17262 gauge;

void print_binary(uint16_t number);

//#define BATTERY_DEBUG

//------------------------------------------------------------------------
// return true if battery is empty and not charging
bool BatteryImpl::begin()
{
    pinMode(GPOUT_PIN, INPUT);

    if (!gauge.begin(BATTERY_CAPACITY, TERMINATE_VOLTAGE))
    {
#ifdef BATTERY_DEBUG
        // If communication fails, print an error message and loop forever.
        Serial.println("Error: Unable to communicate with battery driver.");
        Serial.println("  Check wiring and try again.");
#endif
        return false;
    }
#ifdef BATTERY_DEBUG
    Serial.println("Connected to battery driver!");
#endif

    readLevel();

    low_battery_state = false;
    empty_battery_state = false;

    initialized = true;
    monitoring = false;

    last_charging_state = readCharging();

    if (isEmpty())
    {
        return true;
    }

    return false;
}

void printBatteryStats()
{
    // Read battery stats from the BQ27441-G1A
    unsigned int soc = gauge.getSOC();       // Read state-of-charge (%)
    unsigned int volts = gauge.getVoltage(); // Read battery voltage (mV)
    int current = gauge.getCurrent();        // Read  current (mA)
    unsigned int emptyVoltage = gauge.getVEmpty();
    unsigned int recoveryVoltage = gauge.getVRecovery();
    unsigned int fullCap = gauge.getFullCapacity();
    unsigned int capacity = gauge.getCurrentCapacity();
    int timeMin = gauge.getTimeToEmpty();

    // Assemble a string to print
    String toPrint = "[" + String(millis() / 1000) + "] ";
    toPrint += String(soc) + "% | ";
    toPrint += String(volts) + " mV | ";
    toPrint += String(current) + " mA | ";
    toPrint += String(emptyVoltage) + " mV | ";
    toPrint += String(recoveryVoltage) + " mV | ";
    toPrint += String(capacity) + " mAh | ";
    toPrint += String(fullCap) + " mAh | ";
    toPrint += String(timeMin) + " min | ";

    // battery is charging
    if (gauge.isCharging())
        toPrint += " CHARG";

    // Print the string
    Serial.print(toPrint);

    Serial.print(" ");
    print_binary(gauge.getStatus());

    Serial.println();
}

void print_binary(uint16_t number)
{
    for (int i = 15; i >= 0; i--)
    {
        Serial.print((number >> i) & 1 ? '1' : '0');
    }
}

// cykliczne sprawdzanie bateri
// wywoluje akcje interfejsu dotyczace baterii
void BatteryImpl::update()
{
    if (!initialized || !monitoring)
        return;

    if (millis() - intervalTimeMs < 1000)
    {
        return;
    }

    intervalTimeMs = millis();

    if (!gauge.verify())
        return;

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
        if (last_level_read < 20 && !low_battery_state)
        {
            low_battery_state = true;
            if (lowBatteryAction != nullptr)
                lowBatteryAction();
        }

        if (last_voltage_read <= (TERMINATE_VOLTAGE / 1000.0) && !empty_battery_state)
        {
            empty_battery_state = true;
            if (emptyBatteryAction != nullptr)
                emptyBatteryAction();
        }
    }
    else
    {
        if (last_level_read > 21 && low_battery_state)
        {
            low_battery_state = false;
        }
    }
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
    if (initialized)
        monitoring = true;
}

void BatteryImpl::disableMonitoring()
{
    if (initialized)
        monitoring = false;
}

bool BatteryImpl::isEmpty()
{
    return (last_voltage_read * 1000) <= (TERMINATE_VOLTAGE + (last_charging_state ? 300 : 0));
}
