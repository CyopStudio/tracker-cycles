/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "sdmmc_config.h"
#include "fsl_iomuxc.h"

#include "interruptsConfig.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void BOARD_SDCardPowerControl(bool enable);

/*******************************************************************************
 * Variables
 ******************************************************************************/
/*!brief sdmmc dma buffer */
AT_NONCACHEABLE_SECTION_ALIGN(
  uint32_t s_sdmmcHostDmaBuffer[BOARD_SDMMC_HOST_DMA_DESCRIPTOR_BUFFER_SIZE],
  SDMMCHOST_DMA_DESCRIPTOR_BUFFER_ALIGN_SIZE);
#if defined SDMMCHOST_ENABLE_CACHE_LINE_ALIGN_TRANSFER && SDMMCHOST_ENABLE_CACHE_LINE_ALIGN_TRANSFER
/* two cache line length for sdmmc host driver maintain unalign transfer */
SDK_ALIGN(static uint8_t s_sdmmcCacheLineAlignBuffer[BOARD_SDMMC_DATA_BUFFER_ALIGN_SIZE * 2U],
          BOARD_SDMMC_DATA_BUFFER_ALIGN_SIZE);
#endif
#if defined(SDIO_ENABLED) || defined(SD_ENABLED)
static sd_detect_card_t s_cd;
static sd_io_voltage_t s_ioVoltage = {
    .type = BOARD_SDMMC_SD_IO_VOLTAGE_CONTROL_TYPE,
    .func = NULL,
};
#endif
sdmmchost_t s_host;
#ifdef SDIO_ENABLED
static sdio_card_int_t s_sdioInt;
#endif

/*******************************************************************************
 * Code
 ******************************************************************************/
uint32_t BOARD_USDHC1ClockConfiguration(void)
{
    CLOCK_InitSysPll(&sysPllConfig_BOARD_BootClockRUN);
    /*configure system pll PFD0 fractional divider to 24, output clock is 528MHZ * 18 / 24 = 396
     * MHZ*/
    CLOCK_InitSysPfd(kCLOCK_Pfd0, 24U);
    /* Configure USDHC clock source and divider */
    CLOCK_SetDiv(kCLOCK_Usdhc1Div, 1U); /* USDHC clock root frequency maximum: 198MHZ */
    CLOCK_SetMux(kCLOCK_Usdhc1Mux, 1U);

    return 396000000U / 2U;
}

#if defined(SDIO_ENABLED) || defined(SD_ENABLED)
void BOARD_SDCardPowerControl(bool enable)
{
}

bool BOARD_SDCardGetDetectStatus(void)
{
    return GPIO_PinRead(BOARD_SDMMC_SD_CD_GPIO_BASE, BOARD_SDMMC_SD_CD_GPIO_PIN) ==
           BOARD_SDMMC_SD_CD_INSERT_LEVEL;
}

void BOARD_SDMMC_SD_CD_PORT_IRQ_HANDLER(void)
{
    if (GPIO_PortGetInterruptFlags(BOARD_SDMMC_SD_CD_GPIO_BASE) &
        (1U << BOARD_SDMMC_SD_CD_GPIO_PIN))
    {
        if (s_cd.callback != NULL)
        {
            s_cd.callback(BOARD_SDCardGetDetectStatus(), s_cd.userData);
        }
    }
    /* Clear interrupt flag.*/
    GPIO_PortClearInterruptFlags(BOARD_SDMMC_SD_CD_GPIO_BASE, ~0U);
}

void BOARD_SDCardDetectInit(sd_cd_t cd, void* userData)
{
    /* install card detect callback */
    s_cd.cdDebounce_ms = BOARD_SDMMC_SD_CARD_DETECT_DEBOUNCE_DELAY_MS;
    s_cd.type = BOARD_SDMMC_SD_CD_TYPE;
    s_cd.cardDetected = BOARD_SDCardGetDetectStatus;
    s_cd.callback = cd;
    s_cd.userData = userData;

    if (BOARD_SDMMC_SD_CD_TYPE == kSD_DetectCardByGpioCD)
    {
        gpio_pin_config_t sw_config = {
            kGPIO_DigitalInput,
            0,
            kGPIO_IntRisingOrFallingEdge,
        };
        GPIO_PinInit(BOARD_SDMMC_SD_CD_GPIO_BASE, BOARD_SDMMC_SD_CD_GPIO_PIN, &sw_config);
        GPIO_PortEnableInterrupts(BOARD_SDMMC_SD_CD_GPIO_BASE, 1U << BOARD_SDMMC_SD_CD_GPIO_PIN);
        GPIO_PortClearInterruptFlags(BOARD_SDMMC_SD_CD_GPIO_BASE, ~0);

        /* set IRQ priority */
        NVIC_SetPriority(BOARD_SDMMC_SD_CD_IRQ, kSDHCDetectInterruptLevel);
        /* Open card detection pin NVIC. */
        EnableIRQ(BOARD_SDMMC_SD_CD_IRQ);

        if (GPIO_PinRead(BOARD_SDMMC_SD_CD_GPIO_BASE, BOARD_SDMMC_SD_CD_GPIO_PIN) ==
            BOARD_SDMMC_SD_CD_INSERT_LEVEL)
        {
            if (cd != NULL)
            {
                cd(true, userData);
            }
        }
    }
}

void BOARD_SD_Pin_Config(uint32_t freq)
{
    uint32_t speed = 0U, strength = 0U;

    if (freq <= 50000000)
    {
        speed = 0U;
        strength = 7U;
    }
    else if (freq <= 100000000)
    {
        speed = 2U;
        strength = 7U;
    }
    else
    {
        speed = 3U;
        strength = 7U;
    }

    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_00_USDHC1_CMD,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(speed) | IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                          IOMUXC_SW_PAD_CTL_PAD_PKE_MASK | IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
                          IOMUXC_SW_PAD_CTL_PAD_HYS_MASK | IOMUXC_SW_PAD_CTL_PAD_PUS(1) |
                          IOMUXC_SW_PAD_CTL_PAD_DSE(strength));
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_01_USDHC1_CLK,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(speed) | IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                          IOMUXC_SW_PAD_CTL_PAD_HYS_MASK | IOMUXC_SW_PAD_CTL_PAD_PUS(0) |
                          IOMUXC_SW_PAD_CTL_PAD_DSE(strength));
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_02_USDHC1_DATA0,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(speed) | IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                          IOMUXC_SW_PAD_CTL_PAD_PKE_MASK | IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
                          IOMUXC_SW_PAD_CTL_PAD_HYS_MASK | IOMUXC_SW_PAD_CTL_PAD_PUS(1) |
                          IOMUXC_SW_PAD_CTL_PAD_DSE(strength));
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_03_USDHC1_DATA1,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(speed) | IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                          IOMUXC_SW_PAD_CTL_PAD_PKE_MASK | IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
                          IOMUXC_SW_PAD_CTL_PAD_HYS_MASK | IOMUXC_SW_PAD_CTL_PAD_PUS(1) |
                          IOMUXC_SW_PAD_CTL_PAD_DSE(strength));
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_04_USDHC1_DATA2,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(speed) | IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                          IOMUXC_SW_PAD_CTL_PAD_PKE_MASK | IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
                          IOMUXC_SW_PAD_CTL_PAD_HYS_MASK | IOMUXC_SW_PAD_CTL_PAD_PUS(1) |
                          IOMUXC_SW_PAD_CTL_PAD_DSE(strength));
    IOMUXC_SetPinConfig(IOMUXC_GPIO_SD_B0_05_USDHC1_DATA3,
                        IOMUXC_SW_PAD_CTL_PAD_SPEED(speed) | IOMUXC_SW_PAD_CTL_PAD_SRE_MASK |
                          IOMUXC_SW_PAD_CTL_PAD_PKE_MASK | IOMUXC_SW_PAD_CTL_PAD_PUE_MASK |
                          IOMUXC_SW_PAD_CTL_PAD_HYS_MASK | IOMUXC_SW_PAD_CTL_PAD_PUS(1) |
                          IOMUXC_SW_PAD_CTL_PAD_DSE(strength));
}
#endif

#ifdef SD_ENABLED
void BOARD_SD_Config(void* card, sd_cd_t cd, uint32_t hostIRQPriority, void* userData)
{
    assert(card);

    s_host.dmaDesBuffer = s_sdmmcHostDmaBuffer;
    s_host.dmaDesBufferWordsNum = BOARD_SDMMC_HOST_DMA_DESCRIPTOR_BUFFER_SIZE;
    s_host.enableCacheControl = BOARD_SDMMC_HOST_CACHE_CONTROL;
#if defined SDMMCHOST_ENABLE_CACHE_LINE_ALIGN_TRANSFER && SDMMCHOST_ENABLE_CACHE_LINE_ALIGN_TRANSFER
    s_host.cacheAlignBuffer = s_sdmmcCacheLineAlignBuffer;
    s_host.cacheAlignBufferSize = BOARD_SDMMC_DATA_BUFFER_ALIGN_SIZE * 2U;
#endif

    ((sd_card_t*)card)->host = &s_host;
    ((sd_card_t*)card)->host->hostController.base = BOARD_SDMMC_SD_HOST_BASEADDR;
    ((sd_card_t*)card)->host->hostController.sourceClock_Hz =
      CLOCK_GetClockRootFreq(kCLOCK_Usdhc1ClkRoot);

    ((sd_card_t*)card)->usrParam.cd = &s_cd;
    ((sd_card_t*)card)->usrParam.pwr = BOARD_SDCardPowerControl;
    ((sd_card_t*)card)->usrParam.ioStrength = BOARD_SD_Pin_Config;
    ((sd_card_t*)card)->usrParam.ioVoltage = &s_ioVoltage;
    ((sd_card_t*)card)->usrParam.maxFreq = BOARD_SDMMC_SD_HOST_SUPPORT_SDR104_FREQ;

    BOARD_SDCardDetectInit(cd, userData);

    NVIC_SetPriority(BOARD_SDMMC_SD_HOST_IRQ, hostIRQPriority);
}
#endif
