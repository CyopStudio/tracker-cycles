#include "padToBeatNoteConverter.h"
#include "mtStructs.h"

PadToBeatNoteConverter padToBeatNoteConverter;

uint8_t PadToBeatNoteConverter::getNoteFromPad(uint8_t padNumber)
{
    if (padNumber > 47)
        padNumber = 47;

    return padNumber + FIRST_BEAT_SLICE_NOTE;
}

uint8_t PadToBeatNoteConverter::getPadForNote(uint8_t note)
{
    if (note > FIRST_BEAT_SLICE_NOTE + 47)
        note = FIRST_BEAT_SLICE_NOTE + 47;
    if (note < FIRST_BEAT_SLICE_NOTE)
        note = FIRST_BEAT_SLICE_NOTE;

    return note - FIRST_BEAT_SLICE_NOTE;
}
