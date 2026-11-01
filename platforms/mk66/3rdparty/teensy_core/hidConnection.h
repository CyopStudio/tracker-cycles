#include "PlatformImpl.h"
#include <Arduino.h>
#include <FastCRC.h>
#include <stdint.h>
#ifndef HIDCONNECTION_H
#define HIDCONNECTION_H

class hidConnection
{
  public:
    static const uint16_t FRAMEID_RAWDATA = 0x01, FRAMEID_RAWDATA_END = 0x02,
                          FRAMEID_MEMO_READ_SLOT = 0x03, FRAMEID_MEMO_WRITE_SLOT = 0x04,
                          FRAMEID_MEMO_ERASE_SLOT = 0x05, FRAMEID_MEMO_COPY_FROM_TO = 0x06,
                          FRAMEID_MEMO_ERASE_ALL = 0x07, FRAMEID_SAVE_OK = 0x08,
                          FRAMEID_SET_PCCONNECTION = 0x21, FRAMEID_PAR_READ = 0x22,
                          FRAMEID_PAR_WRITE = 0x23, FRAMEID_WT_WRITE = 0x24, FRAMEID_WT_DATA = 0x25,
                          FRAMEID_WT_DATA_END = 0x26, FRAMEID_SEND_BUTTON_STATE = 0x27,
                          FRAMEID_DEBUG_MODE = 0xF0, FRAMEID_ABORT = 0xFA;

    static const uint8_t MODE_NONE = 0, MODE_WRITING = 1;

    hidConnection();
    hidConnection(uint8_t _debugMode) { debugMode = _debugMode; };
    void handle();

    void set_setPcConnection(void (*func)(uint16_t));
    void set_memoReadSlot(void (*func)(uint16_t, uint16_t));
    void set_sendButtonState(void (*func)(uint16_t, uint16_t));
    void set_memoSaveSlot(bool (*func)(uint16_t, uint16_t));
    void set_memoCopyFromTo(bool (*func)(uint16_t, uint16_t, uint16_t, uint16_t));

    void set_getWritePtr(uint8_t* (*func)(uint16_t, uint16_t));
    void sendMemo(uint8_t* ptr, uint16_t size, uint16_t bank, uint16_t slot);

  private:
    uint8_t buffer[64] = { 0 };

    uint8_t debugMode = 0;

    uint8_t mode = MODE_NONE;
    uint16_t writeCounter = 0;
    uint16_t writeBytesLeft = 0;
    uint16_t wtDataToGet = 0;
    uint16_t bankToSave = 0, slotToSave = 0;
    void (*setPcConnection)(uint16_t) = NULL;
    void (*memoReadSlot)(uint16_t, uint16_t) = NULL;
    void (*sendButtonState)(uint16_t, uint16_t) = NULL;
    bool (*memoSaveSlot)(uint16_t, uint16_t) = NULL;
    bool (*memoCopyFromTo)(uint16_t, uint16_t, uint16_t, uint16_t) = NULL;

    uint8_t* (*getWritePtr)(uint16_t, uint16_t) = NULL;
    uint8_t* writingPtr = NULL;
    void sendReport(uint16_t val);
    uint32_t timeMs = 0;
};

#endif
