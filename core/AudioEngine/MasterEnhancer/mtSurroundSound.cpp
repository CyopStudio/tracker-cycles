#include "mtSurroundSound.h"
#include "Platform.h"
#include "mtAudioEngine.h"

void mtSurroundSound::setCodec(Sgtl5000* codec)
{
    mtSurroundSound::codec = codec;
}

void mtSurroundSound::enable(uint8_t init_width, mtSorroundSoundSelection init_selection)
{
    codec->enableSurroundSound(true);
    change(init_width, init_selection);
    isDisable = false;
}
void mtSurroundSound::change(uint8_t width, mtSorroundSoundSelection selection)
{
    constrainChangeParams(&width, &selection);
    Sgtl5000::SurroundSoundParams params;
    params.width = width;
    params.selection = (int)selection;

    codec->configureSurroundSound(&params);
}
void mtSurroundSound::disable()
{
    codec->enableSurroundSound(false);
    isDisable = true;
}
bool mtSurroundSound::isEffectDisable()
{
    return isDisable;
}

void mtSurroundSound::constrainChangeParams(uint8_t* width, mtSorroundSoundSelection* selection)
{
    if (*width > SURROUND_SOUND_WIDTH_MAX)
        *width = SURROUND_SOUND_WIDTH_MAX;
    else if (*width < SURROUND_SOUND_WIDTH_MIN)
        *width = SURROUND_SOUND_WIDTH_MIN;

    if ((uint8_t)*selection > SURROUND_SOUND_WIDTH_MAX)
        *selection = (mtSorroundSoundSelection)SURROUND_SOUND_WIDTH_MAX;
    else if ((uint8_t)*selection < SURROUND_SOUND_WIDTH_MIN)
        *selection = (mtSorroundSoundSelection)SURROUND_SOUND_WIDTH_MIN;
}
