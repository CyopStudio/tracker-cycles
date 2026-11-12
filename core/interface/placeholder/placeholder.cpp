// See placeholder.h.
#include "placeholder.h"

#include "core/interfaceMenuPopup.h"
#include "mtPadBoard.h"
#include "mtPadsBacklight.h"

cPlaceholder songPlaceholder("Song", "Song mode is not part of Tracker Cycles.");
cPlaceholder recorderPlaceholder("Sample Recorder", "Sample recording is not part of Tracker Cycles.");
cPlaceholder performancePlaceholder("Performance", "Performance mode is not part of Tracker Cycles.");
cPlaceholder tidalPlaceholder("Tidal", "This test image has no Tidal editor.");

namespace {

cPlaceholder* gActive = nullptr;

void functSwitchModule(uint8_t button)
{
    if (gActive)
        gActive->eventFunct(eventSwitchModule, gActive, &button, 0);
}

void functMenu(uint8_t state)
{
    if (!gActive)
        return;
    if (state == buttonPress)
        mtMenuPopup.showModulePopup(gActive);
    else if (state == buttonRelease)
        mtMenuPopup.hideModulePopup(gActive);
}

}  // namespace

void cPlaceholder::start(uint32_t)
{
    gActive = this;
    moduleRefresh = 1;

    ip->setButtonObj(interfaceButtonPerformance, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonParams, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonFile, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonConfig, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonMaster, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSamplePlay, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSampleEdit, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSampleRec, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSampleLoad, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSong, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonPattern, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonGame, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonMiniMenu, functMenu);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlText(label_[i], "");
        display.refreshControl(label_[i]);
    }
    titleBar->show();
    titleBar->setTitle(title_);
    display.setControlText(textBox_, text_);
    display.setControlShow(textBox_);
    display.refreshControl(textBox_);
    display.refreshControl(bgLabel_);
    display.synchronizeRefresh();
}

void cPlaceholder::stop()
{
    moduleRefresh = 0;
    display.setControlHide(textBox_);
    mtPadBoard.releaseAllInstruments();
    padsBacklight.clearGlobalPads(1, 1, 1);
    if (gActive == this)
        gActive = nullptr;
}

void cPlaceholder::initDisplayControls()
{
    strControlProperties prop;
    for (uint8_t i = 0; i < 8; i++)
    {
        prop.colors = display.getTheme().activeLabelsColors;
        prop.text = (char*)"";
        prop.style = (controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
        prop.x = (800 / 8) * i + (800 / 16);
        prop.y = 424;
        prop.w = 800 / 8 - 6;
        prop.h = 55;
        prop.value = 0;
        label_[i] = display.createControl<cLabel>(&prop);
    }

    strControlProperties prop2;
    prop2.text = nullptr;
    prop2.colors = nullptr;
    prop2.value = 255;
    prop2.style = controlStyleNoTransparency | controlStyleShow;
    prop2.x = 0;
    prop2.w = 800;
    prop2.y = 425;
    prop2.h = 54;
    bgLabel_ = display.createControl<cBgLabel>(&prop2);

    strControlProperties prop3;
    prop3.colors = display.getTheme().configItemEditorTextLabelColors;
    prop3.text = nullptr;
    prop3.data = nullptr;
    prop3.style = controlStyleFont5;
    prop3.value = 3;
    prop3.x = 20;
    prop3.y = 60;
    prop3.w = 760;
    prop3.h = 300;
    textBox_ = display.createControl<cTextBox>(&prop3);
}
