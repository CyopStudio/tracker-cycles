#include "usb.h"
#include "usb_device.h"
#include "usb_device_config.h"

#include "usb_device_class.h"

#include "usb_device_ch9.h"
#include "usb_device_descriptor.h"
#include "usb_device_midi.h"

#include "board.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "fsl_device_registers.h"

#include "composite.h"

#include "usb_midi.h"

#include <stdio.h>
#include <stdlib.h>

extern usb_device_endpoint_struct_t g_MidiEndpoints[];

#define RECEIVE_BUFF_SIZE HS_MIDI_BULK_OUT_PACKET_SIZE

/* Data buffer for receiving and sending*/
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t s_currRecvBuf[RECEIVE_BUFF_SIZE];
volatile static uint32_t s_recvSize = 0;

#define SEND_BUFF_SIZE  64
#define SEND_BUFF_COUNT 6

USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint32_t sendBuf[SEND_BUFF_COUNT][SEND_BUFF_SIZE];

volatile static int currentInputBuf = 0;
volatile static int currentSendBuf = 0;
volatile static int sendBufSize[SEND_BUFF_COUNT] = { 0 };

#define QUEUE_SIZE 256
volatile int queueTop = 0;
volatile int queueBottom = 0;
volatile MidiUsbMessage midiMessageQueue[QUEUE_SIZE];

volatile static usb_device_composite_struct_t* g_deviceComposite;

//#define DEBUG_PRINT_MIDI_OUT_STATUS
#ifdef DEBUG_PRINT_MIDI_OUT_STATUS
uint32_t lastClkCount = 0;
#endif

bool midiSendPacket();

usb_status_t USB_DeviceMidiCallback(class_handle_t handle, uint32_t event, void* param)
{
    usb_status_t error = kStatus_USB_Success;
    usb_device_endpoint_callback_message_struct_t* epCbParam;
    epCbParam = (usb_device_endpoint_callback_message_struct_t*)param;

    switch (event)
    {
        case kUSB_DeviceMidiEventSendResponse:
        {
            if (epCbParam->length > 0)
            {
                sendBufSize[currentSendBuf] = 0;
                currentSendBuf = currentSendBuf >= SEND_BUFF_COUNT - 1 ? 0 : currentSendBuf + 1;
            }

#ifdef DEBUG_PRINT_MIDI_OUT_STATUS
            DbgConsole_Printf("t: %8d used bufs: %1d  set send buf to: %d \n",
                              (DWT->CYCCNT - lastClkCount) / 528,
                              currentInputBuf >= currentSendBuf
                                ? currentInputBuf - currentSendBuf
                                : SEND_BUFF_COUNT - currentSendBuf + currentInputBuf,
                              currentSendBuf);
            lastClkCount = DWT->CYCCNT;
#endif

            if (!midiSendPacket())
            {
                if ((epCbParam->length != 0) &&
                    (!(epCbParam->length % g_MidiEndpoints[0].maxPacketSize)))
                {
                    /*
                    If the last packet is the size of endpoint, then send also
                    zero-ended packet,  meaning that we want to inform the host that we do not
                    have any additional data, so it can flush the output.
                    */
                    error = USB_DeviceMidiSend(handle, USB_MIDI_BULK_IN_ENDPOINT, NULL, 0);
                }
            }
            break;
        }
        case kUSB_DeviceMidiEventRecvResponse:
        {
            if (epCbParam->length == 0xffffffff)
            {
                break;
            }

            s_recvSize = epCbParam->length;

            uint32_t frame, channel, type1, type2, command;
            uint32_t index = 0;

            while (index + 4 <= s_recvSize)
            {
                bool skipMessage = false;
                frame = ((uint32_t*)s_currRecvBuf)[index / 4];

                type1 = frame & 15;
                type2 = (frame >> 12) & 15;
                command = (frame >> 8) & 0xFF;
                channel = (command & 15) + 1;
                // cable = (n >> 4) & 15; // not used for now

                if (type1 >= 0x08 && type1 <= 0x0E)
                {
                    if (type1 == 0x08 && type2 == 0x08)
                    {
                        midiMessageQueue[queueTop].type = kNoteOff;
                    }
                    else if (type1 == 0x09 && type2 == 0x09)
                    {
                        if ((frame >> 24) > 0)
                        {
                            midiMessageQueue[queueTop].type = kNoteOn;
                        }
                        else
                        {
                            midiMessageQueue[queueTop].type = kNoteOff;
                        }
                    }
                    else if (type1 == 0x0A && type2 == 0x0A)
                    {
                        midiMessageQueue[queueTop].type = kAfterTouch;
                    }
                    else if (type1 == 0x0B && type2 == 0x0B)
                    {
                        midiMessageQueue[queueTop].type = kControlChange;
                    }
                    else if (type1 == 0x0C && type2 == 0x0C)
                    {
                        midiMessageQueue[queueTop].type = kProgramChange;
                    }
                    else if (type1 == 0x0D && type2 == 0x0D)
                    {
                        midiMessageQueue[queueTop].type = kChannelPressure;
                    }
                    else if (type1 == 0x0E && type2 == 0x0E)
                    {
                        midiMessageQueue[queueTop].type = kPitchBend;
                    }
                    else
                    {
                        skipMessage = true;
                    }
                }
                else
                {
                    if (type1 == 0x02 || type1 == 0x03 ||
                        (type1 == 0x05 && command >= 0xF1 && command != 0xF7) ||
                        (type1 == 0x0F && command >= 0xF8))
                    {
                        midiMessageQueue[queueTop].type = command;
                    }
                    else
                    {
                        skipMessage = true;
                    }
                }

                midiMessageQueue[queueTop].channel = channel;
                midiMessageQueue[queueTop].data1 = (frame >> 16);
                midiMessageQueue[queueTop].data2 = (frame >> 24);

                if (!skipMessage)
                {
                    if (queueTop >= QUEUE_SIZE - 1)
                    {
                        queueTop = 0;
                    }
                    else
                    {
                        queueTop++;
                    }

                    if (queueTop == queueBottom)
                    {
                        if (queueBottom >= QUEUE_SIZE - 1)
                        {
                            queueBottom = 0;
                        }
                        else
                        {
                            queueBottom++;
                        }
                    }
                }

                index += 4;
            }

            s_recvSize = 0;

            if (!s_recvSize)
            {
                /* Schedule buffer for next receive event */
                error = USB_DeviceMidiRecv(handle,
                                           USB_MIDI_BULK_OUT_ENDPOINT,
                                           s_currRecvBuf,
                                           g_MidiEndpoints[1].maxPacketSize);
            }
            break;
        }
    }
    error = kStatus_USB_Success;
    return error;
}

/*!
 * @brief MIDI device set configuration function.
 *
 * This function sets configuration for msc class.
 *
 * @param handle The msc class handle.
 * @param configure The msc class configure index.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t USB_DeviceMidiSetConfigure(class_handle_t handle, uint8_t configure)
{
    usb_status_t error = kStatus_USB_Success;

    /* Schedule buffer for receive */
    USB_DeviceMidiRecv(
      handle, USB_MIDI_BULK_OUT_ENDPOINT, s_currRecvBuf, g_MidiEndpoints[1].maxPacketSize);

    return error;
}

/*!
 * @brief MIDI device initialization function.
 *
 * This function initializes the device with the composite device class information.
 *
 * @param deviceComposite The pointer to the composite device structure.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t USB_DeviceMidiImplInit(usb_device_composite_struct_t* deviceComposite)
{
    g_deviceComposite = deviceComposite;
    return kStatus_USB_Success;
}

//-----------------------------------------------------------------------------------------------

bool midiUsbAvailable()
{
    return !(queueBottom == queueTop);
}

void midiUsbSend(MidiUsbMessage* midiMessage)
{
    if (midiMessage->type < 0xF0)
    {
        if (midiMessage->type < 0x80)
        {
            return;
        }
        midiMessage->type &= 0xF0;
        midiUsbTx((midiMessage->type << 8) | (midiMessage->type >> 4) |
                  (((midiMessage->channel - 1) & 0x0F) << 8) | ((midiMessage->data1 & 0x7F) << 16) |
                  ((midiMessage->data2 & 0x7F) << 24));
    }
    else if (midiMessage->type >= 0xF8 || midiMessage->type == 0xF6)
    {
        midiUsbTx((midiMessage->type << 8) | 0x0F);
    }
    else if (midiMessage->type == 0xF1 || midiMessage->type == 0xF3)
    {
        midiUsbTx((midiMessage->type << 8) | 0x02 | ((midiMessage->data1 & 0x7F) << 16));
    }
    else if (midiMessage->type == 0xF2)
    {
        midiUsbTx((midiMessage->type << 8) | 0x03 | ((midiMessage->data1 & 0x7F) << 16) |
                  ((midiMessage->data2 & 0x7F) << 24));
    }
}

bool midiUsbReceive(MidiUsbMessage* midiMessage)
{
    uint32_t primask = DisableGlobalIRQ();

    if (queueBottom == queueTop)
    {
        EnableGlobalIRQ(primask);
        return false;
    }

    *midiMessage = midiMessageQueue[queueBottom];

    if (queueBottom >= QUEUE_SIZE - 1)
    {
        queueBottom = 0;
    }
    else
    {
        queueBottom++;
    }

    EnableGlobalIRQ(primask);
    return true;
}

void midiUsbTx(uint32_t packet)
{
    uint32_t primask = DisableGlobalIRQ();

    if (sendBufSize[currentInputBuf] < SEND_BUFF_SIZE)
    {
        ((uint32_t*)(&sendBuf[currentInputBuf][0]))[sendBufSize[currentInputBuf] / 4] = packet;
        sendBufSize[currentInputBuf] += 4;

#ifdef DEBUG_PRINT_MIDI_OUT_STATUS
        DbgConsole_Printf("t: %8d used bufs: %1d   add to buf num: %d, len: %d\n",
                          (DWT->CYCCNT - lastClkCount) / 528,
                          currentInputBuf >= currentSendBuf
                            ? currentInputBuf - currentSendBuf
                            : SEND_BUFF_COUNT - currentSendBuf + currentInputBuf,
                          currentInputBuf,
                          sendBufSize[currentInputBuf]);
        lastClkCount = DWT->CYCCNT;
#endif

        if (sendBufSize[currentInputBuf] >= SEND_BUFF_SIZE)
        {
            int nextBuf = currentInputBuf >= SEND_BUFF_COUNT - 1 ? 0 : currentInputBuf + 1;
            if (nextBuf != currentSendBuf)
            {
                currentInputBuf = nextBuf;
                sendBufSize[currentInputBuf] = 0;
            }
        }
    }

    midiSendPacket();

    EnableGlobalIRQ(primask);
}

bool midiSendPacket()
{
    if (!((usb_device_midi_struct_t*)g_deviceComposite->midi.midiHandle)->bulkIn.isBusy)
    {
        if (sendBufSize[currentSendBuf] > 0)
        {
            if (currentInputBuf == currentSendBuf)
            {
                currentInputBuf = currentInputBuf >= SEND_BUFF_COUNT - 1 ? 0 : currentInputBuf + 1;
                sendBufSize[currentInputBuf] = 0;

#ifdef DEBUG_PRINT_MIDI_OUT_STATUS
                DbgConsole_Printf("t: %8d used bufs: %1d  set input buf to: %d \n",
                                  (DWT->CYCCNT - lastClkCount) / 528,
                                  currentInputBuf >= currentSendBuf
                                    ? currentInputBuf - currentSendBuf
                                    : SEND_BUFF_COUNT - currentSendBuf + currentInputBuf,
                                  currentInputBuf);
                lastClkCount = DWT->CYCCNT;
#endif
            }

            USB_DeviceMidiSend(g_deviceComposite->midi.midiHandle,
                               USB_MIDI_BULK_IN_ENDPOINT,
                               (uint8_t*)&sendBuf[currentSendBuf][0],
                               sendBufSize[currentSendBuf]);

            return true;
        }
    }
    return false;
}
