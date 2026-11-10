// See tidalStatus.h.
#include "tidalStatus.h"

#include <stdio.h>
#include <string.h>

#include "core/interface.h"
#include "core/interfaceMenuPopup.h"
#include "cr_section_macros.h"
#include "mtPadBoard.h"
#include "mtPadsBacklight.h"
#include "../../tidal/tidal_player.h"

cTidalStatus tidalStatus;

namespace {

// label texts must outlive the labels (cLabel keeps the pointer)
__NOINIT(EXTERNAL_RAM) char gText[cTidalStatus::kLines][112];
__NOINIT(EXTERNAL_RAM) char gShown[cTidalStatus::kLines][112];

void functSwitchModule(uint8_t button)
{
    tidalStatus.eventFunct(eventSwitchModule, &tidalStatus, &button, 0);
}

void functMenu(uint8_t state)
{
    if (state == buttonPress)
        mtMenuPopup.showModulePopup(&tidalStatus);
    else if (state == buttonRelease)
        mtMenuPopup.hideModulePopup(&tidalStatus);
}

#ifdef TC_TIDAL_PLAYER
void functReload(uint8_t)
{
    tidalPlayerReload();
    tidalStatus.refresh(true);
}

void functHush(uint8_t)
{
    tidalPlayerHush();
    tidalStatus.refresh(true);
}

void functPlay()
{
    if (mtInterface.getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift))
        tidalPlayerHush();
    else
        tidalPlayerReload();
    tidalStatus.refresh(true);
}
#endif

}  // namespace

void cTidalStatus::start(uint32_t)
{
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
#ifdef TC_TIDAL_PLAYER
    ip->setButtonObj(interfaceButton0, buttonPress, functReload);
    ip->setButtonObj(interfaceButton1, buttonPress, functHush);
    ip->setButtonObj(interfaceButtonPlay, buttonPress, functPlay);
#endif

    for (uint8_t i = 0; i < 8; i++)
        display.setControlText(label_[i], "");
#ifdef TC_TIDAL_PLAYER
    display.setControlText(label_[0], "Reload");
    display.setControlText(label_[1], "Hush");
#endif
    for (uint8_t i = 0; i < 8; i++)
        display.refreshControl(label_[i]);

    titleBar->show();
    titleBar->setTitle("Tidal");
    display.refreshControl(bgLabel_);
    refresh(true);
}

void cTidalStatus::stop()
{
    moduleRefresh = 0;
    for (int i = 0; i < kLines; i++)
        display.setControlHide(line_[i]);
    mtPadBoard.releaseAllInstruments();
    padsBacklight.clearGlobalPads(1, 1, 1);
}

void cTidalStatus::update()
{
    if (!moduleRefresh)
        return;
    const uint32_t ms = mtInterface.getPlatform()->getTimer()->millis();
    if (ms - lastRefreshMs_ < 250)
        return;
    lastRefreshMs_ = ms;
    refresh(false);
}

void cTidalStatus::refresh(bool force)
{
    for (int i = 0; i < kLines; i++)
        gText[i][0] = 0;
#ifdef TC_TIDAL_PLAYER
    if (!tidalPlayerStarted())
    {
        snprintf(gText[0], sizeof gText[0], "%s - waiting for the project", kTidalPlayerFile);
    }
    else
    {
        const float cps = tidalPlayerCps();
        snprintf(gText[0], sizeof gText[0], "%s    cps %d.%03d (%d BPM)    audio %d %%    voices %d / %d",
                 kTidalPlayerFile, (int)cps, (int)(cps * 1000.0f + 0.5f) % 1000, (int)(cps * 240.0f + 0.5f),
                 tidalPlayerAudioLoad(), tidalPlayerSounding(), tidalPlayerCap());
        const char* m = tidalPlayerMessage();
        snprintf(gText[1], sizeof gText[1], "%s", m[0] ? m : (tidalPlayerAnyPlaying() ? "playing" : "silent"));
        for (int s = 0; s < 16; s++)
        {
            char warn[64];
            const char* err = tidalPlayerSlotError(s);
            const char* w = tidalPlayerSlotWarning(s, warn, sizeof warn);
            if (err)
                snprintf(gText[2 + s], sizeof gText[2 + s], "d%-2d  error: %s", s + 1, err);
            else if (tidalPlayerSlotPlaying(s))
                snprintf(gText[2 + s], sizeof gText[2 + s], "d%-2d  playing%s%s", s + 1, w ? "  - " : "", w ? w : "");
            else
                snprintf(gText[2 + s], sizeof gText[2 + s], "d%-2d  -", s + 1);
        }
    }
#else
    snprintf(gText[0], sizeof gText[0], "Tidal runs as a test build in this image.");
#endif
    bool changed = false;
    for (int i = 0; i < kLines; i++)
    {
        if (!force && strcmp(gText[i], gShown[i]) == 0)
            continue;
        memcpy(gShown[i], gText[i], sizeof gShown[i]);
        display.setControlText(line_[i], gShown[i]);
        display.setControlShow(line_[i]);
        display.refreshControl(line_[i]);
        changed = true;
    }
    if (changed)
        display.synchronizeRefresh();
}

void cTidalStatus::initDisplayControls()
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

    for (int i = 0; i < kLines; i++)
    {
        gShown[i][0] = 0;
        strControlProperties p;
        p.colors = display.getTheme().configItemEditorTextLabelColors;
        p.text = gShown[i];
        p.style = controlStyleShow | controlStyleFont1;
        p.x = 16;
        p.y = 34 + i * 20;
        p.w = 768;
        p.h = 20;
        p.value = 0;
        line_[i] = display.createControl<cLabel>(&p);
    }
}
