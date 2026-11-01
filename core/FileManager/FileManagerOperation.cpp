#include "FileManagerOperation.h"

#include "fileManager.h"
#include "fileManagerDefs.h"
#include "fileTransfer.h"
#include "mtStructs.h"

FileSystem* FileManagerOperation::fileSystem = nullptr;

void FileManagerOperation::init(FileSystem* fileSystem)
{
    FileManagerOperation::fileSystem = fileSystem;
};

void FileManagerOperation::stopOperationWithError(uint8_t errorId)
{
    newFileManager.stopOperationWithError(errorId);
};
