// Tracker Cycles device editor (plan 4d step 3, -DTIDAL_EDITOR): the shared
// editor (tidal/editor) on the FT812, in the Pattern key's module slot in
// place of the Tidal status screen. Every button, pad and the jog go
// through the shared control mapping (tidal/editor/controls.*); Perform,
// Master, the Sample keys, Config and File (unshifted) still switch modules,
// Shift+File saves /project.tidal.
#ifndef INTERFACE_TIDALEDITOR_H_
#define INTERFACE_TIDALEDITOR_H_

#include "core/modulesBase.h"

class cTidalEditorModule : public ModuleBase
{
  public:
    void update() override;
    void start(uint32_t options) override;
    void stop() override;
    void initDisplayControls() override;
    void destroyDisplayControls() override;

    void redraw();

  private:
    hControl code_ = -1, side_ = -1, menu_ = -1, bg_ = -1;
    hControl label_[8] = {};
    uint32_t lastDrawMs_ = 0;
};

extern cTidalEditorModule tidalEditorModule;

#endif
