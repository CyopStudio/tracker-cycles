#include "stdint.h"

// Gets the JPEG size from the array of data passed to the function, file reference:
// http://www.obrador.com/essentialjpeg/headerinfo.htm
uint8_t get_jpeg_size(uint8_t* data, int32_t data_size, uint16_t* width, uint16_t* height)
{
    // Check for valid JPEG image
    int i = 0; // Keeps track of the position within the file
    if (data[i] == 0xFF && data[i + 1] == 0xD8 && data[i + 2] == 0xFF && data[i + 3] == 0xE0)
    {
        i += 4;
        // Check for valid JPEG header (null terminated JFIF)
        if (data[i + 2] == 'J' && data[i + 3] == 'F' && data[i + 4] == 'I' && data[i + 5] == 'F' &&
            data[i + 6] == 0x00)
        {
            // Retrieve the block length of the first block since the first block will not contain
            // the size of file
            unsigned short block_length = data[i] * 256 + data[i + 1];
            while (i < data_size)
            {
                i += block_length; // Increase the file index to get to the next block
                if (i >= data_size)
                    return 0; // Check to protect against segmentation faults
                if (data[i] != 0xFF)
                    return 0; // Check that we are truly at the start of another block
                if (data[i + 1] == 0xC0)
                { // 0xFFC0 is the "Start of frame" marker which contains the file size
                    // The structure of the 0xFFC0 block is quite simple [0xFFC0][ushort
                    // length][uchar precision][ushort x][ushort y]
                    *height = data[i + 5] * 256 + data[i + 6];
                    *width = data[i + 7] * 256 + data[i + 8];
                    return 1;
                }
                else
                {
                    i += 2;                                     // Skip the block marker
                    block_length = data[i] * 256 + data[i + 1]; // Go to the next block
                }
            }
            return 0; // If this point is reached then no size was found
        }
        else
        {
            return 0;
        } // Not a valid JFIF string
    }
    else
    {
        return 0;
    } // Not a valid SOI header
}
