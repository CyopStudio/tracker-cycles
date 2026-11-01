/*
 * Cpu.cpp
 *
 *  Created on: Sep 14, 2023
 *      Author: radekp
 */

#include "MIMXRT1051.h"
#include "fsl_tempmon.h"
#include "log.h"
#include <Cpu.h>
#include <locale.h>

#define OVERCLOCK_STEPSIZE 14000000
#define OVERCLOCK_MAX_VOLT 1575
#define PERIOD_MS 5000

Cpu::Cpu()
{
    tempmon_config_t tempConf = {
        .frequency = 2, .highAlarmTemp = 60, .panicAlarmTemp = 70, .lowAlarmTemp = 10
    };

    TEMPMON_Init(TEMPMON, &tempConf);
    TEMPMON_StartMeasure(TEMPMON);
    setlocale(LC_ALL, "");
}

Cpu::~Cpu()
{
    TEMPMON_StopMeasure(TEMPMON);
}

uint32_t Cpu::SetArmClock(uint32_t frequency)
{
    uint32_t cbcdr = CCM->CBCDR; // pg 1021
    uint32_t cbcmr = CCM->CBCMR; // pg 1023
    uint32_t dcdc = DCDC->REG3;

    // compute required voltage
    uint32_t voltage = 1150; // default = 1.15V
    if (frequency > 528000000)
    {
        voltage = 1250; // 1.25V
#if defined(OVERCLOCK_STEPSIZE) && defined(OVERCLOCK_MAX_VOLT)
        if (frequency > 600000000)
        {
            voltage += ((frequency - 600000000) / OVERCLOCK_STEPSIZE) * 25;
            if (voltage > OVERCLOCK_MAX_VOLT)
                voltage = OVERCLOCK_MAX_VOLT;
        }
#endif
    }
    else if (frequency <= 24000000)
    {
        voltage = 950; // 0.95
    }

    // if voltage needs to increase, do it before switch clock speed
    CCM->CCGR6 |= CCM_CCGR3_CG3(1);
    if ((dcdc & DCDC_REG3_TRG_MASK) < DCDC_REG3_TRG((voltage - 800) / 25))
    {
        LOGF("Increasing voltage to %u mV\n", voltage);
        dcdc &= ~DCDC_REG3_TRG_MASK;
        dcdc |= DCDC_REG3_TRG((voltage - 800) / 25);
        DCDC->REG3 = dcdc;
        while (!(DCDC->REG0 & DCDC_REG0_STS_DC_OK(1)))
            ; // wait voltage settling
    }

    if (!(cbcdr & CCM_CBCDR_PERIPH_CLK_SEL(0)))
    {
        LOG("need to switch to alternate clock during reconfigure of ARM PLL\n");
        const uint32_t need1s = CCM_ANALOG_PLL_USB1_ENABLE(1) | CCM_ANALOG_PLL_USB1_POWER(1) |
                                CCM_ANALOG_PLL_USB1_LOCK(1) | CCM_ANALOG_PLL_USB1_EN_USB_CLKS(1);
        uint32_t sel, div;
        if ((CCM_ANALOG->PLL_USB1 & need1s) == need1s)
        {
            LOG("USB PLL is running, so we can use 120 MHz\n");
            sel = 0;
            div = 3; // divide down to 120 MHz, so IPG is ok even if IPG_PODF=0
        }
        else
        {
            LOG("USB PLL is off, use 24 MHz crystal\n");
            sel = 1;
            div = 0;
        }
        if ((cbcdr & CCM_CBCDR_PERIPH_CLK2_PODF_MASK) != CCM_CBCDR_PERIPH_CLK2_PODF(div))
        {
            // PERIPH_CLK2 divider needs to be changed
            cbcdr &= ~CCM_CBCDR_PERIPH_CLK2_PODF_MASK;
            cbcdr |= CCM_CBCDR_PERIPH_CLK2_PODF(div);
            CCM->CBCDR = cbcdr;
        }
        if ((cbcmr & CCM_CBCMR_PERIPH_CLK2_SEL_MASK) != CCM_CBCMR_PERIPH_CLK2_SEL(sel))
        {
            // PERIPH_CLK2 source select needs to be changed
            cbcmr &= ~CCM_CBCMR_PERIPH_CLK2_SEL_MASK;
            cbcmr |= CCM_CBCMR_PERIPH_CLK2_SEL(sel);
            CCM->CBCMR = cbcmr;
            while (CCM->CDHIPR & CCM_CDHIPR_PERIPH2_CLK_SEL_BUSY(1))
                ; // wait
        }
        // switch over to PERIPH_CLK2
        cbcdr |= CCM_CBCDR_PERIPH_CLK_SEL(1);
        CCM->CBCDR = cbcdr;
        while (CCM->CDHIPR & CCM_CDHIPR_PERIPH_CLK_SEL_BUSY(1))
            ; // wait
    }
    else
    {
        LOG("already running from PERIPH_CLK2, safe to mess with ARM PLL\n");
    }

    // TODO: check if PLL2 running, can 352, 396 or 528 can work? (no need for ARM PLL)

    // DIV_SELECT: 54-108 = official range 648 to 1296 in 12 MHz steps
    uint32_t div_arm = 1;
    uint32_t div_ahb = 1;
    while (frequency * div_arm * div_ahb < 648000000)
    {
        if (div_arm < 8)
        {
            div_arm = div_arm + 1;
        }
        else
        {
            if (div_ahb < 5)
            {
                div_ahb = div_ahb + 1;
                div_arm = 1;
            }
            else
            {
                break;
            }
        }
    }
    uint32_t mult = (frequency * div_arm * div_ahb + 6000000) / 12000000;
    if (mult > 108)
        mult = 108;
    if (mult < 54)
        mult = 54;
    LOGF("Freq: 12 MHz * %u / %u / %u\n", mult, div_arm, div_ahb);
    frequency = mult * 12000000 / div_arm / div_ahb;

    const uint32_t arm_pll_mask = CCM_ANALOG_PLL_ARM_LOCK(1) | CCM_ANALOG_PLL_ARM_BYPASS(1) |
                                  CCM_ANALOG_PLL_ARM_ENABLE(1) | CCM_ANALOG_PLL_ARM_POWERDOWN(1) |
                                  CCM_ANALOG_PLL_ARM_DIV_SELECT_MASK;
    if ((CCM_ANALOG->PLL_ARM & arm_pll_mask) !=
        (CCM_ANALOG_PLL_ARM_LOCK(1) | CCM_ANALOG_PLL_ARM_ENABLE(1) |
         CCM_ANALOG_PLL_ARM_DIV_SELECT(mult)))
    {
        LOG("ARM PLL needs reconfigure\n");
        CCM_ANALOG->PLL_ARM = CCM_ANALOG_PLL_ARM_POWERDOWN(1);
        // TODO: delay needed?
        CCM_ANALOG->PLL_ARM = CCM_ANALOG_PLL_ARM_ENABLE(1) | CCM_ANALOG_PLL_ARM_DIV_SELECT(mult);
        while (!(CCM_ANALOG->PLL_ARM & CCM_ANALOG_PLL_ARM_LOCK(1)))
            ; // wait for lock
    }
    else
    {
        LOG("ARM PLL already running at required frequency\n");
    }

    if ((CCM->CACRR & CCM_CACRR_ARM_PODF_MASK) != (div_arm - 1))
    {
        CCM->CACRR = CCM_CACRR_ARM_PODF(div_arm - 1);
        while (CCM->CDHIPR & CCM_CDHIPR_ARM_PODF_BUSY(1))
            ; // wait
    }

    if ((cbcdr & CCM_CBCDR_AHB_PODF_MASK) != CCM_CBCDR_AHB_PODF(div_ahb - 1))
    {
        cbcdr &= ~CCM_CBCDR_AHB_PODF_MASK;
        cbcdr |= CCM_CBCDR_AHB_PODF(div_ahb - 1);
        CCM->CBCDR = cbcdr;
        while (CCM->CDHIPR & CCM_CDHIPR_AHB_PODF_BUSY(1))
            ; // wait
    }

    uint32_t div_ipg = (frequency + 149999999) / 150000000;
    if (div_ipg > 4)
        div_ipg = 4;
    if ((cbcdr & CCM_CBCDR_IPG_PODF_MASK) != (CCM_CBCDR_IPG_PODF(div_ipg - 1)))
    {
        cbcdr &= ~CCM_CBCDR_IPG_PODF_MASK;
        cbcdr |= CCM_CBCDR_IPG_PODF(div_ipg - 1);
        // TODO: how to safely change IPG_PODF ??
        CCM->CBCDR = cbcdr;
    }

    // cbcdr &= ~CCM_CBCDR_PERIPH_CLK_SEL;
    // CCM_CBCDR = cbcdr;  // why does this not work at 24 MHz?
    CCM->CBCDR &= ~CCM_CBCDR_PERIPH_CLK_SEL(1);
    while (CCM->CDHIPR & CCM_CDHIPR_PERIPH_CLK_SEL_BUSY(1))
        ; // wait

    LOGF("New Frequency: ARM=%u, IPG=%u\n", frequency, frequency / div_ipg);

    // if voltage needs to decrease, do it after switch clock speed
    if ((dcdc & DCDC_REG3_TRG_MASK) > DCDC_REG3_TRG((voltage - 800) / 25))
    {
        LOGF("Decreasing voltage to %u mV\n", voltage);
        dcdc &= ~DCDC_REG3_TRG_MASK;
        dcdc |= DCDC_REG3_TRG((voltage - 800) / 25);
        DCDC->REG3 = dcdc;
        while (!(DCDC->REG0 & DCDC_REG0_STS_DC_OK(1)))
            ; // wait voltage settling
    }

    LOGF("ARM PLL=%x CCM CACRR=%x DCDC REG3=%x\n", CCM_ANALOG->PLL_ARM, CCM->CACRR, DCDC->REG3);

    return frequency;
}

void Cpu::setVoltage(uint32_t Voltage)
{
    uint32_t dcdc = DCDC->REG3;
    dcdc &= ~DCDC_REG3_TRG_MASK;
    dcdc |= DCDC_REG3_TRG((Voltage - 800) / 25);
    DCDC->REG3 = dcdc;
    while (!(DCDC->REG0 & DCDC_REG0_STS_DC_OK(1)))
        ;
}
uint32_t Cpu::getVoltage()
{
    uint32_t dcdc = DCDC->REG3;
    uint32_t voltage;

    dcdc &= DCDC_REG3_TRG_MASK;
    voltage = dcdc * 25 + 800;

    return voltage;
}

void Cpu::setFrequency(uint32_t Frequency)
{
    SetArmClock(Frequency);
    SystemCoreClockUpdate();
}

uint32_t Cpu::getFrequency()
{
    return SystemCoreClock;
}

uint32_t Cpu::getTemperature()
{
    return (uint32_t)TEMPMON_GetCurrentTemperature(TEMPMON);
}

void Cpu::update()
{
    LOGF("CPU statistics:\n\r\tFrequency %d Hz\n\r\tVoltage %d mV\n\r\tTemperature %d\u00b0C\n\r",
         getFrequency(),
         getVoltage(),
         getTemperature());
}
