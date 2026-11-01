#ifndef MIDIUART_H_
#define MIDIUART_H_

#include "fsl_lpuart.h"

class MidiUart
{
  public:
    struct TxRingBuffer
    {
        volatile int head;
        volatile int tail;
        volatile int activeTransferSize;
        uint8_t* buffer;
    };
    static constexpr int kRxRingBufferSize = 511; // multiply of 3 plus 1
                                                  // (one byte of ring buffer is not used)
    static constexpr int kTxRingBufferSize = 512;
    static constexpr int kLPUART_TxIdle = 0; // matching enum in fsl_lpuart.cpp

    static const bool thruActivated = false;

    void init(LPUART_Type* uartBase);
    void update();

    void write(uint8_t data);
    uint8_t read();
    size_t available();

    void begin(){};
    bool beginTransmission(int) { return true; };
    void endTransmission(){};

  private:
    TxRingBuffer writeRingBuffer;
    LPUART_Type* uartBase;
    lpuart_handle_t lpuartHandle;
    lpuart_transfer_t txTransfer;
    lpuart_transfer_t rxTransfer;
};

#endif /* MIDIUART_H_ */
