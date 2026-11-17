#include "playModeEditor/playModeEditor.h"

#include "cr_section_macros.h"
#include "display.h"
#include "mtStructs.h"

// The grid's text, rebuilt for the visible lanes and rows on every refresh.
// External RAM: internal RAM is full.
__NOINIT(EXTERNAL_RAM) strTrackerPattern playModeEditorGrid;

namespace
{
const char* const kLabelTexts[8] = {
    "Step", "FX", "Value", "Chance", "", "Reset lane", "Revert", "Save",
};
} // namespace

void cPlayModeEditor::initDisplayControls()
{
    playModeEditorGrid.init();

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

    // The pattern editor's grid, in its 4-track view (all four columns).
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
    gridProp.data = &playModeEditorGrid;
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
