// Tracker Cycles (plan 4a stage 3): the Tidal status screen, in the old
// pattern editor's module slot (Pattern key) until the device editor (4d).
// Shows the file the player plays, every block's state (playing / error /
// mapping warning), cps, audio load and polyphony. Screen 1 = Reload,
// Screen 2 = Hush; Play = reload, Shift+Play = hush.
#ifndef INTERFACE_TIDALSTATUS_H_
#define INTERFACE_TIDALSTATUS_H_

#include "core/modulesBase.h"

class cTidalStatus : public ModuleBase
{
  public:
    static constexpr int kLines = 19;  // header, message, d1..d16, blank

    void update() override;
    void start(uint32_t options) override;
    void stop() override;
    void initDisplayControls() override;
    void destroyDisplayControls() override {}

    void refresh(bool force);

  private:
    hControl label_[8] = {};
    hControl bgLabel_ = -1;
    hControl line_[kLines] = {};
    uint32_t lastRefreshMs_ = 0;
};

extern cTidalStatus tidalStatus;

#endif
