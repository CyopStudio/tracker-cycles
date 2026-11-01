#ifndef INCLUDE_CONFIGMANAGER_H_
#define INCLUDE_CONFIGMANAGER_H_

class ConfigManager
{
  public:
    virtual bool readData(int address, void* data, int size) = 0;
    virtual void writeData(int address, void* data, int size) = 0;
};

#endif /* INCLUDE_CONFIGMANAGER_H_ */
