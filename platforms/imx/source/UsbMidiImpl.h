#ifndef SOURCE_USBMIDIIMPL_H_
#define SOURCE_USBMIDIIMPL_H_
#include "Platform.h"
#include "usb_midi.h"

class UsbMidiImpl : public UsbMidi
{
  public:
    virtual bool isReadAvailable() override;
    virtual void read() override;
    virtual void sendRealTime(MessageType messageType) override;
    virtual void sendControlChange(uint8_t index, uint8_t value, uint8_t channel) override;
    virtual void sendProgramChange(uint8_t programNumber, uint8_t channel) override;
    virtual void sendAfterTouch(uint8_t velocity, uint8_t channel) override;
    virtual void sendPolyPressure(uint8_t note, uint8_t value, uint8_t channel) override;
    virtual void sendPitchBend(int value, uint8_t channel) override;
    virtual void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) override;
    virtual void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) override;
    virtual void setReceiveActions(MidiReceiveActions* receiveActions) override;

  private:
    void send(uint8_t type, uint8_t channel, uint8_t data1, uint8_t data2)
      __attribute__((always_inline))
    {
        MidiUsbMessage message;
        message.type = type;
        message.data1 = data1;
        message.data2 = data2;
        message.channel = channel;
        midiUsbSend(&message);
    }

    MidiReceiveActions midiReceiveActions;
};

#endif /* SOURCE_USBMIDIIMPL_H_ */
