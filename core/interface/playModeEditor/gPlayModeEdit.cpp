#include "playModeEditor/playModeEditor.h"

#include <cstddef>
#include <cstring>

#include "cr_section_macros.h"
#include "display.h"
#include "mtStructs.h"
#include "patternEditor/patternEditor.h"
#include "userPlayModes.h"

// State in external RAM: internal RAM is full.
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::lane;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::row;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::column;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::firstLane;
__NOINIT(EXTERNAL_RAM) int16_t cPlayModeEditor::lastPlayhead;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::gridDirty;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::patternViewSaved;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::followPlayhead;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::selecting;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::selectionAnchor;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::selectionWholeLane;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::fxPopupOpen;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::fillOpen;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::fillColumn;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::fillParam;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::fillType;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::fillFxType;
__NOINIT(EXTERNAL_RAM) int16_t cPlayModeEditor::fillFrom;
__NOINIT(EXTERNAL_RAM) int16_t cPlayModeEditor::fillTo;
__NOINIT(EXTERNAL_RAM) char cPlayModeEditor::fillText[3][12];
__NOINIT(EXTERNAL_RAM) cPlayModeEditor::Clipboard cPlayModeEditor::clipboard;

// On Tracker+ / Mini the play modes' 6 KB reservation (mtStructs.h) holds the
// clipboard too, so it costs no RAM headroom.
static_assert(
  sizeof(userPlayModes::Block) + sizeof(cPlayModeEditor::Clipboard) <= USER_PLAY_MODES_MEMORY_SIZE,
  "the play modes block and the editor's clipboard must fit USER_PLAY_MODES_MEMORY_SIZE");
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
    followPlayhead = true;
    selecting = false;
    selectionAnchor = 0;
    selectionWholeLane = false;
    fxPopupOpen = false;
    fillOpen = false;
    clipboard.count = 0;
}

namespace
{
const char* const kLabelTexts[8] = {
    "Step", "FX", "Value", "Chance", "Fill", "Reset lane", "Revert", "Save",
};
const char* const kFillLabelTitles[8] = {
    "Type", "From", "To", "", "", "", "Cancel", "Apply",
};
constexpr uint32_t kLabelStyle =
  controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont3;
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
    followPlayhead = true;
    selecting = false;
    selectionAnchor = 0;
    selectionWholeLane = false;
    fxPopupOpen = false;
    fillOpen = false;
    clipboard.count = 0;
}

void cPlayModeEditor::initDisplayControls()
{
    strControlProperties prop;

    // Bottom labels, one per screen button.
    for (uint8_t i = 0; i < 8; i++)
    {
        prop.colors = display.getTheme().activeLabelsColors;
        prop.text = (char*)kLabelTexts[i];
        prop.style = kLabelStyle;
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

    refreshLabels();

    gridDirty = true;
    refreshGrid();
    display.setControlShow(grid);
    display.refreshControl(grid);

    display.synchronizeRefresh();
}

void cPlayModeEditor::refreshLabels()
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (fillOpen)
        {
            // Type / From / To show their value on a second line; the one the
            // jog wheel changes is underlined.
            const bool hasValue = i < 3;
            display.setControlText(label[i], kFillLabelTitles[i]);
            display.setControlText2(label[i], hasValue ? fillText[i] : "");
            display.setControlValue(label[i], hasValue ? 1 : 0);
            display.setControlStyle(label[i],
                                    kLabelStyle | (i == fillParam ? controlStyleUnderline : 0));
        }
        else
        {
            display.setControlText(label[i], kLabelTexts[i]);
            display.setControlText2(label[i], "");
            display.setControlValue(label[i], 0);
            display.setControlStyle(label[i], kLabelStyle);
        }
        display.refreshControl(label[i]);
    }
    display.refreshControl(bgLabel);
}
