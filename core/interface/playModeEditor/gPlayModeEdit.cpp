#include "playModeEditor/playModeEditor.h"

#include <cstddef>
#include <cstring>

#include "cr_section_macros.h"
#include "display.h"
#include "mtStructs.h"
#include "patternEditor/patternEditor.h"

// State in external RAM: internal RAM is full.
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::lane;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::row;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::column;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::firstLane;
__NOINIT(EXTERNAL_RAM) int16_t cPlayModeEditor::lastPlayhead;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::gridDirty;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::patternViewSaved;
__NOINIT(EXTERNAL_RAM) char cPlayModeEditor::titleText[64];
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::grid;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::label[8];
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::bgLabel;

namespace
{
// The part of the shared grid buffer that is Pattern editor state, not cell
// text: everything from `actualTrack` on.
constexpr size_t kPatternViewOffset = offsetof(strTrackerPattern, actualTrack);
constexpr size_t kPatternViewSize = sizeof(strTrackerPattern) - kPatternViewOffset;
__NOINIT(EXTERNAL_RAM) uint8_t savedPatternView[kPatternViewSize];
} // namespace

void cPlayModeEditor::savePatternView()
{
    if (patternViewSaved)
    {
        return;
    }
    memcpy(savedPatternView,
           reinterpret_cast<uint8_t*>(&cPatternEditor::trackerPattern) + kPatternViewOffset,
           kPatternViewSize);
    patternViewSaved = true;
}

void cPlayModeEditor::restorePatternView()
{
    if (!patternViewSaved)
    {
        return;
    }
    memcpy(reinterpret_cast<uint8_t*>(&cPatternEditor::trackerPattern) + kPatternViewOffset,
           savedPatternView,
           kPatternViewSize);
    patternViewSaved = false;
}

namespace
{
const char* const kLabelTexts[8] = {
    "Step", "FX", "Value", "Chance", "", "Reset lane", "Revert", "Save",
};
} // namespace

void cPlayModeEditor::initExternalRamState()
{
    lane = 0;
    row = 0;
    column = kColumnStep;
    firstLane = 0;
    lastPlayhead = -1;
    gridDirty = true;
    patternViewSaved = false;
}

void cPlayModeEditor::initDisplayControls()
{

    strControlProperties prop;

    // Bottom labels, one per screen button.
    for (uint8_t i = 0; i < 8; i++)
    {
        prop.colors = display.getTheme().activeLabelsColors;
        prop.text = (char*)kLabelTexts[i];
        prop.style =
          (controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
        prop.x = (800 / 8) * i + (800 / 16);
        prop.y = 424;
        prop.w = 800 / 8 - 6;
        prop.h = 55;
        prop.value = 0;
        prop.data = nullptr;
        label[i] = display.createControl<cLabel>(&prop);
    }

    strControlProperties bg;
    bg.text = nullptr;
    bg.colors = nullptr;
    bg.value = 255;
    bg.style = controlStyleNoTransparency | controlStyleShow;
    bg.x = 0;
    bg.w = 800;
    bg.y = 425;
    bg.h = 54;
    bgLabel = display.createControl<cBgLabel>(&bg);

    // The pattern editor's grid control, in its 4-track view (all four
    // columns), on the Pattern editor's own text buffer.
    strControlProperties gridProp;
    gridProp.text = nullptr;
    gridProp.style =
      controlStyleFont4 | controlStyleStartNumbersFrom1 |
      (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0) |
      (display.getTheme().patternEditorPlayheadShadow > 0 ? controlStyleGradient : 0);
    gridProp.x = 0;
    gridProp.y = 28;
    gridProp.w = 800;
    gridProp.h = 395;
    gridProp.colors = display.getTheme().patternTrackerColors;
    gridProp.data = &cPatternEditor::trackerPattern;
    gridProp.value = 0;
    grid = display.createControl<cTracker>(&gridProp);
}

void cPlayModeEditor::showDefaultScreen()
{
    titleBar->show();
    refreshTitle();

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlText(label[i], kLabelTexts[i]);
        display.refreshControl(label[i]);
    }
    display.refreshControl(bgLabel);

    gridDirty = true;
    refreshGrid();
    display.setControlShow(grid);
    display.refreshControl(grid);

    display.synchronizeRefresh();
}
