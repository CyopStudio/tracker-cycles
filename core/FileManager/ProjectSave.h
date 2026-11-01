#ifndef FILEMANAGER_PROJECTSAVE_H_
#define FILEMANAGER_PROJECTSAVE_H_

#include "FileManagerOperation.h"

#include <stdint.h>

class ProjectSave : public FileManagerOperation
{
  public:
    virtual void update() override;
    virtual float getProgress() override;

    bool saveProject(char* projectPathToSave, char* projectNameToSave);

  private:
    enum ProjectSaveStep
    {
        initStep,
        copyProjectFile,
        copyPatternFiles,
        copyInstrumentFiles,
        finishStep,
    };

    ProjectSaveStep operationStep;

    int currentInstrument;
    int currentPattern;
    uint8_t copyStep;

    uint32_t instrumentsSizeTotal;
    uint32_t instrumentsSizeCopied;

    bool isIncrementalSave = false;
    char** existingInstrumentsList = nullptr;
    uint32_t existingInstrumentsSizeOut = 0;

    void saveProjectInit();
    void copyProject();
    bool copyPaterns(int& idx);
    bool copyInstruments(int idx, uint8_t& copyState);
    void saveProjectFinish();

    void listProjectExistingInstruments();
    void clearExistingInstrumentList();
    void operationError();

    uint32_t calcWorkspaceInstrumentsSize();
};

#endif /* FILEMANAGER_PROJECTSAVE_H_ */
