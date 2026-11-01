
#ifndef AUDIOENGINE_MASTERENHANCER_MTSURROUNDSOUND_H_
#define AUDIOENGINE_MASTERENHANCER_MTSURROUNDSOUND_H_

#include "stdint.h"

enum struct mtSorroundSoundSelection
{
    mono = 2,
    stereo
};

constexpr uint8_t SURROUND_SOUND_WIDTH_MAX = 7;
constexpr uint8_t SURROUND_SOUND_WIDTH_MIN = 0;

constexpr uint8_t SURROUND_SOUND_SELECTION_MAX = (uint8_t)mtSorroundSoundSelection::stereo;
constexpr uint8_t SURROUND_SOUND_SELECTION_MIN = (uint8_t)mtSorroundSoundSelection::mono;

class Sgtl5000;

class mtSurroundSound
{
  public:
    void setCodec(Sgtl5000* codec);
    void enable(uint8_t init_width, mtSorroundSoundSelection init_selection);
    void change(uint8_t width, mtSorroundSoundSelection selection);
    void disable();
    bool isEffectDisable();

  private:
    Sgtl5000* codec;
    bool isDisable = true;
    void constrainChangeParams(uint8_t* width, mtSorroundSoundSelection* selection);
};

#endif /* AUDIOENGINE_MASTERENHANCER_MTSURROUNDSOUND_H_ */
