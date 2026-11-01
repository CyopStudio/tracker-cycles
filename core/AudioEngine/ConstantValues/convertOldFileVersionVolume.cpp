#include "convertOldFileVersionVolume.h"

#include "math.h"
#include "mtStructs.h"
#include "util_math.h"

// definicje dB używane są z wykorzystaniem wzoru 10*log(x/xref) - w modulach moga byc wyswietlane z
// wykorzystaniem 20*log(x/xref) w odniesieniu do amplitudy poprawna jest wartosc 20, ale zeby
// historyczna konwersja miala sens zakresy zostana z wartoscia 10
constexpr float OLD_VOLUME_VALUE_1_IN_DB_1117 = -19.8f;
constexpr float OLD_VOLUME_VALUE_100_IN_DB_1117 = 0.0f;

constexpr float NEW_MIX_VOLUME_VALUE_1_IN_DB_1118 = -36.0f;
constexpr float NEW_MIX_VOLUME_VALUE_100_IN_DB_1118 = 6.0f;

constexpr float NEW_INSTRUMENT_VOLUME_VALUE_1_IN_DB_1118 = -24.0f;
constexpr float NEW_INSTRUMENT_VOLUME_VALUE_100_IN_DB_1118 = 24.0f;

constexpr float NEW_LIMITER_THRESHOLD_VALUE_1_IN_DB_112 = -36.3f;
constexpr float NEW_LIMITER_THRESHOLD_VALUE_100_IN_DB_112 = 0.0f;
// konwersja z 10*log(x/xref) na 20*log(x/ref) 1.3 do 1.3.2

constexpr float OLD_INSTR_VOLUME_VALUE_1_IN_DB_1300 = NEW_INSTRUMENT_VOLUME_VALUE_1_IN_DB_1118;
constexpr float OLD_INSTR_VOLUME_VALUE_100_IN_DB_1300 = NEW_INSTRUMENT_VOLUME_VALUE_100_IN_DB_1118;
constexpr float NEW_INSTR_VOLUME_VALUE_1_IN_DB_1302 = -12.0f;
constexpr float NEW_INSTR_VOLUME_VALUE_100_IN_DB_1302 = 12.0f;

constexpr float OLD_MIXER_VOLUME_VALUE_1_IN_DB_1300 = NEW_MIX_VOLUME_VALUE_1_IN_DB_1118;
constexpr float OLD_MIXER_VOLUME_VALUE_100_IN_DB_1300 = NEW_MIX_VOLUME_VALUE_100_IN_DB_1118;
constexpr float NEW_MIXER_VOLUME_VALUE_1_IN_DB_1302 = -18.0f;
constexpr float NEW_MIXER_VOLUME_VALUE_100_IN_DB_1302 = 3.0f;

uint8_t ConvertOldFileVersionVolume::convertMixerVolumeVersion1117to1118(uint8_t oldValue)
{
    return convertVolume(oldValue,
                         OLD_VOLUME_VALUE_1_IN_DB_1117,
                         OLD_VOLUME_VALUE_100_IN_DB_1117,
                         NEW_MIX_VOLUME_VALUE_1_IN_DB_1118,
                         NEW_MIX_VOLUME_VALUE_100_IN_DB_1118);
}
uint8_t ConvertOldFileVersionVolume::convertInstrumentVolumeVersion1117to1118(uint8_t oldValue)
{
    return convertVolume(oldValue,
                         OLD_VOLUME_VALUE_1_IN_DB_1117,
                         OLD_VOLUME_VALUE_100_IN_DB_1117,
                         NEW_INSTRUMENT_VOLUME_VALUE_1_IN_DB_1118,
                         NEW_INSTRUMENT_VOLUME_VALUE_100_IN_DB_1118);
}
uint16_t ConvertOldFileVersionVolume::convertLimiterThresholdVersion110to112(uint16_t oldValue)
{
    return convertLinearVolume(oldValue,
                               MAX_SIGNED_16BIT,
                               NEW_LIMITER_THRESHOLD_VALUE_1_IN_DB_112,
                               NEW_LIMITER_THRESHOLD_VALUE_100_IN_DB_112);
}

uint8_t ConvertOldFileVersionVolume::convertMixerVolumeVersion1300to1302(uint8_t oldValue)
{
    return convertVolume(oldValue,
                         OLD_MIXER_VOLUME_VALUE_1_IN_DB_1300,
                         OLD_MIXER_VOLUME_VALUE_100_IN_DB_1300,
                         NEW_MIXER_VOLUME_VALUE_1_IN_DB_1302,
                         NEW_MIXER_VOLUME_VALUE_100_IN_DB_1302);
}
uint8_t ConvertOldFileVersionVolume::convertInstrumentVolumeVersion1300to1302(uint8_t oldValue)
{
    return convertVolume(oldValue,
                         OLD_INSTR_VOLUME_VALUE_1_IN_DB_1300,
                         OLD_INSTR_VOLUME_VALUE_100_IN_DB_1300,
                         NEW_INSTR_VOLUME_VALUE_1_IN_DB_1302,
                         NEW_INSTR_VOLUME_VALUE_100_IN_DB_1302);
}

uint8_t ConvertOldFileVersionVolume::convertVolume(uint8_t oldValue0To100Range,
                                                   float oldMinInDb,
                                                   float oldMaxInDb,
                                                   float newMinInDb,
                                                   float newMaxInDb)
{
    if (!oldValue0To100Range)
        return oldValue0To100Range;

    float volumeInDB = map((float)oldValue0To100Range, 1.0f, 100.0f, oldMinInDb, oldMaxInDb);

    float result = map(volumeInDB, newMinInDb, newMaxInDb, 1.0f, 100.0f);

    if (result > 0.0f)
        result += 0.5f;

    result = clamp(result, 1.0f, 100.0f);
    return (uint8_t)result;
}

uint16_t ConvertOldFileVersionVolume::convertLinearVolume(uint16_t linearValue,
                                                          uint16_t linear0dbLevel,
                                                          float newMinInDb,
                                                          float newMaxInDb)
{
    if (!linearValue)
        return linearValue;

    float volumeInDB =
      10 * log10f((float)linearValue /
                  (float)linear0dbLevel); // wartosci z definicji są z mnoznikiem 10 nie 20

    float result = map(volumeInDB, newMinInDb, newMaxInDb, 1.0f, (float)linear0dbLevel);

    if (result > 0.0f)
        result += 0.5f;
    else
        result -= 0.5f;

    return (uint16_t)result;
}
