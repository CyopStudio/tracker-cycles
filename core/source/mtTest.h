#ifndef SOURCE_MTTEST_H_
#define SOURCE_MTTEST_H_

#include <stdint.h>

#include "elapsedMillis.h"

enum enChecklist
{
    checkStart,
    checkScreen,
    checkInputs,
    checkUSB,
    checkRAM,
    checkMidi,
    checkAudio,
    checkRadio,
    checkBattery,
    checkEnd,

    checkCount
};

const uint8_t startCode[5] = { 47, 10, 45, 8, 43 };
const uint8_t startCodeMini[5] = { 7, 0, 6, 1, 0 };

class InputProcessor;
class Sgtl5000;
class Radio;
class SwitchGroup;
class Encoder;
class FileSystem;
class Timer;
class Battery;

class cTest
{
  public:
    void runTestingProcedure(InputProcessor* ip);
    void testLoop();
    void setCodecAudio(Sgtl5000* codec);
    void setRadio(Radio* radio);
    void setButtons(SwitchGroup* buttons);
    void setEncoder(Encoder* encoder);
    void setTimer(Timer* timer);
    void setBattery(Battery* battery);
    void setFileSystem(FileSystem* fileSystem) { this->fileSystem = fileSystem; }

    uint8_t runTestByCombinaion(uint8_t pad);
    uint8_t runTestByMiniCombinaion(uint8_t button);
    void AcceptButton();
    void DeclineButton();
    void skipButton();
    uint8_t blockSkip = 0;
    uint8_t getStatusViewState() { return hideStatus; }

    Radio* getRadio() { return radio; }
    int getTestPhase() { return testPhase; }
    int getMainStatus() { return mainStatus; }

    struct strInputs
    {
        uint8_t pads[48];
        uint8_t buttons[interfaceButtonsCount];
        uint8_t powerButton;
        uint8_t encoderL;
        uint8_t encoderR;
        int16_t encoderValue;

    } inputs;

    const uint8_t midiNotes[8] = { 12, 48, 17, 2, 37, 56, 16, 112 };
    const uint8_t midiVelos[8] = { 1, 48, 17, 2, 37, 56, 16, 127 };
    const uint8_t midiChannels[8] = { 1, 3, 5, 7, 9, 11, 13, 15 };
    uint8_t midiResults = 0;
    uint8_t midiCounter = 0;

    // radio
    char radioFreq[11];

    // ram

    uint32_t ramErrorsMax;
    uint32_t ramTestsPassed;

  private:
    static constexpr char kLineInTestFile[] = "/line_in_test.wav";
    static constexpr char kMicTestFile[] = "/mic_test.wav";

    void drawGui();
    void doTasks();

    void nextTest();

    void showStatus();
    void showMessage(const char* question1,
                     const char* question2,
                     const char* answer1,
                     const char* answer2);

    void showStart();
    void showEnd();

    void showScreenTest();
    void runScreenTest();

    void showInputsTest();
    void runInputsTest();

    void showUSBTest();

    void showRAMTest();
    void runRAMTest();

    void showMidiTest();
    void runMidiTest();

    void showAudioTest();
    void runAudioTest();

    void showRadioTest();
    void runRadioTest();

    void showBatteryTest();
    void runBatteryTest();

    // funct
    void systemReset();
    void setAudiIO(uint8_t config);
    //

    // ogolny
    elapsedMillis guiRefresh;
    uint8_t procedureRunning = 0;
    uint8_t mainStatus = 0;
    uint8_t results[checkCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t hideStatus = 0;

    // konkretny aktualny test konkretnego elementu
    elapsedMillis testTimer;
    uint8_t testPhase = 0;

    uint8_t pos = 0;
    uint8_t ledBright = 0;

    InputProcessor* ip;
    Radio* radio;
    Sgtl5000* codec;
    SwitchGroup* buttons;
    Encoder* encoder;
    Timer* timer;
    Battery* battery;
    FileSystem* fileSystem;

    // audio
    void playSdFile(const char* filePath);
    void stopPlayingSdFile();
};

#define CM4_SCB_AIRCR                  (*(uint32_t*)0xE000ED0C)
#define CM4_SCB_AIRCR_VECTKEY_POS      16
#define CM4_SCB_AIRCR_SYSRESETREQ_MASK 0x04U

extern cTest mtTest;

#endif /* SOURCE_MTTEST_H_ */
