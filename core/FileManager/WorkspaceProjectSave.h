#ifndef FILEMANAGER_WORKSPACEPROJECTSAVE_H_
#define FILEMANAGER_WORKSPACEPROJECTSAVE_H_

#include "FileManagerOperation.h"

#include <stdint.h>

class WorkspaceProjectSave : public FileManagerOperation
{
  public:
    virtual void update() override;

    bool saveProjectToWorkspace(bool forceSaveAll);
    void stop();

  private:
    enum WorkspaceProjectSaveStep
    {
        initStep,
        savePatternFile,
        saveInstrumentStruct,
        saveInstrumentSample,
        saveProjectFile,
        finishStep,
    };

    WorkspaceProjectSaveStep operationStep;

    int currentInstrument;
    bool saveCurrentInstrumentSampleData;
    uint8_t sampleLoadState;
    uint32_t currentInstrumentFileStructSize;

    void saveProjectToWorkspaceInit();
    void saveProjectToWorkspaceFinish();
};

#endif /* FILEMANAGER_WORKSPACEPROJECTSAVE_H_ */
