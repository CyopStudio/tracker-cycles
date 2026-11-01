#ifndef PTFX_MANAGER_H_
#define PTFX_MANAGER_H_

#include "mtEffectModular.h"
#include "ptfx_loader.h"
#include "tracker_plugin.h"
#include <stdint.h>

static constexpr uint16_t kMaxModularPlugins = 128;
static constexpr uint16_t kBuiltInEffectsCount = 17;

struct PtfxEntry
{
    char       filename[42];
    char       displayName[32];
    uint16_t   flags;
};

class PtfxManager
{
  public:
    static void scanEffects(bool force = false);
    static uint16_t getPluginCount();
    static const PtfxEntry* getPlugin(uint16_t index);
    static const char* getPluginDisplayName(uint16_t index);

    // Loads the selected plugin into executable RAM and configures mtEffectModular
    static bool loadPlugin(uint16_t index, mtEffectModular& modularEffect);

    // Active loaded plugin header (single in-RAM instance)
    static const PtfxHeader& getActiveHeader();

    // Returns array of string pointers for cSampleEditor::effectListData
    static char** getEffectListNames();
    static uint16_t getTotalEffectListCount();

  private:
    static uint16_t s_pluginCount;
    static bool     s_scanned;
    static char*    s_effectDisplayNames[kBuiltInEffectsCount + kMaxModularPlugins];
    static PtfxHeader s_activeHeader;
};

#endif /* PTFX_MANAGER_H_ */
