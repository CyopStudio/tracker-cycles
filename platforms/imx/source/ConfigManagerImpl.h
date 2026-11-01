#ifndef SOURCE_CONFIGMANAGERIMPL_H_
#define SOURCE_CONFIGMANAGERIMPL_H_

#include "ConfigManager.h"

class FileSystem;

class ConfigManagerImpl : public ConfigManager
{
  public:
    virtual bool readData(int address, void* data, int size) override;
    virtual void writeData(int address, void* data, int size) override;

    void init(FileSystem* fileSystem);

  private:
    FileSystem* fileSystem;
};

#endif /* SOURCE_CONFIGMANAGERIMPL_H_ */
