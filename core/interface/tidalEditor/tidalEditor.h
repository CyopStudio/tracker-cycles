// Tracker Cycles device editor (plan 4d step 3, -DTIDAL_EDITOR): the shared
// editor (tidal/editor) on the FT812, in the Pattern key's module slot in
// place of the Tidal status screen. Every button, pad and the jog go
// through the shared control mapping (tidal/editor/controls.*); Params,
// Perform, Master, the Sample keys, Config and File (unshifted) still switch
// modules; Shift+File saves /project.tidal, Shift+Config edits the block's
// comment, Shift+Master shows the status view.
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
    hControl code_ = -1, codeBg_ = -1, side_ = -1, menu_ = -1, bg_ = -1;
    uint32_t codeBgHash_ = 0;
    hControl label_[8] = {};
    uint32_t lastDrawMs_ = 0, lastStatusMs_ = 0, lastLitMs_ = 0;
    // what each control showed last (redraw refreshes only changed ones: the
    // display does one step per main-loop pass, and a full redraw of every
    // control took ~1 s while patterns played, RC5)
    uint32_t codeHash_ = 0, sideHash_ = 0, menuHash_ = 0, labelHash_[8] = {};
    bool forceAll_ = true, menuShown_ = false;
};

extern cTidalEditorModule tidalEditorModule;

#endif
