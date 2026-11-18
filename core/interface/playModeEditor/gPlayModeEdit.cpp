#include "playModeEditor/playModeEditor.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "cr_section_macros.h"
#include "display.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "patternEditor/patternEditor.h"
#include "userPlayModes.h"

// State in external RAM: internal RAM is full.
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::lane;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::row;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::column;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::firstLane;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::editMode;
__NOINIT(EXTERNAL_RAM) int16_t cPlayModeEditor::lastPlayhead;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::gridDirty;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::patternViewSaved;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::selecting;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::selectionAnchor;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::selectionEnd;
__NOINIT(EXTERNAL_RAM) int8_t cPlayModeEditor::heldPlace;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::fxPopupOpen;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::fxButtonHeld;
__NOINIT(EXTERNAL_RAM) NoInit<elapsedMillis> cPlayModeEditor::fxButtonTimer;
__NOINIT(EXTERNAL_RAM) bool cPlayModeEditor::fillOpen;
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::fillKind;
__NOINIT(EXTERNAL_RAM) int8_t cPlayModeEditor::fillPlace;
__NOINIT(EXTERNAL_RAM) cPlayModeEditor::FillData cPlayModeEditor::fillData[kFillKindCount];
__NOINIT(EXTERNAL_RAM) char cPlayModeEditor::fillText[3][12];
__NOINIT(EXTERNAL_RAM) char cPlayModeEditor::titleText[64];
__NOINIT(EXTERNAL_RAM) char cPlayModeEditor::resultText[20];
__NOINIT(EXTERNAL_RAM) char cPlayModeEditor::laneText[6];
__NOINIT(EXTERNAL_RAM) char cPlayModeEditor::editStepText[6];
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::grid;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::label[8];
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::bgLabel;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::frame;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::popupBg;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::popupTitle;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::listWhere;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::listFxType;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::listType;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::barDensity;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::barFrom;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::barTo;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::barCancel;
__NOINIT(EXTERNAL_RAM) hControl cPlayModeEditor::barFill;
__NOINIT(EXTERNAL_RAM) strFrameData cPlayModeEditor::frameData;
__NOINIT(EXTERNAL_RAM) NoInit<strList> cPlayModeEditor::whereList;
__NOINIT(EXTERNAL_RAM) NoInit<strList> cPlayModeEditor::typeList;
__NOINIT(EXTERNAL_RAM) NoInit<strList> cPlayModeEditor::fxTypeList;
__NOINIT(EXTERNAL_RAM) NoInit<strList> cPlayModeEditor::countList;
__NOINIT(EXTERNAL_RAM) const char* cPlayModeEditor::fxTypeNames[FX_COUNT];
__NOINIT(EXTERNAL_RAM) uint8_t cPlayModeEditor::fxTypeCount;
__NOINIT(EXTERNAL_RAM) cPlayModeEditor::Clipboard cPlayModeEditor::clipboard;
__NOINIT(EXTERNAL_RAM) cPlayModeEditor::Undo cPlayModeEditor::undoBuffer;

// On Tracker+ / Mini the play modes' reservation (mtStructs.h) holds the
// clipboard and the undo buffer too, so they cost no RAM headroom.
static_assert(sizeof(userPlayModes::Block) + sizeof(cPlayModeEditor::Clipboard) +
                  sizeof(cPlayModeEditor::Undo) <=
                USER_PLAY_MODES_MEMORY_SIZE,
              "the play modes block and the editor's buffers must fit USER_PLAY_MODES_MEMORY_SIZE");

namespace
{
// The part of the shared grid buffer that is Pattern editor state, not cell
// text: everything from `actualTrack` on.
constexpr size_t kPatternViewOffset = offsetof(strTrackerPattern, actualTrack);
constexpr size_t kPatternViewSize = sizeof(strTrackerPattern) - kPatternViewOffset;
__NOINIT(EXTERNAL_RAM) uint8_t savedPatternView[kPatternViewSize];

// Frames around the bottom labels and around the fill popup's columns, as in
// the Pattern editor.
const uint16_t kLabelFrames[8][4] = {
    { 0 + 1, 423, 800 / 8 - 1, 55 },
    { (800 / 8) * 1 + 1, 423, 800 / 8 - 1, 55 },
    { (800 / 8) * 2 + 1, 423, 800 / 8 - 1, 55 },
    { (800 / 8) * 3 + 1, 423, 800 / 8 - 1, 55 },
    { (800 / 8) * 4 + 1, 423, 800 / 8 - 1, 55 },
    { (800 / 8) * 5 + 1, 423, 800 / 8 - 1, 55 },
    { (800 / 8) * 6 + 1, 423, 800 / 8 - 1, 55 },
    { (800 / 8) * 7 + 1, 423, 800 / 8 - 1, 55 },
};
const uint16_t kPopupFrames[8][4] = {
    { 0 + 1, 58, 800 / 8 - 3, 364 },
    { (800 / 8) * 1 + 1, 58, 800 / 8 - 3, 364 },
    { (800 / 8) * 2 + 1, 58, 800 / 8 - 3, 364 },
    { (800 / 8) * 3 + 1, 58, 800 / 8 - 3, 364 },
    { (800 / 8) * 4 + 1, 58, 800 / 8 - 3, 364 },
    { (800 / 8) * 5 + 1, 58, 800 / 8 - 3, 364 },
    { (800 / 8) * 6 + 1, 58, 800 / 8 - 3, 364 },
    { (800 / 8) * 7 + 1, 58, 800 / 8 - 3, 364 },
};

// "Where?" choices: the Pattern editor's, without the note ones.
const char* const kWhereNames[] = { "Fx", "No Fx", "Random", "Each", "Euclidean" };
const char* const kCountNames[] = { "Filled rows", "All rows" };
const char* const kFillTypeNames[cPlayModeEditor::kTypeCount] = {
    "Constant",  "From-To", "Random",  "Up from",   "Up to",
    "Down from", "Down to", "Loop up", "Loop down",
};

const char* const kLabelTexts[8] = {
    "Lane", "Revert", "Step jump", "Fill", "Reset lane", "Undo", "Redo", "Save",
};
const char* const kFillTitles[cPlayModeEditor::kFillKindCount] = {
    " Fill Steps",
    " Fill Fx",
    " Fill Chance",
};
constexpr uint32_t kLabelStyle =
  controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont3;

void setFramePlaces(strFrameData& data, const uint16_t (*places)[4])
{
    for (uint8_t i = 0; i < 8; i++)
    {
        data.places[i] = &places[i][0];
    }
}

// The FX list's row for an FX type.
int16_t fxTypeListIndex(int16_t fxType)
{
    const int index = userPlayModes::allowedFxIndex(static_cast<uint8_t>(fxType));
    return static_cast<int16_t>(index < 0 ? 0 : index);
}

void setupList(strList& list, const char* const* names, uint16_t count, uint16_t textLen)
{
    list = strList();
    list.linesCount = count < 13 ? count : 13;
    list.start = 0;
    list.length = count;
    list.data = const_cast<char**>(names);
    list.lineWidthMax = 800 / 8 - 30;
    list.lineTextLenMax = textLen;
}

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

void cPlayModeEditor::initExternalRamState()
{
    lane = 0;
    row = 0;
    column = kColumnStep;
    firstLane = 0;
    editMode = false;
    lastPlayhead = -1;
    gridDirty = true;
    patternViewSaved = false;
    selecting = false;
    selectionAnchor = 0;
    selectionEnd = 0;
    heldPlace = kHeldNone;
    fxPopupOpen = false;
    fxButtonHeld = 0;
    fillOpen = false;
    fillKind = kFillStep;
    fillPlace = kPlaceType;
    clipboard.count = 0;
    undoBuffer.state = 0;
    undoBuffer.lastEdit = UINT32_MAX;

    // Fill settings, kept between fills as the Pattern editor keeps its own.
    for (uint8_t kind = 0; kind < kFillKindCount; kind++)
    {
        FillData& data = fillData[kind];
        data.where = 3; // Each
        data.type = 1;  // From-To
        data.density[0] = Sequencer::MAX_FILL_DENSITY / 2;
        data.density[1] = 1;
        data.density[2] = 3;
        data.fxType = 0;
        data.countAllRows = 0;
    }
    fillData[kFillStep].from = 1;
    fillData[kFillStep].to = 16;
    fillData[kFillFx].from = 0;
    fillData[kFillFx].to = 0;
    fillData[kFillChance].from = 100;
    fillData[kFillChance].to = 0;

    // The FX a play mode may use, by their short names.
    fxTypeCount = 0;
    for (int i = 0; i < userPlayModes::allowedFxCount() && fxTypeCount < FX_COUNT; i++)
    {
        const uint8_t name = interfaceGlobals.fxIdToName(userPlayModes::allowedFxType(i));
        fxTypeNames[fxTypeCount++] = interfaceGlobals.ptrAllFxNamesShort[name + 1];
    }
    fillData[kFillFx].fxType = userPlayModes::allowedFxType(0);

    setupList(whereList.value, kWhereNames, 5, 11);
    setupList(typeList.value, kFillTypeNames, kTypeCount, 11);
    setupList(fxTypeList.value, fxTypeNames, fxTypeCount, 15);
    setupList(countList.value, kCountNames, 2, 11);

    frameData.placesCount = 8;
    frameData.startPlace = 0;
    frameData.multiSelActiveNum = 0;
    setFramePlaces(frameData, kLabelFrames);
}

void cPlayModeEditor::initDisplayControls()
{
    strControlProperties prop;

    prop.x = 0;
    prop.y = 0;
    prop.w = 0;
    prop.h = 0;
    prop.style = 0;
    prop.value = 0;
    prop.data = &frameData;
    frame = display.createControl<cFrame>(&prop);

    // Bottom labels, one per screen button.
    for (uint8_t i = 0; i < 8; i++)
    {
        strControlProperties labelProp;
        labelProp.colors = display.getTheme().activeLabelsColors;
        labelProp.text = (char*)kLabelTexts[i];
        labelProp.style = kLabelStyle;
        labelProp.x = (800 / 8) * i + (800 / 16);
        labelProp.y = 424;
        labelProp.w = 800 / 8 - 6;
        labelProp.h = 55;
        labelProp.value = 0;
        labelProp.data = nullptr;
        label[i] = display.createControl<cLabel>(&labelProp);
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

    // The fill popup: the Pattern editor's controls and layout.
    strControlProperties popup;
    popup.x = 1;
    popup.y = 58;
    popup.w = 800 / 8 - 3;
    popup.h = 140 + 8 * 28;
    popup.colors = nullptr;
    popup.value = 0;
    popup.style = controlStyleBackground |
                  (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);
    popup.data = &whereList.value;
    listWhere = display.createControl<cList>(&popup);
    popup.x = (800 / 8) * 2 + 1;
    popup.data = &fxTypeList.value;
    listFxType = display.createControl<cList>(&popup);
    popup.x = (800 / 8) * 3 + 1;
    popup.data = &typeList.value;
    listType = display.createControl<cList>(&popup);

    popup.data = nullptr;
    popup.style = controlStyleValue_0_100 | controlStyleBackground |
                  (display.getTheme().barShadow > 0 ? controlStyleGradient : 0) |
                  (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);
    popup.x = (800 / 8) * 1 + 1;
    barDensity = display.createControl<cBar>(&popup);
    popup.x = (800 / 8) * 4 + 1;
    barFrom = display.createControl<cBar>(&popup);
    popup.x = (800 / 8) * 5 + 1;
    barTo = display.createControl<cBar>(&popup);
    popup.value = -1;
    popup.x = (800 / 8) * 6 + 1;
    barCancel = display.createControl<cBar>(&popup);
    popup.x = (800 / 8) * 7 + 1;
    barFill = display.createControl<cBar>(&popup);

    popup.value = 0;
    popup.style = controlStyleCenterY | controlStyleFont4 | controlStyleBackground;
    popup.colors = display.getTheme().topBarLabelColors;
    popup.x = 0;
    popup.y = 28;
    popup.w = 799;
    popup.h = 27;
    popupTitle = display.createControl<cLabel>(&popup);

    popup.style = controlStyleBackground | controlStyleCenterX | controlStyleNoTransparency;
    popup.colors = display.getTheme().patternFillBgLabelColors;
    popup.x = 400;
    popup.y = 28 + 4;
    popup.w = 800;
    popup.h = 193 + 7 * 28;
    popupBg = display.createControl<cLabel>(&popup);

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

// The bottom labels, as the Pattern editor's first page: edit actions are
// greyed out outside edit mode.
void cPlayModeEditor::refreshLabels()
{
    snprintf(laneText, sizeof(laneText), "C%u", lane + 1);
    snprintf(editStepText, sizeof(editStepText), "%u", mtProject.values.patternEditStep);

    const auto& theme = display.getTheme();
    for (uint8_t i = 0; i < 8; i++)
    {
        const bool editAction = i == 3 || i == 4 || i == 5 || i == 6;
        const bool available =
          !editAction || (editMode && (i < 5 || (i == 5 && undoBuffer.state == 1) ||
                                       (i == 6 && undoBuffer.state == 2)));
        const char* value = i == 0 ? laneText : (i == 2 ? editStepText : nullptr);

        display.setControlText(label[i], kLabelTexts[i]);
        display.setControlText2(label[i], value ? value : "");
        display.setControlValue(label[i], value ? 1 : 0);
        display.setControlStyle(label[i], kLabelStyle);
        display.setControlStyle2(label[i],
                                 controlStyleCenterX | controlStyleCenterY | controlStyleFont2);
        display.setControlColors(label[i],
                                 available ? theme.activeLabelsColors : theme.inactiveLabelsColors);
        display.refreshControl(label[i]);
    }
    display.refreshControl(bgLabel);

    if (heldPlace != kHeldNone)
    {
        setFramePlaces(frameData, kLabelFrames);
        display.setControlValue(frame, heldPlace);
        display.setControlShow(frame);
    }
    else
    {
        display.setControlHide(frame);
    }
    display.refreshControl(frame);
}

//==============================================================================================
// Fill popup

void cPlayModeEditor::showFillPopup()
{
    setFramePlaces(frameData, kPopupFrames);

    display.setControlHide(grid);
    display.refreshControl(grid);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlStyle(label[i], kLabelStyle);
        display.setControlStyle2(label[i],
                                 controlStyleCenterX | controlStyleCenterY | controlStyleFont2);
        display.setControlColors(label[i], display.getTheme().activeLabelsColors);
    }
    display.setControlText(label[0], "Where?");
    display.setControlText2(label[0], "");
    display.setControlValue(label[0], 0);
    display.setControlText(label[3], "Fill Type");
    display.setControlText2(label[3], "");
    display.setControlValue(label[3], 0);
    display.setControlText(label[6], "Cancel");
    display.setControlText2(label[6], "");
    display.setControlValue(label[6], 0);
    display.setControlText(label[7], "Fill");
    display.setControlText2(label[7], "");
    display.setControlValue(label[7], 0);

    display.setControlText(popupTitle, kFillTitles[fillKind]);
    display.setControlShow(popupBg);
    display.refreshControl(popupBg);
    display.setControlShow(popupTitle);
    display.refreshControl(popupTitle);

    display.setControlShow(listWhere);
    display.setControlShow(listType);
    display.setControlShow(barDensity);
    display.setControlShow(barFrom);
    display.setControlShow(barTo);
    display.setControlShow(barCancel);
    display.setControlShow(barFill);
    display.refreshControl(barCancel);
    display.refreshControl(barFill);
    // Put each list at its value, as the Pattern editor does when it shows the
    // popup: setControlData() also places the highlight bar. After this, only
    // setControlValue() moves them (the list animates to it); setting `start`
    // directly would scroll the text without moving the bar.
    const FillData& data = fillData[fillKind];
    whereList.value.start = data.where;
    display.setControlData(listWhere, &whereList.value);
    // Fill Steps has the sequences too.
    typeList.value.length = fillTypeCount(fillKind);
    typeList.value.linesCount = typeList.value.length;
    typeList.value.start = data.type;
    display.setControlData(listType, &typeList.value);
    // Column 3 is Fx Type for Fill Fx and Count for Fill Steps (one list
    // control, shown in refreshFillPopup()).
    if (fillKind == kFillStep)
    {
        countList.value.start = data.countAllRows;
        display.setControlData(listFxType, &countList.value);
    }
    else
    {
        fxTypeList.value.start = fxTypeListIndex(data.fxType);
        display.setControlData(listFxType, &fxTypeList.value);
    }

    refreshFillPopup();
}

// Every value in the popup, from fillData.
void cPlayModeEditor::refreshFillPopup()
{
    FillData& data = fillData[fillKind];

    // Where? and its density
    display.setControlValue(listWhere, data.where);
    display.refreshControl(listWhere);

    const char* densityTitle = nullptr;
    const int whereChoice = data.where - 2; // Random / Each / Euclidean
    if (whereChoice >= 0)
    {
        const int16_t density = data.density[whereChoice];
        densityTitle = whereChoice == 0 ? "Density" : (whereChoice == 1 ? "Step" : "Events");
        if (whereChoice == 0)
        {
            snprintf(fillText[0],
                     sizeof(fillText[0]),
                     "%d %%",
                     density * 100 / Sequencer::MAX_FILL_DENSITY);
        }
        else
        {
            snprintf(fillText[0], sizeof(fillText[0]), "%d", density);
        }
        display.setControlValue(barDensity, density * 100 / Sequencer::MAX_FILL_DENSITY);
    }
    else
    {
        display.setControlValue(barDensity, -1);
    }
    display.setControlText(label[1], densityTitle ? densityTitle : "");
    display.setControlText2(label[1], densityTitle ? fillText[0] : "");
    display.setControlValue(label[1], 1);
    display.refreshControl(barDensity);

    // Column 3: FX type (Fill Fx) or Count (Fill Steps sequences)
    const bool hasColumn3 = isFillPlaceValid(kPlaceFxType);
    if (hasColumn3)
    {
        display.setControlShow(listFxType);
        display.setControlValue(
          listFxType, fillKind == kFillFx ? fxTypeListIndex(data.fxType) : data.countAllRows);
    }
    else
    {
        display.setControlHide(listFxType);
    }
    display.refreshControl(listFxType);
    display.setControlText(label[2], hasColumn3 ? (fillKind == kFillFx ? "Fx Type" : "Count") : "");
    display.setControlText2(label[2], "");
    display.setControlValue(label[2], 0);

    // Fill type
    display.setControlValue(listType, data.type);
    display.refreshControl(listType);

    // From / To
    int low = 1;
    int high = kRowCount;
    if (fillKind == kFillFx)
    {
        low = userPlayModes::fxMin(static_cast<uint8_t>(data.fxType));
        high = userPlayModes::fxMax(static_cast<uint8_t>(data.fxType));
    }
    else if (fillKind == kFillChance)
    {
        low = 0;
        high = 100;
    }
    data.from = static_cast<int16_t>(data.from < low ? low : (data.from > high ? high : data.from));
    data.to = static_cast<int16_t>(data.to < low ? low : (data.to > high ? high : data.to));

    for (uint8_t i = 0; i < 2; i++)
    {
        const int16_t value = i == 0 ? data.from : data.to;
        char* text = fillText[1 + i];
        if (fillKind == kFillFx)
        {
            sequencer.makeFxValLabelWithAlgin(text,
                                              sizeof(fillText[0]),
                                              static_cast<uint8_t>(data.fxType),
                                              static_cast<uint8_t>(value),
                                              Sequencer::fxValLabelAlign_center);
        }
        else
        {
            snprintf(text, sizeof(fillText[0]), fillKind == kFillChance ? "%d%%" : "%d", value);
        }
    }

    const bool constant = data.type == kTypeConstant;
    const bool hasFrom = fillTypeUsesFrom(data.type);
    const bool hasTo = fillTypeUsesTo(data.type);
    const char* fromTitle = constant ? (fillKind == kFillStep ? "Step"
                                        : fillKind == kFillFx ? "Fx Value"
                                                              : "Chance")
                                     : "From";
    const char* toTitle = data.type >= kTypeLoopUp ? "Loop" : "To";
    const int range = high - low;
    display.setControlText(label[4], hasFrom ? fromTitle : "");
    display.setControlText2(label[4], hasFrom ? fillText[1] : "");
    display.setControlValue(label[4], 1);
    display.setControlValue(barFrom,
                            hasFrom ? (range > 0 ? (data.from - low) * 100 / range : 0) : -1);
    display.refreshControl(barFrom);

    display.setControlText(label[5], hasTo ? toTitle : "");
    display.setControlText2(label[5], hasTo ? fillText[2] : "");
    display.setControlValue(label[5], 1);
    display.setControlValue(barTo, hasTo ? (range > 0 ? (data.to - low) * 100 / range : 0) : -1);
    display.refreshControl(barTo);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.refreshControl(label[i]);
    }
    display.refreshControl(bgLabel);

    display.setControlValue(frame, fillPlace);
    display.setControlShow(frame);
    display.refreshControl(frame);

    display.synchronizeRefresh();
}

void cPlayModeEditor::closeFill()
{
    fillOpen = false;

    display.setControlHide(listWhere);
    display.setControlHide(listFxType);
    display.setControlHide(listType);
    display.setControlHide(barDensity);
    display.setControlHide(barFrom);
    display.setControlHide(barTo);
    display.setControlHide(barCancel);
    display.setControlHide(barFill);
    display.setControlHide(popupTitle);
    display.setControlHide(popupBg);
    display.setControlHide(frame);

    setDefaultScreenFunct();
    showDefaultScreen();
}
