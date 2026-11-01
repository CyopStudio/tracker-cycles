/*
 * GENERATED FILE - DO NOT EDIT
 * Copyright (c) 2008-2013 Code Red Technologies Ltd,
 * Copyright 2015, 2018-2019 NXP
 * (c) NXP Semiconductors 2013-2022
 * Generated linker script file for MIMXRT1051xxxxB
 * Created from memory.ldt by FMCreateLinkMemory
 * Using Freemarker v2.3.30
 * MCUXpresso IDE v11.5.1 [Build 7266] [2022-04-13] on 5 wrz 2022, 09:18:35
 */

MEMORY
{
  /* Define each memory region */
  QSPI_FLASH (rx) : ORIGIN = 0x60030000, LENGTH = 0xfd0000 /* 16580608 bytes (alias Flash) */  
  SRAM_DTC (rwx) : ORIGIN = 0x20000000, LENGTH = 0x58000 /* 384K bytes (alias RAM) */  
  SRAM_ITC (rwx) : ORIGIN = 0x8, LENGTH = 0x1fff8 /* 98296 bytes (alias RAM2) */  
  SRAM_OC (rwx) : ORIGIN = 0x20200000, LENGTH = 0x8000 /* 32K bytes (alias RAM3) */  
  EXTERNAL_RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 0x4000000 /* 64M bytes (alias RAM4) */  
}

  /* Define a symbol for the top of each memory region */
  __base_QSPI_FLASH = 0x60030000  ; /* QSPI_FLASH */  
  __base_Flash = 0x60030000 ; /* Flash */  
  __top_QSPI_FLASH = 0x60030000 + 0xfd0000 ; /* 16580608 bytes */  
  __top_Flash = 0x60030000 + 0xfd0000 ; /* 16580608 bytes */  
  __base_SRAM_DTC = 0x20000000  ; /* SRAM_DTC */  
  __base_RAM = 0x20000000 ; /* RAM */  
  __top_SRAM_DTC = 0x20000000 + 0x58000 ; /* 416K bytes */  
  __top_RAM = 0x20000000 + 0x58000 ; /* 416K bytes */  
  __base_SRAM_ITC = 0x8  ; /* SRAM_ITC */  
  __base_RAM2 = 0x8 ; /* RAM2 */  
  __top_SRAM_ITC = 0x8 + 0x1fff8 ; /* 65528 bytes */  
  __top_RAM2 = 0x8 + 0x1fff8 ; /* 65528 bytes */  
  __base_SRAM_OC = 0x20200000  ; /* SRAM_OC */  
  __base_RAM3 = 0x20200000 ; /* RAM3 */  
  __top_SRAM_OC = 0x20200000 + 0x8000 ; /* 32K bytes */  
  __top_RAM3 = 0x20200000 + 0x8000 ; /* 32K bytes */  
  __base_EXTERNAL_RAM = 0x80000000  ; /* EXTERNAL_RAM1 */  
  __base_RAM4 = 0x80000000 ; /* RAM4 */  
  __top_EXTERNAL_RAM = 0x80000000 + 0x4000000 ; /* 64M bytes */  
  __top_RAM4 = 0x80000000 + 0x4000000 ; /* 64M bytes */  
