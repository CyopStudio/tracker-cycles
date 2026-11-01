
#ifndef INTERFACE_CORE_SONGTIMER_H_
#define INTERFACE_CORE_SONGTIMER_H_

#include "displayStructs.h"
#include "mtSequencer.h"

class Interface;

class cSongTimer
{
  public:
    void initControls();

    void show();
    void hide();

    void start();
    void stop();

    void refresh();

    void refreshColors();

    friend Interface;

  private:
    bool enabled = true;
    bool visible;
    bool counting;

    hControl timerLabel;

    char timerText[50];

    struct data
    {
        int minutes;
        uint8_t seconds;
        uint8_t pattern;
        uint8_t step;
        Sequencer::enSeqState seqState;
    } actualData, lastData;
};

extern cSongTimer songTimer;
extern elapsedMillisAudio songTimeCounter;

#endif /* INTERFACE_CORE_SONGTIMER_H_ */
