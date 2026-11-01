#ifndef AUDIOENGINE_CONSTANTVALUES_CONVERTOLDFILEVERSIONVOLUME_H_
#define AUDIOENGINE_CONSTANTVALUES_CONVERTOLDFILEVERSIONVOLUME_H_

#include "stdint.h"

class ConvertOldFileVersionVolume
{
  public:
    uint8_t convertMixerVolumeVersion1117to1118(uint8_t oldValue);
    uint8_t convertInstrumentVolumeVersion1117to1118(uint8_t oldValue);
    uint16_t convertLimiterThresholdVersion110to112(uint16_t oldValue);
    uint8_t convertMixerVolumeVersion1300to1302(uint8_t oldValue);
    uint8_t convertInstrumentVolumeVersion1300to1302(uint8_t oldValue);

  private:
    uint8_t convertVolume(uint8_t oldValue0To100Range,
                          float oldMinInDb,
                          float oldMaxInDb,
                          float newMinInDb,
                          float newMaxInDb);
    uint16_t convertLinearVolume(uint16_t linearValue,
                                 uint16_t linear0dbLevel,
                                 float newMinInDb,
                                 float newMaxInDb);
};

#endif /* AUDIOENGINE_CONSTANTVALUES_CONVERTOLDFILEVERSIONVOLUME_H_ */
