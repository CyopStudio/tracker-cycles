
#ifndef PINOUT_H
#define PINOUT_H

#include <stdint.h>

// https://www.pjrc.com/teensy/schematic.html + teensy_core/core_pins.h
// Teensy 3.6 pin's numbers converted to Kinetis K66 pin's names
enum KinetisK66Pinout
{
    kPTB16 = 0,
    kPTB17 = 1,
    kPTD0 = 2,
    kPTA12 = 3,
    kPTA13 = 4,
    kPTD7 = 5,
    kPTD4 = 6,
    kPTD2 = 7,
    kPTD3 = 8,
    kPTC3 = 9,
    kPTC4 = 10,
    kPTC6 = 11,
    kPTC7 = 12,
    kPTC5 = 13,
    kPTD1 = 14,
    kPTC0 = 15,
    kPTB0 = 16,
    kPTB1 = 17,
    kPTB3 = 18,
    kPTB2 = 19,
    kPTD5 = 20,
    kPTD6 = 21,
    kPTC1 = 22,
    kPTC2 = 23,
    kPTE26 = 24,
    kPTA5 = 25,
    kPTA14 = 26,
    kPTA15 = 27,
    kPTA16 = 28,
    kPTB18 = 29,
    kPTB19 = 30,
    kPTB10 = 31,
    kPTB11 = 32,
    kPTE24 = 33,
    kPTE25 = 34,
    kPTC8 = 35,
    kPTC9 = 36,
    kPTC10 = 37,
    kPTC11 = 38,
    kPTA17 = 39,
    kPTA28 = 40,
    kPTA29 = 41,
    kPTA26 = 42,
    kPTB20 = 43,
    kPTB22 = 44,
    kPTB23 = 45,
    kPTB21 = 46,
    kPTD8 = 47,
    kPTD9 = 48,
    kPTB4 = 49,
    kPTB5 = 50,
    kPTD14 = 51,
    kPTD13 = 52,
    kPTD12 = 53,
    kPTD15 = 54,
    kPTD11 = 55,
    kPTE10 = 56,
    kPTE11 = 57,
    kPTE0 = 58,
    kPTE1 = 59,
    kPTE2 = 60,
    kPTE3 = 61,
    kPTE4 = 62,
    kPTE5 = 63,
    kPTE8 = 64,
    kADC1_DP0 = 64,
    kPTE9 = 65,
    kPTA4 = 66,
    kPTE27 = 67,
    kPTE28 = 68,

    kPTC19 = 78,
    kPTA2 = 79,
    kPTA24 = 80,
    kPTA1 = 81,
    kPTD10 = 82,
    kPTA7 = 83,
    kPTA8 = 84,
    kPTA9 = 85,
    kPTC18 = 86,
    kADC1_SE16 = 78

};

// TODO
// please take care to keep pins correct when adding new ones
namespace TrackerPinout
{
constexpr int kExternalRamEnable = KinetisK66Pinout::kPTA24;
constexpr int kBlinkLed = KinetisK66Pinout::kPTC19;

#ifndef TRACKER_MINI
constexpr int kPowerButton = KinetisK66Pinout::kPTC3;
#else
constexpr int kPowerButton = KinetisK66Pinout::kPTA7;
constexpr int kPowerReset = KinetisK66Pinout::kPTA8;
constexpr int kPowerSet = KinetisK66Pinout::kPTA9;
#endif

#ifndef TRACKER_MINI
constexpr int kAudioOutputEnable = KinetisK66Pinout::kPTD6;
#else
constexpr int kAudioOutputEnable = KinetisK66Pinout::kPTC18;
#endif

#ifndef TRACKER_MINI
constexpr int kRadioReset = KinetisK66Pinout::kPTE27;
constexpr int kRadioInputKey = KinetisK66Pinout::kPTE26;
constexpr int kRadioInterrupt = KinetisK66Pinout::kPTE28;
#else
constexpr int kRadioReset = KinetisK66Pinout::kPTE26;
constexpr int kRadioInputKey = KinetisK66Pinout::kPTE28;
constexpr int kRadioInterrupt = KinetisK66Pinout::kPTD6;
#endif

constexpr int kI2cData0 = KinetisK66Pinout::kPTD9;
constexpr int kI2cClock0 = KinetisK66Pinout::kPTD8;

#ifndef TRACKER_MINI
constexpr int kButtonsInterrupt = KinetisK66Pinout::kPTD1;
#else
constexpr int kButtonsInterrupt = KinetisK66Pinout::kPTD10;
#endif

#ifndef TRACKER_MINI
constexpr int kPadsInterrupt = KinetisK66Pinout::kPTC11;
#endif

constexpr int kBootloaderTrig = KinetisK66Pinout::kPTA1;

constexpr int kSdCardDetect = KinetisK66Pinout::kPTD0;
}

#endif
