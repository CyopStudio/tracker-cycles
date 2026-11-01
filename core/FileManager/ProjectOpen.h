#ifndef FILEMANAGER_PROJECTOPEN_H_
#define FILEMANAGER_PROJECTOPEN_H_

#include "FileManagerOperation.h"

#include <stdint.h>

class ProjectOpen : public FileManagerOperation
{
  public:
    virtual void update() override;
    virtual float getProgress() override;

    bool openProject(char* path, char* name);

  private:
    enum ProjectOpenStep
    {
        initStep,
        loadProjectFile,
        saveProjectFile,
        copyPatternFiles,
        copyInstrumentFiles,
        copySampleFiles,
        finishStep,
    };

    ProjectOpenStep operationStep;

    int currentInstrument;
    int currentPattern;
    uint8_t copyStep;

    uint32_t sampleMemoryCopied;

    char** instrumentsList;
    uint32_t instrumentsSizeOut;

    uint32_t memoryToCopyTotal;
    uint32_t memoryToCopyDone;

    void openProjectInit();
    void loadProjectFileFromSd();
    bool copyPaterns(int& idx);
    bool copyInstruments(int idx, uint8_t& copyState);
    bool copySamples(int idx, uint8_t& copyState);
    void openProjectFinish();

    void operationError();

    void listProjectNewInstruments();
    void clearInstrumentList();

    uint32_t calcProjectInstrumentsSize();
    uint32_t countProjectPatterns();
};

#endif /* FILEMANAGER_PROJECTOPEN_H_ */
