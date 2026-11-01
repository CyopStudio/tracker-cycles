#include "MidiUart.h"
#include "TimeControl.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "interruptsConfig.h"
#include "pin_mux.h"
#include "stdio.h"

//#define UART_TEST

uint8_t rxData;

uint8_t rxRingBuffer[MidiUart::kRxRingBufferSize];
volatile uint8_t txRingBuffer[MidiUart::kTxRingBufferSize];

void LPUART_UserCallback(LPUART_Type* base,
                         lpuart_handle_t* handle,
                         status_t status,
                         void* userData)
{
    if (kStatus_LPUART_TxIdle == status)
    {
        MidiUart::TxRingBuffer* writeRingBuffer = (MidiUart::TxRingBuffer*)userData;

        if (writeRingBuffer->activeTransferSize > 0)
        {
            writeRingBuffer->tail =
              (writeRingBuffer->tail + writeRingBuffer->activeTransferSize) %
              MidiUart::kTxRingBufferSize;
            writeRingBuffer->activeTransferSize = 0;
        }

        if (writeRingBuffer->head != writeRingBuffer->tail)
        {
            int chunkLen = writeRingBuffer->tail > writeRingBuffer->head
                             ? MidiUart::kTxRingBufferSize - writeRingBuffer->tail
                             : writeRingBuffer->head - writeRingBuffer->tail;
            lpuart_transfer_t txTransfer;
            txTransfer.data = &writeRingBuffer->buffer[writeRingBuffer->tail];
            txTransfer.dataSize = (size_t)chunkLen;
            writeRingBuffer->activeTransferSize = chunkLen;
            status_t sendStatus = LPUART_TransferSendNonBlocking(base, handle, &txTransfer);
            if (sendStatus != kStatus_Success)
            {
                writeRingBuffer->activeTransferSize = 0;
            }
        }
    }
}

void MidiUart::init(LPUART_Type* uartBase)
{
    assert(uartBase);
    this->uartBase = uartBase;

    writeRingBuffer.buffer = (uint8_t*)txRingBuffer;
    writeRingBuffer.head = 0;
    writeRingBuffer.tail = 0;
    writeRingBuffer.activeTransferSize = 0;

    rxTransfer.data = &rxData;
    rxTransfer.dataSize = 1;

    lpuart_config_t config;
    LPUART_GetDefaultConfig(&config);

    config.baudRate_Bps = 31250;
    config.parityMode = kLPUART_ParityDisabled;
    config.stopBitCount = kLPUART_OneStopBit;
    config.txFifoWatermark = 0;
    config.rxFifoWatermark = 0;
    config.enableTx = false;
    config.enableRx = false;

    uint32_t uartClock = CLOCK_GetClockRootFreq(kCLOCK_UartClkRoot);
    assert(uartClock == 8000000);

    LPUART_Init(uartBase, &config, uartClock);
    uartBase->CTRL |= LPUART_CTRL_TXINV_MASK; // inverting tx signal

    NVIC_SetPriority(LPUART5_IRQn, kMidiJackInUARTPriorityLevel);

    LPUART_TransferCreateHandle(uartBase, &lpuartHandle, LPUART_UserCallback, &writeRingBuffer);
    LPUART_TransferStartRingBuffer(uartBase, &lpuartHandle, rxRingBuffer, kRxRingBufferSize);

    LPUART_EnableRx(uartBase, true);
    LPUART_EnableTx(uartBase, true);
}

void MidiUart::write(uint8_t data)
{
    uint32_t primask = DisableGlobalIRQ();

    int nextHead = (writeRingBuffer.head + 1) % kTxRingBufferSize;
    if (nextHead != writeRingBuffer.tail)
    {
        writeRingBuffer.buffer[writeRingBuffer.head] = data;
        writeRingBuffer.head = nextHead;
    }

    if (lpuartHandle.txState == kLPUART_TxIdle && writeRingBuffer.activeTransferSize == 0)
    {
        LPUART_UserCallback(uartBase, &lpuartHandle, kStatus_LPUART_TxIdle, &writeRingBuffer);
    }

    EnableGlobalIRQ(primask);
}

uint8_t MidiUart::read()
{
    if (available() > 0)
    {
        LPUART_TransferReceiveNonBlocking(uartBase, &lpuartHandle, &rxTransfer, nullptr);
        return rxData;
    }

    return 0;
}

size_t MidiUart::available()
{
    return LPUART_TransferGetRxRingBufferLength(uartBase, &lpuartHandle);
}

void MidiUart::update()
{
#ifdef UART_TEST
    if (availible())
    {
        uint8_t rx = read();
        PRINTF("rx data: %d\n", rx);

        write(rx);
    }
#endif
}
