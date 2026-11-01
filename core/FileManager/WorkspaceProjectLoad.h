#ifndef FILEMANAGER_WORKSPACEPROJECTLOAD_H_
#define FILEMANAGER_WORKSPACEPROJECTLOAD_H_

#include "FileManagerOperation.h"

#include <stdint.h>

class WorkspaceProjectLoad : public FileManagerOperation
{
  public:
    virtual void update() override;
    virtual float getProgress() override;

    bool openProjectFromWorkspace();

  private:
    enum WorkspaceProjectLoadStep
    {
        initStep,
        loadProjectFile,
        loadInstrumentStruct,
        loadInstrumentSample,
        loadPatternFile,
        finishStep,
    };

    WorkspaceProjectLoadStep operationStep;

    int currentInstrument;
    uint8_t sampleLoadStep;
    uint32_t currentInstrumentFileDataOffset;

    uint32_t memoryToLoadTotal;
    uint32_t memoryToLoadDone;

    void loadProjectFromWorkspaceInit();
    void loadProjectFromWorkspaceFinish();

    void loadProjectFileFromWorkspace();

    bool loadInstrStruct(int idx, uint32_t& instrumentFileDataOffsetOut);

    uint32_t calcWorkspaceInstrumentsSize();
};

#endif /* FILEMANAGER_WORKSPACEPROJECTLOAD_H_ */
