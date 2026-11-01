#ifndef FILEMANAGER_PATTERNSMETADATA_H_
#define FILEMANAGER_PATTERNSMETADATA_H_

#include <stdint.h>

class FileSystem;

class PatternsMetadata
{
  public:
    void init(FileSystem* fileSystem);
    void loadPatternsMetadata();
    bool copyPatternsMetadata(const char* src, const char* dest);

    char* getPatternName(int idx);
    void setPatternName(int idx, char* name);

  private:
    bool readWorkspacePatternsMetadata();
    void writeWorkspacePatternsMetadata();
    bool readPatternsMetadata(const char* path);
    void writePatternsMetadata(const char* path);
    bool validatePatternsMetadata();

    FileSystem* fileSystem;

    bool successfulRead;
};

#endif /* FILEMANAGER_PATTERNSMETADATA_H_ */
