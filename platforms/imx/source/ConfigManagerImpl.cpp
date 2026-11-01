#include "ConfigManagerImpl.h"
#include "FileSystemImpl.h"
#include "config.h"
#include "polyassert.h"

void ConfigManagerImpl::init(FileSystem* fileSystem)
{
    this->fileSystem = fileSystem;
}

bool ConfigManagerImpl::readData(int address, void* data, int size)
{
    assert(fileSystem);

    if (!fileSystem->exists(kConfigFilePath))
    {
        return false;
    }

    auto openFileInfo = fileSystem->openFile(kConfigFilePath);
    auto isOpen = std::get<0>(openFileInfo);
    auto file = std::get<1>(openFileInfo);

    if (isOpen)
    {
        fileSystem->seekFile(address, file);
        fileSystem->readFile(data, size, file);
        fileSystem->closeFile(file);
        return true;
    }

    return false;
}

void ConfigManagerImpl::writeData(int address, void* data, int size)
{
    assert(fileSystem);

    auto openFileInfo = fileSystem->openFile(kConfigFilePath, FileSystem::FileOpenMode::kWriteOpen);
    auto isOpen = std::get<0>(openFileInfo);
    auto file = std::get<1>(openFileInfo);

    if (isOpen)
    {
        fileSystem->seekFile(address, file);
        fileSystem->writeFile(data, size, file);
        fileSystem->closeFile(file);
    }
}
