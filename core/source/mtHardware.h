#ifndef MTHARDWARE_H
#define MTHARDWARE_H

#include <stdint.h>

// PAD,POT,BUTTONS MUXS

// control lines (KINETIS VER)
#define MUX_CTRL_A          8 // PTA8			pin-60
#define MUX_CTRL_A_PCR      PORTA_PCR8
#define MUX_CTRL_A_GPIO_SET GPIOA_PSOR
#define MUX_CTRL_A_GPIO_CLR GPIOA_PCOR
#define MUX_CTRL_A_GPIO_DDR GPIOA_PDDR

#define MUX_CTRL_B          9 // PTA9			pin-61
#define MUX_CTRL_B_PCR      PORTA_PCR9
#define MUX_CTRL_B_GPIO_SET GPIOA_PSOR
#define MUX_CTRL_B_GPIO_CLR GPIOA_PCOR
#define MUX_CTRL_B_GPIO_DDR GPIOA_PDDR

#define MUX_CTRL_C          27 // PTA27			pin-78
#define MUX_CTRL_C_PCR      PORTA_PCR27
#define MUX_CTRL_C_GPIO_SET GPIOA_PSOR
#define MUX_CTRL_C_GPIO_CLR GPIOA_PCOR
#define MUX_CTRL_C_GPIO_DDR GPIOA_PDDR

#define MUX_CTRL_D          28 // PTA28			pin-79
#define MUX_CTRL_D_PCR      PORTA_PCR28
#define MUX_CTRL_D_GPIO_SET GPIOA_PSOR
#define MUX_CTRL_D_GPIO_CLR GPIOA_PCOR
#define MUX_CTRL_D_GPIO_DDR GPIOA_PDDR

#define MUX_OUT_0 24 // PTE26			pin-47
#define MUX_OUT_1 77 // PTA7			pin-59	ADC0_SE10
#define BUTTON1   14 // PTD1			pin-128

// LCD DISLPAY
#define LCD_MOSI  52 // PTD13			pin-142
#define LCD_MISO  51 // PTD14			pin-143
#define LCD_SCK   53 // PTD12			pin-141
#define LCD_CS    54 // PTD15			pin-144
#define LCD_PD    55 // PTD11			pin-140
#define LCD_RESET 82 // PTD10			pin-139

// FLASH
#define FLASH_MOSI 28 // PTA16			pin-68
#define FLASH_MISO 39 // PTA17			pin-69
#define FLASH_SCK  27 // PTA15			pin-67
#define FLASH_CS   26 // PTA14			pin-66

/*
#define GRID_B			64			//PTE8			pin-11
#define GRID_C			65	 		//PTE9			pin-12
#define GRID_I2C_SELECT	xxx 		//PTA4			pin-54
*/

// I2C
#define GRID_I2C_SDA 4 // PTA13			pin-65
#define GRID_I2C_SCL 3 // PTA12			pin-64

#define AUDIO_I2C_SDA 48 // PTA13			pin-65
#define AUDIO_I2C_SCL 47 // PTA12			pin-64

#ifdef TRACKER_MINI

#define BATTERY_I2C_SDA 4  // PTA13			pin-65
#define BATTERY_I2C_SCL 26 // PTA14			pin-64

#endif

// AUDIO CODEC
#define AUDIO_OUT_MUX 68 // PTE28			pin-49
#define AUDIO_IN_MUX  67 // PTE27			pin-48

// MIDI DIN/JACK
#define MIDI_OUT xxx // PTE24			pin-45
#define MIDI_IN  xxx // PTE25			pin-46

// HAPTIC - DAC OPTION
#define HAPTIC_OUT xxx // DAC0_OUT		pin-38

// TACT SWITCH
#define TACT_SWITCH 9 // PTC3			pin-106

// SD DETECT
//#define SD_DETECT 2 // PTD0			pin-127   (moved to platform)

// BOOTLOADER CTRL
#define BL_CTRL          1 // PTA1			pin-51
#define BL_CTRL_PCR      PORTA_PCR1
#define BL_CTRL_GPIO_SET GPIOA_PSOR
#define BL_CTRL_GPIO_CLR GPIOA_PCOR
#define BL_CTRL_GPIO_DDR GPIOA_PDDR

// RAM ON/OFF
#define RAM_CTRL          24 // PTA24			pin-75
#define RAM_CTRL_PCR      PORTA_PCR24
#define RAM_CTRL_GPIO_SET GPIOA_PSOR
#define RAM_CTRL_GPIO_CLR GPIOA_PCOR
#define RAM_CTRL_GPIO_DDR GPIOA_PDDR

// ENCODER
#define ENC_A          10 // PTA10			pin-62
#define ENC_A_PCR      PORTA_PCR10
#define ENC_A_GPIO_SET GPIOA_PSOR
#define ENC_A_GPIO_CLR GPIOA_PCOR
#define ENC_A_GPIO_DDR GPIOA_PDDR

#define ENC_B          11 // PTA11			pin-63
#define ENC_B_PCR      PORTA_PCR11
#define ENC_B_GPIO_SET GPIOA_PSOR
#define ENC_B_GPIO_CLR GPIOA_PCOR
#define ENC_B_GPIO_DDR GPIOA_PDDR

// ENCODER SWITCH
#define ENC_SWITCH 41 // PTD29			pin-80

#define MINI_MIC_SELECT 67 // PTE27			pin-48		// MIC

#define STANDBY_TACT  83 // PTA7			pin-51
#define STANDBY_RESET 84 // PTA8			pin-60
#define STANDBY_SET   85 // PTA9			pin-61

#define EXTERNAL_RAM_KEY 80

#ifdef TRACKER_MINI
#define BATTERY_LEVEL_PIN      39 // PTA17			pin-69
#define BATTERY_CHARG_SENS_PIN 14 // PTD1			pin-128
#endif

#endif
