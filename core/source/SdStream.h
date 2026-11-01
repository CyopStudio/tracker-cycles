#ifndef SOURCE_SDSTREAM_H_
#define SOURCE_SDSTREAM_H_

#include "Platform.h"

struct strWavFileHeader;

class SdStream
{
  public:
    // bufferSize should be power of 2
    void init(FileSystem* fileSystem, void* buffer, int bufferSize);
    bool startStream(const char* filepath,
                     strWavFileHeader* header,
                     uint16_t from,
                     uint16_t to,
                     uint32_t* currentPosition,
                     uint32_t* endPosition,
                     uint8_t* fileType);
    void endCurrentStream();
    void update();
    int readFromStream(int bytes, void* buffer);
    bool isStreamActive();

  private:
    static constexpr int kWriteBuffers = 3;
    static constexpr int kReadBuffers = 1;
    static constexpr int kHelpingBuffers = 1;
    static constexpr int kNextReadBufferIdx = 0;

    bool isAnyEmptyBuffer();
    int getFirstEmptyBufferIdx();
    void switchBuffer();
    void fillBuffer(bool align = false);
    bool isBufferRead();
    bool handleFileDecapsulation(const char* path,
                                 strWavFileHeader* header,
                                 uint16_t from,
                                 uint16_t to,
                                 uint32_t* currentPosition,
                                 uint32_t* endPosition,
                                 uint8_t* fileType);
    void fillWavHeaderForInstrumentFile(strWavFileHeader* header, size_t length, int channels);
    void makeBufferInterleaved(uint8_t*& buffer, int channelDataLength);

    FileSystem* fileSystem;
    bool isActive = false;
    bool isFileOpened = false;
    int readIdx = 0;
    FileSystem::FileHandle wavfile = nullptr;

    struct Buffer
    {
        uint8_t* address;
        int storedDataSize;
        bool isFilled;
    };

    Buffer readBuffer;
    Buffer writeBuffers[kWriteBuffers];
    Buffer helpingBuffer;

    int bufferSize;
    bool isSourceInterleaved;
    int dataLength;
};

#endif /* SOURCE_SDSTREAM_H_ */
