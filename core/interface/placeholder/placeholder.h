// Tracker Cycles (plan 4a): a screen for a module slot whose stock module
// was removed (pattern / song editor, sample recorder, performance mode).
// Interface::modules[], the function-key menu and the saved key bindings
// index these slots, so they stay; this shows a title and a line of text
// and lets every module button switch away. The pattern editor's slot has
// the Tidal editor (interface/tidalEditor), or tidalPlaceholder in the test
// images built without it.
#ifndef INTERFACE_PLACEHOLDER_H_
#define INTERFACE_PLACEHOLDER_H_

#include "core/modulesBase.h"

class cPlaceholder : public ModuleBase
{
  public:
    cPlaceholder(const char* title, const char* text) : title_(title), text_(text) {}

    void update() override {}
    void start(uint32_t options) override;
    void stop() override;
    void initDisplayControls() override;
    void destroyDisplayControls() override {}

  private:
    const char* title_;
    const char* text_;
    hControl label_[8] = {};
    hControl bgLabel_ = -1;
    hControl textBox_ = -1;
};

extern cPlaceholder songPlaceholder;
extern cPlaceholder recorderPlaceholder;
extern cPlaceholder performancePlaceholder;
extern cPlaceholder tidalPlaceholder;

#endif
