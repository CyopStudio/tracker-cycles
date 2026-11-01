#include "Profiler.h"
#include "config.h"
#include "limits.h"
#include "log.h"
#include "malloc.h"
#include "usb_serial.h"
#include "util.h"
#include <polyassert.h>

#include <algorithm>
#include <vector>

namespace interrupt_priority_profiler
{
struct NVIC_Type
{
    volatile uint32_t ISER[8];
    uint32_t RESERVED0[24];
    volatile uint32_t ICER[8];
    uint32_t RSERVED1[24];
    volatile uint32_t ISPR[8];
    uint32_t RESERVED2[24];
    volatile uint32_t ICPR[8];
    uint32_t RESERVED3[24];
    volatile uint32_t IABR[8];
    uint32_t RESERVED4[56];
    volatile uint8_t IP[240];
    uint32_t RESERVED5[644];
    volatile uint32_t STIR;
};

struct SCB_Type
{
    volatile uint32_t CPUID;
    volatile uint32_t ICSR;
    volatile uint32_t VTOR;
    volatile uint32_t AIRCR;
    volatile uint32_t SCR;
    volatile uint32_t CCR;
    volatile uint8_t SHP[12];
    volatile uint32_t SHCSR;
    volatile uint32_t CFSR;
    volatile uint32_t HFSR;
    volatile uint32_t DFSR;
    volatile uint32_t MMFAR;
    volatile uint32_t BFAR;
    volatile uint32_t AFSR;
    volatile uint32_t PFR[2];
    volatile uint32_t DFR;
    volatile uint32_t ADR;
    volatile uint32_t MMFR[4];
    volatile uint32_t ISAR[5];
    uint32_t RESERVED0[5];
    volatile uint32_t CPACR;
};

constexpr uint32_t kScsBaseAddr = 0xE000E000UL;
constexpr int kNvicPriorityBits = 4;

NVIC_Type* nvic = (NVIC_Type*)(kScsBaseAddr + 0x0100UL);
SCB_Type* scb = (SCB_Type*)(kScsBaseAddr + 0x0D00UL);

uint32_t NVIC_GetPriority(int IRQn)
{

    if (IRQn < 0)
    {
        return ((uint32_t)(scb->SHP[((uint32_t)(IRQn)&0xF) - 4] >> (8 - kNvicPriorityBits)));
    } /* get priority for Cortex-M  system interrupts */
    else
    {
        return ((uint32_t)(nvic->IP[(uint32_t)(IRQn)] >> (8 - kNvicPriorityBits)));
    } /* get priority for K66 interrupts  */
}

const char* irqName[PlatformProfiler::kK66VectorNumber] = {
    (const char*)"Initial Stack Pointer",
    (const char*)"Initial Program Counter",
    (const char*)"Non-maskable Interrupt (NMI)",
    (const char*)"Hard Fault",
    (const char*)"MemManage Fault",
    (const char*)"Bus Fault",
    (const char*)"Usage Fault",
    (const char*)"--empty",
    (const char*)"--empty",
    (const char*)"--empty",
    (const char*)"--empty",
    (const char*)"Supervisor call (SVCall)",
    (const char*)"Debug Monitor",
    (const char*)"--empty",
    (const char*)"Pendable request for system service (PendableSrvReq)",
    (const char*)"System tick timer (SysTick)",
    (const char*)"DMA channel 0 or 16 transfer complete",
    (const char*)"DMA channel 1 or 17 transfer complete",
    (const char*)"DMA channel 2 or 18 transfer complete",
    (const char*)"DMA channel 3 or 19 transfer complete",
    (const char*)"DMA channel 4 or 20 transfer complete",
    (const char*)"DMA channel 5 or 21 transfer complete",
    (const char*)"DMA channel 6 or 22 transfer complete",
    (const char*)"DMA channel 7 or 23 transfer complete",
    (const char*)"DMA channel 8 or 24 transfer complete",
    (const char*)"DMA channel 9 or 25 transfer complete",
    (const char*)"DMA channel 10 or 26 transfer complete",
    (const char*)"DMA channel 11 or 27 transfer complete",
    (const char*)"DMA channel 12 or 28 transfer complete",
    (const char*)"DMA channel 13 or 29 transfer complete",
    (const char*)"DMA channel 14 or 30 transfer complete",
    (const char*)"DMA channel 15 or 31 transfer complete",
    (const char*)"DMA error interrupt channels",
    (const char*)"MCM interrupt",
    (const char*)"Flash memory command Complete",
    (const char*)"Flash memory read collision",
    (const char*)"Mode Controler - Low-voltage detect",
    (const char*)"LLWU - Low Leakage Wakeup",
    (const char*)"WDOG or EWM",
    (const char*)"Random generation number",
    (const char*)"I2C0",
    (const char*)"I2C1",
    (const char*)"SPI0",
    (const char*)"SPI1",
    (const char*)"I2S0 transmit",
    (const char*)"I2S0 receive",
    (const char*)"--empty",
    (const char*)"UART0 status",
    (const char*)"UART0 error",
    (const char*)"UART1 status",
    (const char*)"UART1 error",
    (const char*)"UART2 status",
    (const char*)"UART2 error",
    (const char*)"UART3 status",
    (const char*)"UART3 error",
    (const char*)"ADC0",
    (const char*)"CMP0",
    (const char*)"CMP1",
    (const char*)"FTM0",
    (const char*)"FTM1",
    (const char*)"FTM2",
    (const char*)"CMT",
    (const char*)"RTC alarm",
    (const char*)"RTC seconds",
    (const char*)"PIT channel 0",
    (const char*)"PIT channel 1",
    (const char*)"PIT channel 2",
    (const char*)"PIT channel 3",
    (const char*)"PDB",
    (const char*)"USBFS OTG",
    (const char*)"USBFS Charger Detect",
    (const char*)"--empty",
    (const char*)"DAC0",
    (const char*)"MCG",
    (const char*)"Low power timer",
    (const char*)"Port A pin detect",
    (const char*)"Port B pin detect",
    (const char*)"Port C pin detect",
    (const char*)"Port D pin detect",
    (const char*)"Port E pin detect",
    (const char*)"Software",
    (const char*)"SPI2",
    (const char*)"UART4 status",
    (const char*)"UART4 error",
    (const char*)"--empty",
    (const char*)"--empty",
    (const char*)"CMP2",
    (const char*)"FTM3",
    (const char*)"DAC1",
    (const char*)"ADC1",
    (const char*)"I2C2",
    (const char*)"CAN0 OR'ed Message buffer (0-15)",
    (const char*)"CAN0 Bus Off",
    (const char*)"CAN0 Error",
    (const char*)"CAN0 Transmit Warning",
    (const char*)"CAN0 Receive Warning",
    (const char*)"CAN0 Wake Up",
    (const char*)"SDHC",
    (const char*)"Ethernet MAC IEEE 1588 Timer Interrupt",
    (const char*)"Ethernet MAC Transmit interrupt",
    (const char*)"Ethernet MAC Receive interrupt",
    (const char*)"Ethernet MAC Error and miscellaneous interrupt",
    (const char*)"LPUART0",
    (const char*)"TSI0",
    (const char*)"TPM1",
    (const char*)"TPM2",
    (const char*)"USBHS DCD or USBHS Phy",
    (const char*)"I2C3",
    (const char*)"CMP3",
    (const char*)"USBHS OTG",
    (const char*)"CAN1 OR'ed Message buffer (0-15)",
    (const char*)"CAN1 Bus Off",
    (const char*)"CAN1 Error",
    (const char*)"CAN1 Transmit Warning",
    (const char*)"CAN1 Receive Warning",
    (const char*)"CAN1 Wake Up"
};

}

void PlatformProfiler::listInterruptsPriorities()
{
    for (int interruptIdx = 0; interruptIdx < kK66VectorNumber; interruptIdx++)
    {
        LOGF("%d.%s: %d\n",
             interruptIdx,
             getNvicIrqName(interruptIdx),
             getNvicIrqPriorityRegister(interruptIdx));
    }
}

void PlatformProfiler::resetMeasuringAudio()
{
    lastSampleUs = 0;
    sumOfSamples = 0;
    minSampleUs = INT_MAX;
    maxSampleUs = 0;
    samples = 0;
}

void PlatformProfiler::startMeasuringAudio()
{
    audioUpdateTimer = micros();
}

void PlatformProfiler::stopMeasuringAudio()
{
    lastSampleUs = micros() - audioUpdateTimer;

    minSampleUs = std::min(minSampleUs, lastSampleUs);
    maxSampleUs = std::max(maxSampleUs, lastSampleUs);

    sumOfSamples += lastSampleUs;
    samples++;
}

void PlatformProfiler::getAudioProcessingUsageUs(AudioProcessorUsage& usage)
{

    usage.average = sumOfSamples / samples;
    usage.last = lastSampleUs;
    usage.maximum = maxSampleUs;
    usage.minimum = minSampleUs;
}

void PlatformProfiler::getAudioProcessingUsagePercent(AudioProcessorUsage& usage)
{
    getAudioProcessingUsageUs(usage);

    usage.average = calculatePercentProcessorUsage(usage.average);
    usage.last = calculatePercentProcessorUsage(usage.last);
    usage.maximum = calculatePercentProcessorUsage(usage.maximum);
    usage.minimum = calculatePercentProcessorUsage(usage.minimum);
}

uint32_t PlatformProfiler::getNvicIrqPriorityRegister(int vector)
{
    return interrupt_priority_profiler::NVIC_GetPriority(vector - kCoreInterruptsNumber);
}

const char* PlatformProfiler::getNvicIrqName(int vector)
{
    return interrupt_priority_profiler::irqName[vector];
}

int PlatformProfiler ::calculatePercentProcessorUsage(int processorUsageUs)
{
    return (100 * processorUsageUs) / kMaxAudioUpdateTime;
}
