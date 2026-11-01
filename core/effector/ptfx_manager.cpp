#include "ptfx_manager.h"
#include "Platform.h"
#include "TrackerImpl.h"
#include "mtAudioEngine.h"
#include "sampleEditor/sampleEditor.h"

#include <cr_section_macros.h>
#include <cstdio>
#include <cstring>
#include <strings.h>

extern uint8_t externalRamScratchBuff[];

__NOINIT(EXTERNAL_RAM) static PtfxEntry s_ptfxEntries[kMaxModularPlugins];
__NOINIT(EXTERNAL_RAM) PtfxHeader PtfxManager::s_activeHeader;
__NOINIT(EXTERNAL_RAM) char* PtfxManager::s_effectDisplayNames[kBuiltInEffectsCount + kMaxModularPlugins];

uint16_t PtfxManager::s_pluginCount = 0;
bool PtfxManager::s_scanned = false;

static PtfxEntry* getPluginEntry(uint16_t index)
{
    if (index >= kMaxModularPlugins)
        return nullptr;
    return &s_ptfxEntries[index];
}

void PtfxManager::scanEffects(bool force)
{
    if (s_scanned && !force)
        return;

    if (!s_scanned)
    {
        memset(&s_activeHeader, 0, sizeof(s_activeHeader));
    }
    s_pluginCount = 0;
    memset(s_effectDisplayNames, 0, sizeof(s_effectDisplayNames));

    // Initialize built-in effect names (0 .. 16)
    for (uint16_t i = 0; i < kBuiltInEffectsCount; i++)
    {
        s_effectDisplayNames[i] = effectNamesLabels[i];
    }

    TrackerImpl* tracker = TrackerImpl::getInstance();
    if (!tracker)
        return;
    Platform* platform = tracker->getPlatform();
    if (!platform)
        return;
    FileSystem* fs = platform->getFileSystem();
    if (!fs || !fs->isSdCardInserted())
        return;

    if (!fs->exists("/Effects"))
    {
        fs->mkdir(0, "/Effects");
        s_scanned = true;
        return;
    }

    // Use externalRamScratchBuff temporarily during scan to avoid static RAM overhead
    engine.getSdPlayer()->stop();
    char** scanFileList = reinterpret_cast<char**>(externalRamScratchBuff);
    char*  scanNamesBuffer = reinterpret_cast<char*>(externalRamScratchBuff + kMaxModularPlugins * sizeof(char*));
    int    scanNamesBufferSize = kMaxModularPlugins * 48;
    int    filesCount = kMaxModularPlugins;

    fs->listDirContents(
      "/Effects",
      FileSystem::DirContentType::kFiles,
      [](const char* name) {
          size_t len = strlen(name);
          return len > 5 && strcasecmp(name + len - 5, ".ptfx") == 0;
      },
      scanNamesBuffer,
      scanNamesBufferSize,
      48,
      filesCount,
      scanFileList);

    for (int i = 0; i < filesCount && s_pluginCount < kMaxModularPlugins; i++)
    {
        char filepath[64];
        snprintf(filepath, sizeof(filepath), "/Effects/%s", scanFileList[i]);

        PtfxHeader tempHeader;
        if (PtfxLoader::readHeader(filepath, tempHeader))
        {
#ifndef PLATFORM_IMX
            if (tempHeader.flags & PTFX_FLAG_IMX_ONLY)
                continue;
#endif
            PtfxEntry* entry = getPluginEntry(s_pluginCount);
            if (!entry)
                break;

            strncpy(entry->filename, scanFileList[i], sizeof(entry->filename) - 1);
            entry->filename[sizeof(entry->filename) - 1] = '\0';
            entry->flags = tempHeader.flags;

            if (tempHeader.display_name[0] != '\0')
            {
                strncpy(entry->displayName, tempHeader.display_name, sizeof(entry->displayName) - 1);
            }
            else
            {
                // Fallback to filename without extension
                strncpy(entry->displayName, entry->filename, sizeof(entry->displayName) - 1);
                size_t nameLen = strlen(entry->displayName);
                if (nameLen > 5 && strcasecmp(entry->displayName + nameLen - 5, ".ptfx") == 0)
                {
                    entry->displayName[nameLen - 5] = '\0';
                }
            }
            entry->displayName[sizeof(entry->displayName) - 1] = '\0';

            s_effectDisplayNames[kBuiltInEffectsCount + s_pluginCount] = entry->displayName;
            s_pluginCount++;
        }
    }

    s_scanned = true;
}

uint16_t PtfxManager::getPluginCount()
{
    return s_pluginCount;
}

const PtfxEntry* PtfxManager::getPlugin(uint16_t index)
{
    return getPluginEntry(index);
}

const char* PtfxManager::getPluginDisplayName(uint16_t index)
{
    PtfxEntry* entry = getPluginEntry(index);
    if (!entry)
        return "";
    return entry->displayName;
}

const PtfxHeader& PtfxManager::getActiveHeader()
{
    return s_activeHeader;
}

bool PtfxManager::loadPlugin(uint16_t index, mtEffectModular& modularEffect)
{
    if (index >= s_pluginCount)
        return false;

    PtfxEntry* entry = getPluginEntry(index);
    if (!entry)
        return false;

    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/Effects/%s", entry->filename);

    PtfxInitFunc         initFunc         = nullptr;
    PtfxSetParamFunc     setParamFunc     = nullptr;
    PtfxProcessBlockFunc processBlockFunc = nullptr;

    bool loaded = PtfxLoader::loadPlugin(
      filepath, s_activeHeader, initFunc, setParamFunc, processBlockFunc);

    if (!loaded)
        return false;

    modularEffect.configurePlugin(s_activeHeader,
                                 PtfxLoader::getScratchBuffer(),
                                 PtfxLoader::getMaxScratchSize(),
                                 PtfxLoader::getHostApi(),
                                 initFunc,
                                 setParamFunc,
                                 processBlockFunc);

    return true;
}

char** PtfxManager::getEffectListNames()
{
    return s_effectDisplayNames;
}

uint16_t PtfxManager::getTotalEffectListCount()
{
    return kBuiltInEffectsCount + s_pluginCount;
}
