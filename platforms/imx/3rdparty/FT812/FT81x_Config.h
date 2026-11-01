
#ifndef CONFIGS_H_
#define CONFIGS_H_

#include <stdint.h>

// LCD display parameters
struct Ft81xConfig
{
    uint16_t lcdWidth;   // Active width of LCD display
    uint16_t lcdHeight;  // Active height of LCD display
    uint16_t lcdHcycle;  // Total number of clocks per line
    uint16_t lcdHoffset; // Start of active line
    uint16_t lcdHsync0;  // Start of horizontal sync pulse
    uint16_t lcdHsync1;  // End of horizontal sync pulse
    uint16_t lcdVcycle;  // Total number of lines per screen
    uint16_t lcdVoffset; // Start of active screen
    uint16_t lcdVsync0;  // Start of vertical sync pulse
    uint16_t lcdVsync1;  // End of vertical sync pulse
    uint8_t lcdPclk;     // Pixel Clock
    uint8_t lcdSwizzle;  // Define RGB output pins
    uint8_t lcdPclkpol;  // Define active edge of PCLK
    uint8_t lcdCsSpread;
    uint8_t lcdRotate;
    uint8_t lcdDither;
    uint16_t outBits;
};

// WF52ATLASDNN0 display parameters
const Ft81xConfig kConfig480_128 = {

    .lcdWidth = 480,  // Active width of LCD display
    .lcdHeight = 128, // Active height of LCD display

    .lcdHcycle = 531, // Total number of clocks per line
    .lcdHoffset = 51, // Start of active line
    .lcdHsync0 = 8,   // Start of horizontal sync pulse
    .lcdHsync1 = 12,  // End of horizontal sync pulse

    .lcdVcycle = 292, // Total number of lines per screen
    .lcdVoffset = 91, // Start of active screen
    .lcdVsync0 = 8,   // Start of vertical sync pulse
    .lcdVsync1 = 12,  // End of vertical sync pulse

    .lcdPclk = 6,    // Pixel Clock
    .lcdSwizzle = 0, // Define RGB output pins
    .lcdPclkpol = 1, // Define active edge of PCLK
    .lcdCsSpread = 1,
    .lcdRotate = 0,
    .lcdDither = 1,
    .outBits = 0,
};

// WF70A2TIAGDNN0 display parameters
const Ft81xConfig kConfig800_480 = {

    .lcdWidth = 800,  // Active width of LCD display
    .lcdHeight = 480, // Active height of LCD display

    .lcdHcycle = 1056, // Total number of clocks per line
    .lcdHoffset = 46,  // Start of active line
    .lcdHsync0 = 0,    // Start of horizontal sync pulse
    .lcdHsync1 = 10,   // End of horizontal sync pulse

    .lcdVcycle = 525, // Total number of lines per screen
    .lcdVoffset = 23, // Start of active screen
    .lcdVsync0 = 0,   // Start of vertical sync pulse
    .lcdVsync1 = 10,  // End of vertical sync pulse

    .lcdPclk = 2,    // Pixel Clock
    .lcdSwizzle = 0, // Define RGB output pins
    .lcdPclkpol = 1, // Define active edge of PCLK

    .lcdCsSpread = 0,
    .lcdRotate = 1,
    .lcdDither = 1,
    .outBits = 0x1B6
};

// DLC0283BDP04DF-4
const Ft81xConfig kConfig240_320 = {

    .lcdWidth = 240,  // Active width of LCD display
    .lcdHeight = 320, // Active height of LCD display

    .lcdHcycle = 280, // Total number of clocks per line
    .lcdHoffset = 30, // Start of active line
    .lcdHsync0 = 0,   // Start of horizontal sync pulse
    .lcdHsync1 = 10,  // End of horizontal sync pulse

    .lcdVcycle = 328, // Total number of lines per screen
    .lcdVoffset = 4,  // Start of active screen
    .lcdVsync0 = 0,   // Start of vertical sync pulse
    .lcdVsync1 = 4,   // End of vertical sync pulse

    .lcdPclk = 9,    // Pixel Clock
    .lcdSwizzle = 2, // Define RGB output pins
    .lcdPclkpol = 0, // Define active edge of PCLK

    .lcdCsSpread = 0,
    .lcdRotate = 6,

    .lcdDither = 1,
    .outBits = 0,
};

#endif /* CONFIGS_H_ */
