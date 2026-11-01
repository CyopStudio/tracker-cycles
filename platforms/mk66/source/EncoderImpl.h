#ifndef SOURCE_ENCODERIMPL_H_
#define SOURCE_ENCODERIMPL_H_
#include "Encoder.h"
#include "Platform.h"

class EncoderImpl : Encoder
{
  public:
    virtual void setAcceleration(int acceleration) override;
    virtual void setResolution(int resolution) override;

    void init();
    void update();

  private:
    uint32_t encTimer;
    cEncoder encoderDriver;
};

#endif /* SOURCE_ENCODERIMPL_H_ */
