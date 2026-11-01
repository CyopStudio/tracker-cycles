#ifndef AUDIOENGINE_PADTOBEATNOTECONVERTER_H_
#define AUDIOENGINE_PADTOBEATNOTECONVERTER_H_
#include "stdint.h"

class PadToBeatNoteConverter
{
  public:
    uint8_t getNoteFromPad(uint8_t padNumber);
    uint8_t getPadForNote(uint8_t note);
};

extern PadToBeatNoteConverter padToBeatNoteConverter;

#endif /* AUDIOENGINE_PADTOBEATNOTECONVERTER_H_ */
