#include <stdint.h>
//#include "arduino.h"
#include <elapsedMillis.h>

#ifndef MT_ENVELOPEGENERATOR_H
#define MT_ENVELOPEGENERATOR_H

// #define CHANNEL_QTY 20
#define MAX_ADSR_PAR 4095

#define MAX_HOLD 4095

#define MAX_SUSTAIN 4095
#define MIN_OUTPUT  -1.0
#define MAX_OUTPUT  1.0

// #define ADSR_DEBG 1
class Timer;

class envelopeGenerator
{

  public:
    struct strEnv // do flash
    {
        float amount;     // 	= 1.0;
        uint16_t delay;   // 		= 0;
        uint16_t attack;  // 	= 20;
        uint16_t hold;    // 		= 0;
        uint16_t decay;   // 		= 30;
        float sustain;    // 	= 1.0;
        uint16_t release; // 	= 50;

        uint8_t loop; // 		= 0; 0=ADSR, 1=LFO, 2=WT Env, 3=WT LFO
        uint8_t enable;

        bool isAdsrEnv() const { return enable && loop == 0; }
        bool isStandardLfo() const { return enable && loop == 1; }
        bool isWtEnv() const { return enable && loop == 2; }
        bool isWtLfo() const { return enable && loop == 3; }
        bool isAnyLfo() const { return enable && (loop == 1 || loop == 3); }
        bool isAnyEnv() const { return enable && (loop == 0 || loop == 2); }
    };
    static_assert(sizeof(strEnv) == 20, "strEnv size must be 20 bytes for backward compatibility");

    static const strEnv s_defaultEnvelope;

    envelopeGenerator();

    void begin(Timer* timer);

    void init(strEnv* _env);

    void calc();
    void calcTime();
    void start();
    void stop();
    void kill();
    void killToZero();

    float getOut();
    float getLastOut();
    // float calcOutput();
    uint8_t getPhase() { return envTemp.phase; }
    uint8_t isKeyPressed() { return keyPressed; }

    void putMidiParameter(uint8_t ch, uint8_t parNo, uint8_t value);

    void syncTrackerSeq(uint32_t val, float seqSpeed);
    void setSync(uint8_t state);
    void calcSyncSeq();
    void setSyncStartStep(uint16_t n);
    void setPhaseNumbers(int8_t n1, int8_t n2);
    void setSyncRate(float sync);
    void setIsRandom(bool value);
    void setWavetableParams(uint8_t shape, uint8_t bend, uint8_t sust)
    {
        wtShape = shape;
        wtBend = bend;
        wtSust = sust;
    }
    void setWavetableDurationMs(float ms)
    {
        decay = ms * timeMul;
    }

  private:
    uint8_t wtShape = 0;
    uint8_t wtBend = 0;
    uint8_t wtSust = 0;
    float wtReleaseStartPhase = 0.0f;

    float fMap(float x, float in_min, float in_max, float out_min, float out_max);

    const float timeMul = 1000; // parametry ADSR są mnożone przez ten czynnik

    bool keyPressed = 0; // triggeruje cały envelope

    strEnv* envelope = const_cast<strEnv*>(&s_defaultEnvelope);
    bool isRandom;
    float randomValue;

    struct strEnvelopeTemp
    {
        // tylko do RAM
        float output = 0;     // wyjście adsra
        float tempOutput = 0; // czyste wyjście adsra, bez modu
        float maxOutput = 0;  // maksymalna osiągnięta wartość ( np w trakcie narastania attacku)

        float killOutput = 0;         // bufor warotści po kill
        elapsedMicrosAudio timer = 0; // timer fazy
        uint8_t phase = 0;            // aktualna faza

    } envTemp;

    uint16_t startStep = 0;
    int8_t phaseNumber[2] = { -1, -1 };
    float syncRate = 1;
    uint16_t periodTime = 0;
    uint32_t currentSeqTicks = 0;
    float currentSeqSpeed = 0;
    uint8_t isSync = 0;

    uint32_t lastTimer;

    float amount;
    float sustain;

    // time-dependent values
    float delay;
    float attack;
    float hold;
    float decay;
    float release;

    bool _loop;
    // uint8_t modTarget = 0;		//	jesli 0 to nieaktywny, jesli 1 to 0 w tablicy
    // float mod = 0;				// 	tu można wpisać z zewnątrz wartość modulacji
    // nr fazy
    enum adsrPhase
    {
        phase_nothing,  // 0
        phase_predelay, // 1
        phase_attack,   // 2
        phase_hold,     // 3
        phase_decay,    // 4
        phase_sustain,  // 5
        phase_release   // 6
    };

    // nr cc parametru
    enum parNumber
    {
        par_predelay = 40, // 40
        par_attack,        // 41
        par_hold,          // 42
        par_decay,         // 43
        par_sustain,       // 44
        par_release,       // 45
        par_amount         // 46
    };

    Timer* timer;
    uint32_t lastRandomTick = 0;
};

#endif
