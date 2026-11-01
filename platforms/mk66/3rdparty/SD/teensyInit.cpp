
#include "kinetis.h"

#include "core_pins.h"

#include "config.h"
#include "interruptsConfig.h"

void enableGPIO(bool enable)
{
    const uint32_t PORT_CLK = PORT_PCR_MUX(4) | PORT_PCR_DSE;
    const uint32_t PORT_CMD_DATA = PORT_CLK | PORT_PCR_PE | PORT_PCR_PS;
    const uint32_t PORT_PUP = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS;

    PORTE_PCR0 = enable ? PORT_CMD_DATA : PORT_PUP; // SDHC_D1
    PORTE_PCR1 = enable ? PORT_CMD_DATA : PORT_PUP; // SDHC_D0
    PORTE_PCR2 = enable ? PORT_CLK : PORT_PUP;      // SDHC_CLK
    PORTE_PCR3 = enable ? PORT_CMD_DATA : PORT_PUP; // SDHC_CMD
    PORTE_PCR4 = enable ? PORT_CMD_DATA : PORT_PUP; // SDHC_D3
    PORTE_PCR5 = enable ? PORT_CMD_DATA : PORT_PUP; // SDHC_D2
}

const uint32_t SDHC_IRQSTATEN_MASK =
  SDHC_IRQSTATEN_DMAESEN | SDHC_IRQSTATEN_AC12ESEN | SDHC_IRQSTATEN_DEBESEN |
  SDHC_IRQSTATEN_DCESEN | SDHC_IRQSTATEN_DTOESEN | SDHC_IRQSTATEN_CIESEN | SDHC_IRQSTATEN_CEBESEN |
  SDHC_IRQSTATEN_CCESEN | SDHC_IRQSTATEN_CTOESEN | SDHC_IRQSTATEN_DINTSEN | SDHC_IRQSTATEN_TCSEN |
  SDHC_IRQSTATEN_CCSEN;

static uint32_t m_sdClkKhz = 0;

void setSdclk(uint32_t kHzMax)
{
    const uint32_t DVS_LIMIT = 0X10;
    const uint32_t SDCLKFS_LIMIT = 0X100;
    uint32_t dvs = 1;
    uint32_t sdclkfs = 1;
    uint32_t maxSdclk = 1000 * kHzMax;

    while ((F_CPU / (sdclkfs * DVS_LIMIT) > maxSdclk) && (sdclkfs < SDCLKFS_LIMIT))
    {
        sdclkfs <<= 1;
    }
    while ((F_CPU / (sdclkfs * dvs) > maxSdclk) && (dvs < DVS_LIMIT))
    {
        dvs++;
    }
    m_sdClkKhz = F_CPU / (1000 * sdclkfs * dvs);
    sdclkfs >>= 1;
    dvs--;

    // Disable SDHC clock.
    SDHC_SYSCTL &= ~SDHC_SYSCTL_SDCLKEN;

    // Change dividers.
    uint32_t sysctl =
      SDHC_SYSCTL & ~(SDHC_SYSCTL_DTOCV_MASK | SDHC_SYSCTL_DVS_MASK | SDHC_SYSCTL_SDCLKFS_MASK);

    SDHC_SYSCTL =
      sysctl | SDHC_SYSCTL_DTOCV(0x0E) | SDHC_SYSCTL_DVS(dvs) | SDHC_SYSCTL_SDCLKFS(sdclkfs);

    // Wait until the SDHC clock is stable.
    while (!(SDHC_PRSSTAT & SDHC_PRSSTAT_SDSTB))
    {
    }
    // Enable the SDHC clock.
    SDHC_SYSCTL |= SDHC_SYSCTL_SDCLKEN;
}

void initSDHC()
{

#ifdef HAS_KINETIS_MPU
    // Allow SDHC Bus Master access.
    MPU_RGDAAC0 |= 0x0C000000;
#endif
    // Enable SDHC clock.
    SIM_SCGC3 |= SIM_SCGC3_SDHC;

    // Disable GPIO clock.
    enableGPIO(false);

    // Reset SDHC. Use default Water Mark Level of 16.
    SDHC_SYSCTL = SDHC_SYSCTL_RSTA;
    while (SDHC_SYSCTL & SDHC_SYSCTL_RSTA)
    {
    }
    // Set initial SCK rate.
    setSdclk(400);

    enableGPIO(true);

    // Enable desired IRQSTAT bits.
    SDHC_IRQSTATEN = SDHC_IRQSTATEN_MASK;

    NVIC_SET_PRIORITY(IRQ_SDHC, kSDHCInterruptLevel);
    NVIC_ENABLE_IRQ(IRQ_SDHC);

    // Send 80 clocks to card.
    SDHC_SYSCTL |= SDHC_SYSCTL_INITA;
    while (SDHC_SYSCTL & SDHC_SYSCTL_INITA)
    {
    }
}
