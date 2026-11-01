#ifndef SOURCE_ENCODERIMPL_H_
#define SOURCE_ENCODERIMPL_H_
#include "Platform.h"

class EncoderImpl : Encoder
{
  public:
    virtual void setAcceleration(int acceleration) override;
    virtual void setResolution(int resolution) override;
#ifndef TRACKER_MINI
    void init();
    void update();
    void setChangeCallback(void (*changeCallback)(uint8_t, int16_t));

  private:
    static constexpr int kAccelerationLookupSize = 50;
    static constexpr int kAccelerationLevelMax = 5;
    static constexpr int kPulsesOnClick = 4;
    static constexpr int kUpdateTimeMs = 100;
    static constexpr int kResetPulsesTimeout = 2000;
    static const uint16_t accelerationLookupTable[kAccelerationLevelMax][kAccelerationLookupSize];
    int accelerate(int diff);
    void (*changeCallback)(uint8_t, int16_t) = nullptr;
    uint32_t accumulatedPositionPulses;
    uint32_t updateTime;
    uint32_t lastClickTime;
    int acceleration = 0;
#endif
};

#endif /* SOURCE_ENCODERIMPL_H_ */
