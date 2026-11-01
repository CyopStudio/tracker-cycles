#ifndef LIBS_NEWDISPLAY_COMPRESSIONRLE_H_
#define LIBS_NEWDISPLAY_COMPRESSIONRLE_H_

#include "stdint.h"

struct CompressedData
{
    uint8_t value;
    uint8_t count;
};

void decompress(const CompressedData* compressed, int compressed_size, uint8_t* decompressed);

#endif /* LIBS_NEWDISPLAY_COMPRESSIONRLE_H_ */
