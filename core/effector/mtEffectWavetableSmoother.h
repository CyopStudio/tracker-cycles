#ifndef EFFECTOR_MTEFFECTWAVETABLESMOOTHER_H_
#define EFFECTOR_MTEFFECTWAVETABLESMOOTHER_H_
#include "mtEffect.h"

struct strEffectWavetableSmootherParams
{
    int windowSize;
    int windowsNumber;
};

class mtEffectWavetableSmoother : public mtEffect
{
  public:
    virtual void setParameter(float& value, int idx) override;
    virtual bool startProcess() override;
    virtual int32_t processBlock() override;
    virtual bool getProcessState() override;
    virtual uint32_t getExpectedProcessLength() override;

  private:
    bool detectChangeSourceWindow(
      int channel); // wykrywa kiedy probki zaczynaja byc pobierane z kolejnego okienka
    uint32_t getSmoothStepNumber(
      int channel); // wykrywa ile "międzyokienek" ma miec wygladzanie miedzy oryginalami

    struct ChannelParams
    {
        int16_t* dstAddr;
        int16_t* srcAddr;
        uint32_t sourceWindowMax;  // maksymalny numer okienka zrodlowego
        float smoothWindowStep;    // krok o jaki ma sie zmienic zrodlowe okienko - gdy przekracza
                                   // wartosci calkowite zmienia sie okienko
        float currentWindowNumber; // numer zrodlowego okienka wraz z zakumulowaną zmianą
                                   // zmiennoprzecinkowa
        uint32_t iCurrentWindowNumber; // zrzutowana wartosc zrodlowego okienka
        uint32_t iLastWindowNumber;
        float currentCrossfadeCoef; // mnoznik biezacego okienka (odpowiada za srednia wazona -
                                    // drugie okienko 1 - currentCrossfadeCoef)
        float crossfadeStep;        // zmiana currentCrossfadeCoef w kazdym kroku wygladzania
        uint32_t processedSamples = 0;
    };

    ChannelParams channelParams[kChannels];

    strEffectWavetableSmootherParams effectWavetableSmootherParams;

    uint32_t length;

    bool state = false;
};

extern mtEffectWavetableSmoother sampleEditorWavetableSmoother;

#endif /* EFFECTOR_MTEFFECTWAVETABLESMOOTHER_H_ */
