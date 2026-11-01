#include "hidConnection.h"
#include "Tracker.h"
#include <Arduino.h>
#include <stdint.h>

hidConnection::hidConnection()
{
}

void hidConnection::handle()
{
    Timer* timer = Tracker::getInstance()->getPlatform()->getTimer();
    int n;
    memset(buffer, 0, 64);
    n = RawHID.recv(buffer, 0);
    if (n > 0)
    {
        switch (*(uint16_t*)&buffer[0])
        {
            case FRAMEID_MEMO_READ_SLOT:
                Serial.printf(
                  "read bank: %d slot %d\n", *(uint16_t*)&buffer[2], *(uint16_t*)&buffer[4]);

                if (memoReadSlot != NULL)
                    memoReadSlot(*(uint16_t*)&buffer[2], *(uint16_t*)&buffer[4]);
                else
                    Serial.println("set_memoReadSlot not defined");
                // Serial.printf("NO KURWA");

                mode = MODE_NONE;
                break;

            case FRAMEID_MEMO_WRITE_SLOT:

            	timeMs = timer->millis();
                writeCounter = 0;    // licznik paczek przychodzących
                mode = MODE_WRITING; // ustawiamy tryb dla rawdata

                uint16_t size, bank, slot;
                size = *(uint16_t*)&buffer[2];
                bank = *(uint16_t*)&buffer[4];
                slot = *(uint16_t*)&buffer[6];

                //			Serial.printf("MEMO_WRITE_SLOT, size %d, bank %d, slot: %d\n",
                //							size,
                //							bank,
                //							slot);

                writeBytesLeft = size;
                if (getWritePtr != NULL)
                {
                    writingPtr = getWritePtr(bank, slot);
                    bankToSave = bank;
                    slotToSave = slot;
                }
                else
                {
                    mode = MODE_NONE;
                }

                break;

            case FRAMEID_WT_WRITE:

            	timeMs = timer->millis();
                writeCounter = 0;    // licznik paczek przychodzących
                mode = MODE_WRITING; // ustawiamy tryb dla rawdata

                uint16_t waves, samples;

                bank = *(uint16_t*)&buffer[2];
                slot = *(uint16_t*)&buffer[4];
                waves = *(uint16_t*)&buffer[6];
                samples = *(uint16_t*)&buffer[8];

                Serial.printf("WT_WRITE, bank %d, slot %d, waves: %d, samples: %d\n",
                              bank,
                              slot,
                              waves,
                              samples);

                writeBytesLeft = waves * samples * 2;
                wtDataToGet = (writeBytesLeft / 62) + 1;
                Serial.printf("wtDataToGet: %d\n", wtDataToGet);

                if (getWritePtr != NULL)
                {
                    writingPtr = getWritePtr(bank, slot);
                    bankToSave = bank;
                    slotToSave = slot;
                }
                else
                {
                    mode = MODE_NONE;
                }

                break;

            case FRAMEID_RAWDATA:

                // Serial.println("RAWDATA..");
                if (mode == MODE_WRITING && writingPtr != NULL)
                {
                    //				Serial.print("writeBytesLeft: ");
                    //				Serial.println(writeBytesLeft);
                    if (writeBytesLeft >= 62)
                    {
                        memcpy(writingPtr + writeCounter * 62, &buffer[2], 62);
                        writeBytesLeft -= 62;
                    }
                    else
                    {
                        memcpy(writingPtr + writeCounter * 62, &buffer[2], writeBytesLeft);
                        writeBytesLeft = 0;
                    }

                    writeCounter++;
                }
                else
                {
                    Serial.println("RAWDATA error");
                }

                break;

            case FRAMEID_WT_DATA:

                //			Serial.printf("FRAMEID_WT_DATA..%d\n", writeCounter);
                if (mode == MODE_WRITING && writingPtr != NULL)
                {
                    //				Serial.print("writeBytesLeft: ");
                    //				Serial.println(writeBytesLeft);
                    if (writeBytesLeft >= 62)
                    {
                        memcpy(writingPtr + writeCounter * 62, &buffer[2], 62);
                        writeBytesLeft -= 62;
                    }
                    else
                    {
                        memcpy(writingPtr + writeCounter * 62, &buffer[2], writeBytesLeft);
                        writeBytesLeft = 0;
                    }

                    writeCounter++;
                }
                else
                {
                    Serial.println("WT_DATA error");
                }

                break;

            case FRAMEID_RAWDATA_END:

                if (mode == MODE_WRITING && writingPtr != NULL)
                {
                    uint16_t size = *(uint16_t*)&buffer[2];
                    uint32_t crcFromFile = *(uint32_t*)&buffer[4];

                    uint32_t crc;
                    FastCRC32 objCrc;
                    crc = objCrc.crc32(writingPtr, size);

                    if (crcFromFile == crc)
                    {
                        Serial.println("CRC ok");
                        bool saveOk = memoSaveSlot(bankToSave, slotToSave);
                        uint32_t tempTimer = timer->millis() - timeMs;
                        Serial.printf("time of transaction: %dms\n", tempTimer);

                        sendReport(saveOk);
                    }
                    else
                    {
                        Serial.println("CRC bad!: ");
                        Serial.printf(" file:%X,\n calc: %X\n size:%d\nwriteCounter: %d\n",
                                      crcFromFile,
                                      crc,
                                      size,
                                      writeCounter);
                        sendReport(0);
                        //					Serial.println(writeCounter);
                    }

                    writeCounter = 0;
                    mode = MODE_NONE;
                }
                else
                {
                    Serial.println("RAWDATA_END error");
                }
                break;

            case FRAMEID_WT_DATA_END:

                Serial.printf("WT_DATA_END, writeCounter %d\n", writeCounter);
                if (mode == MODE_WRITING && writingPtr != NULL)
                {

                    if (wtDataToGet == writeCounter)
                    {
                        Serial.println("writeCounter ok");
                        bool saveOk = memoSaveSlot(bankToSave, slotToSave);
                        uint32_t tempTimer = timer->millis() - timeMs;
                        Serial.printf("time of transaction: %dms\n", tempTimer);

                        sendReport(saveOk);
                    }
                    else
                    {
                        Serial.println("writeCounter bad!: ");
                        sendReport(0);
                        //					Serial.println(writeCounter);
                    }

                    writeCounter = 0;
                    mode = MODE_NONE;
                }
                else
                {
                    if (debugMode)
                        Serial.println("RAWDATA_END error");
                }
                break;

            case FRAMEID_MEMO_COPY_FROM_TO:
                uint16_t fromBank, fromSlot, toBank, toSlot;

                fromBank = *(uint16_t*)&buffer[2];
                fromSlot = *(uint16_t*)&buffer[4];
                toBank = *(uint16_t*)&buffer[6];
                toSlot = *(uint16_t*)&buffer[8];

                if (memoCopyFromTo(fromBank, fromSlot, toBank, toSlot))
                {
                    sendReport(1);
                    if (debugMode)
                        Serial.println("copy OK!");
                }
                else
                {
                    sendReport(0);
                }

                break;

            case FRAMEID_SET_PCCONNECTION:

                uint16_t val;
                val = *(uint16_t*)&buffer[2];

                if (setPcConnection != NULL)
                    setPcConnection(val);

                sendReport(1);

                break;
            case FRAMEID_DEBUG_MODE:

                debugMode = *(uint16_t*)&buffer[2];
                Serial.printf("debugMode = %d\n", debugMode);
                break;

            case FRAMEID_SEND_BUTTON_STATE:

                uint16_t buttonId, state;
                buttonId = *(uint16_t*)&buffer[2];
                state = *(uint16_t*)&buffer[4];

                if (debugMode)
                    Serial.printf("button %d, state: %d\n", buttonId, state);
                if (sendButtonState != NULL)
                    sendButtonState(buttonId, state);

                break;

            default:
                Serial.println("unknown frameID");
                mode = MODE_NONE;
                break;
        }
    }
}

void hidConnection::sendReport(uint16_t val)
{
    uint8_t report[64]{ 0 };
    *(uint16_t*)&report[0] = FRAMEID_SAVE_OK;
    *(uint16_t*)&report[2] = val;
    // wyslij raport zapisu
    RawHID.send(report, 100);
}

void hidConnection::set_setPcConnection(void (*func)(uint16_t val))
{
    setPcConnection = func;
}

void hidConnection::set_memoReadSlot(void (*func)(uint16_t bank, uint16_t slot))
{
    memoReadSlot = func;
}
void hidConnection::set_sendButtonState(void (*func)(uint16_t buttonId, uint16_t state))
{
    sendButtonState = func;
}

void hidConnection::set_memoSaveSlot(bool (*func)(uint16_t bank, uint16_t slot))
{
    memoSaveSlot = func;
}

void hidConnection::set_memoCopyFromTo(bool (*func)(uint16_t, uint16_t, uint16_t, uint16_t))
{
    memoCopyFromTo = func;
}

void hidConnection::set_getWritePtr(uint8_t* (*func)(uint16_t bank, uint16_t slot))
{
    getWritePtr = func;
}

void hidConnection::sendMemo(uint8_t* ptr, uint16_t size, uint16_t bank, uint16_t slot)
{
    Serial.printf("sendMemo, size %d, bank %d, slot: %d", size, bank, slot);

    // RAMKA NAGŁÓWKOWA 64B
    // FRAMEID 2B || SIZE 2B || BANK 2B || SLOT 2B

    int32_t sizeLeft = size;
    uint8_t buff[64]{ 0 };
    *(uint16_t*)&buff[0] = FRAMEID_MEMO_WRITE_SLOT; // twprzymy plik do importu, czytając go w drugą
                                                    // strone chcemy zapisać
    *(uint16_t*)&buff[2] = size;
    *(uint16_t*)&buff[4] = bank;
    *(uint16_t*)&buff[6] = slot;

    // buff[0] = 9;
    // buff[1] = 8;
    // buff[2] = 7;

    RawHID.send(buff, 100);
    delay(20);
    // RawHID.send(buff, 100);

    // KONIEC RAMKI NAGŁÓWKOWEJ

    // RAMKI DANYCH
    // 2B FRAMEID +62B danych
    uint16_t a = 0;
    while (sizeLeft > 0)
    {
        memset(buff, 0, 64);
        *(uint16_t*)&buff[0] = FRAMEID_RAWDATA;

        if (sizeLeft > 62)
            memcpy(&buff[2], ptr + a * 62, 62);
        else
            memcpy(&buff[2], ptr + a * 62, sizeLeft);
        RawHID.send(buff, 100);
        delay(5);

        a++;
        sizeLeft -= 62;
    }
    // KONIEC RAMKI DANYCH

    uint32_t crc;
    FastCRC32 objCrc;
    crc = objCrc.crc32(ptr, size);

    memset(buff, 0, 64);
    *(uint16_t*)&buff[0] = FRAMEID_RAWDATA_END; // na końcu dajemy CRC
    *(uint16_t*)&buff[2] = size;
    *(uint32_t*)&buff[4] = crc; // TODO rozpisać CRC
    // *(uint16_t*) &buff[10] = 0xCCCC;

    Serial.printf("CRC: 0x%04X, %d frames\n", crc, a + 2);

    RawHID.send(buff, 100);
    delay(20);
}
