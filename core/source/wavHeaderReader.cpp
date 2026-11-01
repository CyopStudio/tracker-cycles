#include "wavHeaderReader.h"
#include "TrackerImpl.h"

#include <string.h>

void readHeader(strWavFileHeader* header, FileSystem::FileHandle wavfile)
{

    FileSystem* fileSystem = TrackerImpl::getInstance()->getPlatform()->getFileSystem();

    struct strSubchunkHeader
    {
        uint8_t id[4];
        uint32_t size;
    } subchunkHeader;

    uint16_t subchunkCounter = 0;

    fileSystem->readFile(&subchunkHeader, 8, wavfile);

    if ((subchunkHeader.id[0] != 'R') || (subchunkHeader.id[1] != 'I') ||
        (subchunkHeader.id[2] != 'F') || (subchunkHeader.id[3] != 'F'))
    {
        memset(header, 0, sizeof(*header));
        return;
    }

    memcpy(&header->chunkId, subchunkHeader.id, 4);
    header->chunkSize = subchunkHeader.size;
    fileSystem->readFile(&header->waveId, 4, wavfile);

    while (subchunkCounter < 10)
    {
        fileSystem->readFile(&subchunkHeader, 8, wavfile);

        subchunkCounter++;

        if ((subchunkHeader.id[0] == 'f') && (subchunkHeader.id[1] == 'm') &&
            (subchunkHeader.id[2] == 't') && (subchunkHeader.id[3] == ' '))
        {
            memcpy(&header->subchunk1Id, subchunkHeader.id, 4);
            header->subchunk1Size = subchunkHeader.size;
            fileSystem->readFile(&header->AudioFormat, 16, wavfile);

            fileSystem->seekFileCur(
              (subchunkHeader.size % 2 == 1 ? subchunkHeader.size + 1 : subchunkHeader.size) - 16,
              wavfile);

            break;
        }
        else
        {
            fileSystem->seekFileCur(
              (subchunkHeader.size % 2 == 1 ? subchunkHeader.size + 1 : subchunkHeader.size),
              wavfile);
        }
    }
    if (subchunkCounter == 10)
    {
        memset(header, 0, sizeof(*header));
        return;
    }

    subchunkCounter = 0;

    while (subchunkCounter < 10)
    {
        fileSystem->readFile(&subchunkHeader, 8, wavfile);
        subchunkCounter++;

        if ((subchunkHeader.id[0] == 'd') && (subchunkHeader.id[1] == 'a') &&
            (subchunkHeader.id[2] == 't') && (subchunkHeader.id[3] == 'a'))
        {
            memcpy(&header->subchunk2Id, subchunkHeader.id, 4);
            header->subchunk2Size = subchunkHeader.size;
            break;
        }
        else
        {
            fileSystem->seekFileCur(
              (subchunkHeader.size % 2 == 1 ? subchunkHeader.size + 1 : subchunkHeader.size),
              wavfile);
        }
    }

    if (subchunkCounter == 10)
    {
        memset(header, 0, sizeof(*header));
        return;
    }
}
