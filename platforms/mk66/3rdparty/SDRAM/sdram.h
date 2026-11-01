
#ifndef _SDRAM_H_
#define _SDRAM_H_

#include <cr_section_macros.h>

#define EXAMPLE_SDRAMC SDRAM
#define SDRAM_START_ADDRESS (0x70000000U)
#define BUS_CLK_FREQ CLOCK_60GetFreq(kCLOCK_FlexBusClk)

#define SDRAM_EXAMPLE_DATALEN 579
#define SDRAM_EXAMPLE_WRITETIMES (1000U)

int Extern_SDRAM_Init(void);
void BOARD_DeinitPins(void);
void BOARD_InitPins(void);
void Extern_SDRAM_SetLowPower(uint8_t enable);

#endif
