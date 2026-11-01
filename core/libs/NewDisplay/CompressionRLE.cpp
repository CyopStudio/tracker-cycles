#include "CompressionRLE.h"

void decompress(const CompressedData* compressed, int compressed_size, uint8_t* decompressed)
{
    int index = 0;
    for (int i = 0; i < compressed_size; i++)
    {
        for (int j = 0; j < compressed[i].count; j++)
        {
            decompressed[index] = compressed[i].value;
            index++;
        }
    }
}

// use these functions to generate compressed arrays in e.g compiler online

// CompressedData* compress(const uint8_t* data, int size, int& compressed_size)
//{
//    CompressedData* compressed = new CompressedData[size];
//    compressed_size = 0;
//    int count = 0;
//    int value = data[0];
//
//    for (int i = 0; i < size; i++)
//    {
//        if (data[i] == value && count < std::numeric_limits<uint8_t>::max() )
//        {
//            count++;
//        }
//        else
//        {
//            compressed[compressed_size].value = value;
//            compressed[compressed_size].count = count;
//            compressed_size++;
//            value = data[i];
//            count = 1;
//        }
//    }
//    compressed[compressed_size].value = value;
//    compressed[compressed_size].count = count;
//    compressed_size++;
//    return compressed;
//}

// void printCompressed(CompressedData * data, int compressedSize, std::string name)
//{
//    cout<<"const CompressedData "<<name<<"["<<compressedSize<<"] = {\n";
//
//    for(int idx = 0; idx < compressedSize; idx++)
//    {
//        cout<<"{"<<(int)data[idx].value<<" , "<<(int)data[idx].count<<"},\n";
//    }
//    cout<<"};\n";
//}
