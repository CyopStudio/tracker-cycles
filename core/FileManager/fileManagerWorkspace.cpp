#include "../tidal/tidal_editor.h"
#include "fileManagerDefs.h"
#include "mtStructs.h"

#include "mtSequencer.h"

#include "fileManager.h"

#include <cstring>

bool cFileManager::createNewProjectInWorkspace()
{
    addCurrentProjectToRecentList();

    clearWorkspace();
    createWorkspaceDirs();

    report(" crating new project ");

    strcpy(currentProjectName, cNewProjectName);
    currentProjectPatch[0] = 0;

    getDefaultProject(&mtProject);
    mtProject.values.projectNotSavedFlag = 2; // nie zapiany do folderu Projects
    mtProject.values.actualPattern = 1;

    // saveActualPattern(cWorkspacePatternsPath, mtProject.values.actualPattern);
    changesFlags.pattern[mtProject.values.actualPattern] = 1;

    changesFlags.project = 1;
    saveProjectFileToWorkspace();
    currentOperationStep = 0;
    tidalProjectNew();  // Tracker Cycles (plan 4e): the editor starts empty

    return true;
}

bool cFileManager::createWorkspaceDirs()
{
    if (!fileSystem->exists(cWorkspacePath))
    {
        fileSystem->mkdir(1, cWorkspacePath);
    }

    if (!fileSystem->exists(cWorkspaceInstrumentsPath))
    {
        fileSystem->mkdir(1, cWorkspaceInstrumentsPath);
    }

    if (!fileSystem->exists(cWorkspacePatternsPath))
    {
        fileSystem->mkdir(1, cWorkspacePatternsPath);
    }

    if (!fileSystem->exists(cWorkspaceSamplesPath))
    {
        fileSystem->mkdir(1, cWorkspaceSamplesPath);
    }

    return true;
}

void cFileManager::clearWorkspace()
{
    fileSystem->removeDirWithFiles(cWorkspacePath);
}
