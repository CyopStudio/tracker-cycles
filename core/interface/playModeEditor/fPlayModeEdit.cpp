#include "playModeEditor/playModeEditor.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/interfaceMenuPopup.h"
#include "core/interfacePopups.h"
#include "display.h"
#include "fileManager.h"
#include "mtConfig.h"
#include "mtPadBoard.h"
#include "mtPadsBacklight.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "patternEditor/patternEditor.h"
#include "performanceMode/performanceMode.h"
#include "userPlayModes.h"
#include "util_math.h"

cPlayModeEditor playModeEditor;
static cPlayModeEditor* PME = &playModeEditor;

static void functPlayAction();
static void functRecAction(uint8_t state);
static void functUp();
static void functDown();
static void functLeft();
static void functRight();
static void functDelete(uint8_t state);
static void functInsert(uint8_t state);
static void functColumnButton(uint8_t button, uint8_t state);
static void functCopy(uint8_t state);
static void functEncoder(int16_t value);
static void functSwitchModule(uint8_t button);
static void functActionButton(uint8_t button, uint8_t state);
static void functPads(uint8_t pad, uint8_t state, int16_t velo);
static void functMenu(uint8_t state);

namespace
{

const char* const kLaneNames[cPlayModeEditor::kLaneCount] = {
    "C1",  "C2",  "C3",  "C4",  "C5",  "C6",  "C7",  "C8",  "C9",  "C10",
    "C11", "C12", "C13", "C14", "C15", "C16", "C17", "C18", "C19", "C20",
};
// Lanes changed from their built-in mode.
const char* const kEditedLaneNames[cPlayModeEditor::kLaneCount] = {
    "C1 *",  "C2 *",  "C3 *",  "C4 *",  "C5 *",  "C6 *",  "C7 *",  "C8 *",  "C9 *",  "C10 *",
    "C11 *", "C12 *", "C13 *", "C14 *", "C15 *", "C16 *", "C17 *", "C18 *", "C19 *", "C20 *",
};

// Fill "Where?" choices (kWhereNames in gPlayModeEdit.cpp).
enum : uint8_t
{
    kWhereFx,
    kWhereNoFx,
    kWhereRandom,
    kWhereEach,
    kWhereEuclidean,
    kWhereCount
};

// The Pattern editor shows its step popups after the button is held this long.
constexpr uint32_t kFxPopupDelay = 180;
constexpr int kRollStride = Sequencer::strFxConsts::ROLL_PERIOD_MAX + 1;
constexpr uint8_t kGridRows = 15;
constexpr uint8_t kCursorGridRow = 7;

int clampInt(int value, int low, int high)
{
    return value < low ? low : (value > high ? high : value);
}

// A roll value with period 0 means "no roll": move off it, towards `direction`.
int skipNoRoll(uint8_t fxType, int value, int direction, int low, int high)
{
    if (fxType == Sequencer::strFxConsts::FX_TYPE_ROLL && value % kRollStride == 0)
    {
        value += (value + direction >= low && value + direction <= high) ? direction : -direction;
    }
    return value;
}

// "R - Roll" etc., the pattern editor's FX name.
const char* fxName(uint8_t fxType)
{
    const uint8_t index = interfaceGlobals.fxIdToName(fxType);
    return index < FX_COUNT ? &interfaceGlobals.fxNames[index][0] : "?";
}

void fillCell(strTrackerPattern::strTracks::strRow& cell, uint8_t lane, uint8_t index)
{
    const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, index);

    snprintf(cell.note, sizeof(cell.note), "%3u", info.stepNumber);

    // The Instr column is not used. instr[2] must stay 0 (it marks MIDI).
    memset(cell.instr, 0, sizeof(cell.instr));

    if (info.fxType == 0)
    {
        strcpy(cell.fx[0], "----");
        strcpy(cell.fx[1], "---");
        return;
    }

    // The FX letter then its value, as the Pattern editor's FX columns.
    cell.fx[0][0] = fxName(info.fxType)[0];
    sequencer.makeFxValLabelWithAlgin(&cell.fx[0][1],
                                      sizeof(cell.fx[0]) - 1,
                                      info.fxType,
                                      static_cast<uint8_t>(info.value),
                                      Sequencer::fxValLabelAlign_right);
    snprintf(cell.fx[1], sizeof(cell.fx[1]), "%3u%%", info.chance);
}

// Row (0-127) that `lane`'s mode is playing, or -1 when stopped: the first
// track set to that mode, or else track 1. Every track counts its position
// whatever its play mode, so a lane no track uses shows where it would be.
int16_t playheadFor(uint8_t lane)
{
    if (!sequencer.isPlay() || !sequencer.isPerformanceMode())
    {
        return -1;
    }
    const auto& tracks = sequencer.ptrPlayer->track;
    const int trackCount = sizeof(tracks) / sizeof(tracks[0]);
    int found = 0;
    for (int t = 0; t < trackCount; t++)
    {
        if (tracks[t].performancePlayMode == Sequencer::PLAYMODE_CUSTOM_1 + lane)
        {
            found = t;
            break;
        }
    }
    return tracks[found].actual_pos_absolute % cPlayModeEditor::kRowCount;
}

// Which fill a column opens.
uint8_t fillKindFor(uint8_t column)
{
    return column == cPlayModeEditor::kColumnStep     ? cPlayModeEditor::kFillStep
           : column == cPlayModeEditor::kColumnChance ? cPlayModeEditor::kFillChance
                                                      : cPlayModeEditor::kFillFx;
}

void fillRange(uint8_t kind, uint8_t fxType, int& low, int& high)
{
    switch (kind)
    {
        case cPlayModeEditor::kFillStep:
            low = 1;
            high = cPlayModeEditor::kRowCount;
            break;
        case cPlayModeEditor::kFillFx:
            low = userPlayModes::fxMin(fxType);
            high = userPlayModes::fxMax(fxType);
            break;
        default:
            low = 0;
            high = 100;
            break;
    }
}

// The Pattern editor's Sequencer::isEuclideanStep(), for a run of `length` rows.
bool isEuclideanStep(int offset, int density, int length)
{
    const float stepSize = static_cast<float>(length) / density;
    const float nearest = roundf(offset / stepSize) * stepSize;
    const float diff = nearest - static_cast<float>(offset);
    return diff <= 0.5f && diff > -0.5f;
}

// Whether a fill writes the row at `offset` (as Sequencer::isStepToFillFx()).
bool isRowToFill(const cPlayModeEditor::FillData& data, int offset, int length, bool hasFx)
{
    switch (data.where)
    {
        case kWhereFx: return hasFx;
        case kWhereNoFx: return !hasFx;
        case kWhereRandom:
            return utilMathRandom::random(0, Sequencer::MAX_FILL_DENSITY) < data.density[0];
        case kWhereEach: return offset % data.density[1] == 0;
        case kWhereEuclidean: return isEuclideanStep(offset, data.density[2], length);
        default: return false;
    }
}

void readRow(cPlayModeEditor::Rows& rows, uint8_t index, uint8_t lane, uint8_t row)
{
    const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, row);
    rows.stepNumber[index] = info.stepNumber;
    rows.fxType[index] = info.fxType;
    rows.value[index] = info.value;
    rows.chance[index] = info.chance;
}

bool writeRow(const cPlayModeEditor::Rows& rows, uint8_t index, uint8_t lane, uint8_t row)
{
    return userPlayModes::setStepNumber(lane, row, rows.stepNumber[index]) &&
           userPlayModes::setStepFx(
             lane, row, rows.fxType[index], rows.value[index], rows.chance[index]);
}

// A row as the built-in order has it: step row+1, no FX.
bool clearRow(uint8_t lane, uint8_t row)
{
    return userPlayModes::setStepNumber(lane, row, static_cast<uint8_t>(row + 1)) &&
           userPlayModes::setStepFx(lane, row, 0, 0, 100);
}

bool copyRow(uint8_t lane, uint8_t from, uint8_t to)
{
    const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, from);
    return userPlayModes::setStepNumber(lane, to, info.stepNumber) &&
           userPlayModes::setStepFx(lane, to, info.fxType, info.value, info.chance);
}

// One step of the jog wheel on a row: false if the FX list is full.
bool changeRowValue(uint8_t lane, uint8_t row, uint8_t column, int16_t delta)
{
    const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, row);

    switch (column)
    {
        case cPlayModeEditor::kColumnStep:
        {
            const int step = clampInt(info.stepNumber + delta, 1, cPlayModeEditor::kRowCount);
            return step == info.stepNumber ||
                   userPlayModes::setStepNumber(lane, row, static_cast<uint8_t>(step));
        }
        case cPlayModeEditor::kColumnFx:
        {
            // The value. The FX itself is picked with [Fx1] held (list or pad).
            if (info.fxType == 0)
            {
                return true;
            }
            const int low = userPlayModes::fxMin(info.fxType);
            const int high = userPlayModes::fxMax(info.fxType);
            int value = info.value;
            const int direction = delta > 0 ? 1 : -1;
            for (int16_t n = delta > 0 ? delta : -delta; n > 0; n--)
            {
                int next = value + direction;
                // A roll value with period 0 means "no roll": skip it.
                if (info.fxType == Sequencer::strFxConsts::FX_TYPE_ROLL && next % kRollStride == 0)
                {
                    next += direction;
                }
                if (next < low || next > high)
                {
                    break;
                }
                value = next;
            }
            return value == info.value ||
                   userPlayModes::setStepFx(
                     lane, row, info.fxType, static_cast<int16_t>(value), info.chance);
        }
        case cPlayModeEditor::kColumnChance:
        {
            if (info.fxType == 0)
            {
                return true;
            }
            const int chance = clampInt(info.chance + delta, 0, 100);
            return userPlayModes::setStepFx(
              lane, row, info.fxType, info.value, static_cast<uint8_t>(chance));
        }
        default: return true;
    }
}

} // namespace

//==============================================================================================
// Module

void cPlayModeEditor::update()
{
    // The Mini's module menu covers the grid while it is held.
    if (!moduleRefresh || mtMenuPopup.isPopupVisible())
    {
        return;
    }

    // [Instr] / [Fx] held: the FX list, after the Pattern editor's delay.
    if (fxButtonHeld && !fxPopupOpen && !fillOpen && fxButtonTimer.value > kFxPopupDelay)
    {
        openFxPopup();
    }

    if (fillOpen)
    {
        return;
    }

    const int16_t playhead = playheadFor(lane);

    // Outside edit mode the cursor follows the playhead, as in the Pattern editor.
    if (!editMode && playhead >= 0 && playhead != row && !fxPopupOpen)
    {
        row = static_cast<uint8_t>(playhead);
        gridDirty = true;
        refreshTitle();
    }

    if (playhead != lastPlayhead || gridDirty)
    {
        lastPlayhead = playhead;
        refreshGrid();
    }
}

void cPlayModeEditor::start(uint32_t options)
{
    (void)options;
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

    lastPlayhead = -1;
    selecting = false;
    heldPlace = kHeldNone;
    fillOpen = false;
    fxPopupOpen = false;
    fxButtonHeld = 0;

    savePatternView();
    showDefaultScreen();
    setDefaultScreenFunct();
}

void cPlayModeEditor::stop()
{
    moduleRefresh = 0;
    if (fxPopupOpen)
    {
        mtPopups.hideStepPopups();
        fxPopupOpen = false;
    }
    fxButtonHeld = 0;
    fillOpen = false;
    heldPlace = kHeldNone;
    restorePatternView();
    mtPadBoard.releaseAllInstruments();
    padsBacklight.clearGlobalPads(1, 1, 1);
}

void cPlayModeEditor::setDefaultScreenFunct()
{
    ip->clearButtonsRange(interfaceButton0, interfaceButton7);
    ip->clearAllPots();

    ip->setButtonObj(interfaceButtonPlay, buttonPress, functPlayAction);
    ip->setButtonObj(interfaceButtonRec, functRecAction);

    ip->setButtonObj(interfaceButtonUp, buttonPress, functUp);
    ip->setButtonObj(interfaceButtonDown, buttonPress, functDown);
    ip->setButtonObj(interfaceButtonLeft, buttonPress, functLeft);
    ip->setButtonObj(interfaceButtonRight, buttonPress, functRight);
    ip->setButtonObj(interfaceButtonDelete, functDelete);
    ip->setButtonObj(interfaceButtonInsert, functInsert);
    ip->setButtonObj(interfaceButtonCopy, functCopy);

    ip->setButtonObj(interfaceButtonNote, functColumnButton);
    ip->setButtonObj(interfaceButtonInstr, functColumnButton);
    ip->setButtonObj(interfaceButtonVol, functColumnButton);
    ip->setButtonObj(interfaceButtonFx, functColumnButton);

    for (uint8_t button = interfaceButton0; button <= interfaceButton7; button++)
    {
        ip->setButtonObj(button, functActionButton);
    }

    ip->setPotObj(interfacePot0, functEncoder);
    ip->setPadsGlobal(functPads);
}

//==============================================================================================
// Grid

void cPlayModeEditor::refreshGrid()
{
    gridDirty = false;

    if (lane < firstLane)
    {
        firstLane = lane;
    }
    else if (lane >= firstLane + kVisibleLanes)
    {
        firstLane = lane - kVisibleLanes + 1;
    }

    strTrackerPattern& g = cPatternEditor::trackerPattern;
    for (uint8_t visible = 0; visible < kVisibleLanes; visible++)
    {
        const uint8_t shownLane = firstLane + visible;
        for (uint8_t gridRow = 0; gridRow < kGridRows; gridRow++)
        {
            // Modes loop, so rows wrap around like a revolving pattern.
            const uint8_t index = (row + kRowCount - kCursorGridRow + gridRow) % kRowCount;
            fillCell(g.track[visible].row[gridRow], shownLane, index);
        }
        g.tracksNames[visible] = userPlayModes::isUserMode(shownLane) ? kEditedLaneNames[shownLane]
                                                                      : kLaneNames[shownLane];
        g.inactive[visible] = 0;
    }

    const int8_t cursorTrack = static_cast<int8_t>(lane - firstLane);

    g.firstVisibleTrack = 0;
    g.actualTrack = cursorTrack;
    g.actualStep = row;
    g.middleVisibleStep = row;
    g.patternLength = kRowCount;
    g.stepDevider = 4;
    g.selectedParam = column;
    // Red in edit mode, green otherwise, as in the Pattern editor.
    g.selectColor = editMode ? 0 : 1;
    g.selectColumn = 0;
    g.showTrackNames = true;

    // The FX list popup covers the right of the grid.
    g.popupMode = fxPopupOpen ? (2 | (mtConfig.interface.fxPopupDescription ? 4 : 0)) : 0;

    if (heldPlace != kHeldNone)
    {
        g.selectState = 0; // the cursor is hidden while a screen button is held
    }
    else if (selecting)
    {
        uint8_t first;
        uint8_t last;
        actionRows(first, last);
        g.selectState = 2;
        g.selectStartTrack = cursorTrack;
        g.selectEndTrack = cursorTrack;
        g.selectStartStep = first;
        g.selectEndStep = last;
    }
    else
    {
        g.selectState = 1;
    }

    // The grid draws the playhead within 7 rows of the cursor, counting
    // straight: take the nearest copy of the position across the 128 -> 1 wrap.
    int16_t playhead = lastPlayhead;
    if (playhead - row > kRowCount / 2)
    {
        playhead -= kRowCount;
    }
    else if (row - playhead > kRowCount / 2)
    {
        playhead += kRowCount;
    }
    g.showPlayhead = lastPlayhead >= 0;
    g.playheadPosition = lastPlayhead >= 0 ? playhead : 0;
    g.playheadStartTrack = 0;
    g.playheadEndTrack = kVisibleLanes - 1;

    display.refreshControl(grid);
}

void cPlayModeEditor::refreshTitle()
{
    const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, row);
    const char* edited = userPlayModes::isUserMode(lane) ? " *" : "";

    if (info.fxType == 0)
    {
        snprintf(titleText,
                 sizeof(titleText),
                 "Play Modes  C%u%s  %u > step %u",
                 lane + 1,
                 edited,
                 row + 1,
                 info.stepNumber);
    }
    else
    {
        char value[6];
        sequencer.makeFxValLabelWithAlgin(value,
                                          sizeof(value),
                                          info.fxType,
                                          static_cast<uint8_t>(info.value),
                                          Sequencer::fxValLabelAlign_center);
        snprintf(titleText,
                 sizeof(titleText),
                 "C%u%s  %u > step %u  %s %s %u%%",
                 lane + 1,
                 edited,
                 row + 1,
                 info.stepNumber,
                 fxName(info.fxType),
                 value,
                 info.chance);
    }
    titleBar->setTitle(titleText);
}

void cPlayModeEditor::moveRow(int delta)
{
    row = static_cast<uint8_t>((row + kRowCount + delta % kRowCount) % kRowCount);
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::setLane(uint8_t newLane)
{
    if (newLane >= kLaneCount)
    {
        return;
    }
    lane = newLane;
    selecting = false;
    gridDirty = true;
    refreshTitle();
    refreshLabels();
}

void cPlayModeEditor::moveLane(int delta)
{
    setLane(static_cast<uint8_t>(clampInt(lane + delta, 0, kLaneCount - 1)));
}

void cPlayModeEditor::selectColumn(uint8_t newColumn)
{
    column = newColumn < kColumnCount ? newColumn : uint8_t(kColumnStep);
    gridDirty = true;
}

void cPlayModeEditor::toggleEditMode()
{
    // While playing, the selection goes (the cursor follows the playhead again).
    if (sequencer.isPlay())
    {
        selecting = false;
    }
    editMode = !editMode;
    gridDirty = true;
    refreshLabels();
    display.synchronizeRefresh();
}

void cPlayModeEditor::moveCursorByStep(uint8_t extra)
{
    selecting = false;
    const int jump = mtProject.values.patternEditStep + extra;
    if (jump > 0)
    {
        moveRow(jump);
    }
    gridDirty = true;
}

void cPlayModeEditor::changeEditStep(int delta)
{
    mtProject.values.patternEditStep = static_cast<uint8_t>(
      clampInt(mtProject.values.patternEditStep + delta, 0, PATTERN_EDIT_STEP_MAX));
    newFileManager.setProjectStructChanged();
    refreshLabels();
    display.synchronizeRefresh();
}

// Pressing Lane or Step jump frames its label; the jog wheel, the arrows or
// the pads then change it until the button is released.
void cPlayModeEditor::setHeldPlace(int8_t place)
{
    heldPlace = place;
    gridDirty = true;
    refreshLabels();
    refreshGrid();
    display.synchronizeRefresh();
}

//==============================================================================================
// Selection

void cPlayModeEditor::extendSelection(int delta)
{
    if (!selecting)
    {
        selecting = true;
        selectionAnchor = row;
    }
    // No wrapping while selecting: a selection is one run of rows.
    row = static_cast<uint8_t>(clampInt(row + delta, 0, kRowCount - 1));
    selectionEnd = row;
    gridDirty = true;
    refreshTitle();
}

// Shift+Up on the first row selects the whole lane, as the Pattern editor
// selects the whole track.
void cPlayModeEditor::selectWholeLane()
{
    selecting = true;
    selectionAnchor = 0;
    selectionEnd = kRowCount - 1;
    gridDirty = true;
}

void cPlayModeEditor::clearSelection()
{
    if (selecting)
    {
        selecting = false;
        gridDirty = true;
    }
}

bool cPlayModeEditor::isMultiSelection() const
{
    return selecting && selectionAnchor != selectionEnd;
}

void cPlayModeEditor::actionRows(uint8_t& first, uint8_t& last) const
{
    if (selecting)
    {
        first = selectionAnchor < selectionEnd ? selectionAnchor : selectionEnd;
        last = selectionAnchor < selectionEnd ? selectionEnd : selectionAnchor;
    }
    else
    {
        first = row;
        last = row;
    }
}

//==============================================================================================
// Editing

void cPlayModeEditor::changeValue(int16_t delta)
{
    uint8_t first;
    uint8_t last;
    actionRows(first, last);

    // A run of turns on the same cells is one undo step.
    const uint32_t cell = (uint32_t(lane) << 24) | (uint32_t(column) << 16) | (first << 8) | last;
    if (undoBuffer.lastEdit != cell)
    {
        storeUndo();
        undoBuffer.lastEdit = cell;
    }

    for (int r = first; r <= last; r++)
    {
        if (!changeRowValue(lane, static_cast<uint8_t>(r), column, delta))
        {
            showFxListFull();
            break;
        }
    }
    gridDirty = true;
    refreshTitle();
}

// Delete: the column back to its built-in state (Step: row order; the FX
// columns: no FX; Chance: 100%), then the cursor moves by the edit step.
void cPlayModeEditor::clearCells()
{
    uint8_t first;
    uint8_t last;
    actionRows(first, last);
    storeUndo();

    for (int r = first; r <= last; r++)
    {
        const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, r);
        bool ok = true;
        switch (column)
        {
            case kColumnStep:
                ok = userPlayModes::setStepNumber(lane, r, static_cast<uint8_t>(r + 1));
                break;
            case kColumnChance:
                ok = info.fxType == 0 ||
                     userPlayModes::setStepFx(lane, r, info.fxType, info.value, 100);
                break;
            default: ok = userPlayModes::setStepFx(lane, r, 0, 0, 100); break;
        }
        if (!ok)
        {
            showFxListFull();
            break;
        }
    }
    moveCursorByStep();
    refreshTitle();
}

// Pads in edit mode set the column on the selection (or the cursor's step),
// as the Pattern editor's: Step = pad number, FX = the FX list's entry,
// Value and Chance = spread over their range.
void cPlayModeEditor::setFromPad(uint8_t pad)
{
    uint8_t first;
    uint8_t last;
    actionRows(first, last);

    // [Fx1] held: the pad picks the FX, as in the Pattern editor (and the FX
    // list is not shown on release).
    const bool padPicksFx = column == kColumnFx && fxButtonHeld != 0 && !fxPopupOpen;
    if (padPicksFx)
    {
        fxButtonHeld = 0;
        const uint8_t type = pad + 1 < FX_COUNT ? interfaceGlobals.fxIDs[pad + 1] : 0;
        if (type == 0 || userPlayModes::allowedFxIndex(type) < 0)
        {
            showResult(type ? fxName(type) : "No FX on this pad.",
                       type ? "can't be used in play modes." : nullptr);
            return;
        }
    }

    storeUndo();
    for (int r = first; r <= last; r++)
    {
        const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, r);
        bool ok = true;
        switch (column)
        {
            case kColumnStep: ok = userPlayModes::setStepNumber(lane, r, pad + 1); break;
            case kColumnFx:
            {
                if (!padPicksFx)
                {
                    if (info.fxType == 0)
                    {
                        break;
                    }
                    const int low = userPlayModes::fxMin(info.fxType);
                    const int high = userPlayModes::fxMax(info.fxType);
                    const int value =
                      skipNoRoll(info.fxType, low + (high - low) * pad / 47, 1, low, high);
                    ok = userPlayModes::setStepFx(
                      lane, r, info.fxType, static_cast<int16_t>(value), info.chance);
                    break;
                }
                const uint8_t type = interfaceGlobals.fxIDs[pad + 1];
                ok =
                  info.fxType == type ||
                  userPlayModes::setStepFx(
                    lane, r, type, userPlayModes::fxDefault(type), info.fxType ? info.chance : 100);
                break;
            }
            default:
                if (info.fxType != 0)
                {
                    ok = userPlayModes::setStepFx(
                      lane, r, info.fxType, info.value, static_cast<uint8_t>(pad * 100 / 47));
                }
                break;
        }
        if (!ok)
        {
            showFxListFull();
            break;
        }
    }

    if (!isMultiSelection())
    {
        moveCursorByStep();
    }
    gridDirty = true;
    refreshTitle();
}

// Insert: a built-in row at the cursor; the rows below move down and the last
// one drops off (Sequencer::insert()).
void cPlayModeEditor::insertRow()
{
    uint8_t first;
    uint8_t last;
    actionRows(first, last);
    storeUndo();

    bool ok = true;
    for (int r = kRowCount - 1; r > first && ok; r--)
    {
        ok = copyRow(lane, static_cast<uint8_t>(r - 1), static_cast<uint8_t>(r));
    }
    if (!ok || !clearRow(lane, first))
    {
        showFxListFull();
    }
    gridDirty = true;
    refreshTitle();
}

// Shift+Delete: removes the row above the cursor and moves the rest up; the
// last row becomes a built-in row (Sequencer::backspace()).
void cPlayModeEditor::backspaceRow()
{
    uint8_t first;
    uint8_t last;
    actionRows(first, last);
    if (first == 0)
    {
        return;
    }
    storeUndo();

    bool ok = true;
    for (int r = first - 1; r < kRowCount - 1 && ok; r++)
    {
        ok = copyRow(lane, static_cast<uint8_t>(r + 1), static_cast<uint8_t>(r));
    }
    if (!ok || !clearRow(lane, kRowCount - 1))
    {
        showFxListFull();
    }

    if (selecting)
    {
        selectionAnchor--;
        selectionEnd--;
    }
    row--;
    gridDirty = true;
    refreshTitle();
}

//==============================================================================================
// Clipboard

void cPlayModeEditor::copyRows()
{
    uint8_t first;
    uint8_t last;
    actionRows(first, last);

    clipboard.count = static_cast<uint8_t>(last - first + 1);
    for (uint8_t i = 0; i < clipboard.count; i++)
    {
        readRow(clipboard.rows, i, lane, first + i);
    }

    // Not a local buffer: the popup draws the text later.
    snprintf(resultText,
             sizeof(resultText),
             "Copied %u step%s.",
             clipboard.count,
             clipboard.count > 1 ? "s" : "");
    showResult(resultText);
}

// From the cursor (or the selection's first row) down; modes loop, so it
// wraps past row 128. A held column button pastes only that column.
void cPlayModeEditor::pasteRows(uint8_t pasteColumn)
{
    if (clipboard.count == 0)
    {
        showResult("Nothing copied yet.");
        return;
    }

    uint8_t first;
    uint8_t last;
    actionRows(first, last);
    storeUndo();

    for (uint8_t i = 0; i < clipboard.count; i++)
    {
        const uint8_t r = (first + i) % kRowCount;
        const Rows& rows = clipboard.rows;
        const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, r);
        bool ok = true;
        switch (pasteColumn)
        {
            case kColumnStep: ok = userPlayModes::setStepNumber(lane, r, rows.stepNumber[i]); break;
            case kColumnFx:
                ok =
                  userPlayModes::setStepFx(lane, r, rows.fxType[i], rows.value[i], rows.chance[i]);
                break;
            case kColumnChance:
                if (info.fxType != 0)
                {
                    ok = userPlayModes::setStepFx(lane, r, info.fxType, info.value, rows.chance[i]);
                }
                break;
            default: ok = writeRow(rows, i, lane, r); break;
        }
        if (!ok)
        {
            showFxListFull();
            break;
        }
    }
    row = first;
    moveCursorByStep(static_cast<uint8_t>(clipboard.count - 1));
    refreshTitle();
}

//==============================================================================================
// Undo

// Keeps the cursor's lane as it is now, before an edit.
void cPlayModeEditor::storeUndo()
{
    undoBuffer.lane = lane;
    undoBuffer.userMode = userPlayModes::isUserMode(lane);
    for (uint8_t r = 0; r < kRowCount; r++)
    {
        readRow(undoBuffer.rows, r, lane, r);
    }
    undoBuffer.state = 1;
    undoBuffer.lastEdit = UINT32_MAX;
    refreshLabels();
}

// Swaps the lane with the buffer, so the same call undoes and redoes.
void cPlayModeEditor::undo(bool redo)
{
    if (undoBuffer.state != (redo ? 2 : 1))
    {
        return;
    }

    const uint8_t undoLane = undoBuffer.lane;
    const bool wasUserMode = userPlayModes::isUserMode(undoLane);
    bool ok = true;
    if (undoBuffer.userMode)
    {
        for (uint8_t r = 0; r < kRowCount && ok; r++)
        {
            Rows& rows = undoBuffer.rows;
            const userPlayModes::StepInfo now = userPlayModes::getStepInfo(undoLane, r);
            ok = writeRow(rows, r, undoLane, r);
            rows.stepNumber[r] = now.stepNumber;
            rows.fxType[r] = now.fxType;
            rows.value[r] = now.value;
            rows.chance[r] = now.chance;
        }
    }
    else
    {
        // It was the built-in mode: keep the edits for Redo, then reset.
        for (uint8_t r = 0; r < kRowCount; r++)
        {
            readRow(undoBuffer.rows, r, undoLane, r);
        }
        userPlayModes::resetMode(undoLane);
    }
    if (!ok)
    {
        showFxListFull();
    }

    undoBuffer.userMode = wasUserMode;
    undoBuffer.state = redo ? 1 : 2;
    undoBuffer.lastEdit = UINT32_MAX;
    selecting = false;
    lane = undoLane;
    gridDirty = true;
    refreshTitle();
    refreshLabels();
    display.synchronizeRefresh();
}

//==============================================================================================
// Fill

void cPlayModeEditor::openFill()
{
    fillOpen = true;
    fillKind = fillKindFor(column);
    if (!isFillPlaceValid(fillPlace))
    {
        fillPlace = kPlaceType;
    }

    ip->clearButtonsRange(interfaceButton0, interfaceButton7);
    for (uint8_t button = interfaceButton0; button <= interfaceButton7; button++)
    {
        ip->setButtonObj(button, functActionButton);
    }
    showFillPopup();
}

// As the Pattern editor's: the density only for Random / Each / Euclidean,
// the FX type only for FX fills, To only for From-To and Random.
bool cPlayModeEditor::isFillPlaceValid(int place) const
{
    const FillData& data = fillData[fillKind];
    switch (place)
    {
        case kPlaceDensity: return data.where >= kWhereRandom;
        case kPlaceFxType:
            return fillKind == kFillFx || (fillKind == kFillStep && data.type >= kTypeUpFrom);
        case kPlaceFrom: return fillTypeUsesFrom(data.type);
        case kPlaceTo: return fillTypeUsesTo(data.type);
        default: return place >= 0 && place < kPlaceCount;
    }
}

void cPlayModeEditor::setFillPlace(int place)
{
    if (isFillPlaceValid(place))
    {
        fillPlace = static_cast<int8_t>(place);
    }
    refreshFillPopup();
}

void cPlayModeEditor::changeFillPlace(int delta)
{
    int place = fillPlace + delta;
    while (place >= 0 && place < kPlaceCount && !isFillPlaceValid(place))
    {
        place += delta;
    }
    setFillPlace(place);
}

void cPlayModeEditor::changeFillData(int16_t delta, bool rollOver)
{
    FillData& data = fillData[fillKind];
    int16_t* value = nullptr;
    int low = 0;
    int high = 0;

    switch (fillPlace)
    {
        case kPlaceWhere:
            value = &data.where;
            high = kWhereCount - 1;
            break;
        case kPlaceDensity:
            if (data.where < kWhereRandom)
            {
                return;
            }
            value = &data.density[data.where - kWhereRandom];
            low = 1;
            high = Sequencer::MAX_FILL_DENSITY;
            break;
        case kPlaceFxType:
        {
            if (fillKind == kFillStep)
            {
                value = &data.countAllRows;
                high = 1;
                break;
            }
            if (fillKind != kFillFx)
            {
                return;
            }
            const int index = userPlayModes::allowedFxIndex(static_cast<uint8_t>(data.fxType));
            int next = (index < 0 ? 0 : index) + clampInt(delta, -5, 5);
            if (next < 0 || next >= fxTypeCount)
            {
                next = rollOver ? 0 : clampInt(next, 0, fxTypeCount - 1);
            }
            data.fxType = userPlayModes::allowedFxType(next);
            refreshFillPopup();
            return;
        }
        case kPlaceType:
            value = &data.type;
            high = fillTypeCount(fillKind) - 1;
            break;
        case kPlaceFrom:
        case kPlaceTo:
            if (!isFillPlaceValid(fillPlace))
            {
                return;
            }
            value = fillPlace == kPlaceFrom ? &data.from : &data.to;
            fillRange(fillKind, static_cast<uint8_t>(data.fxType), low, high);
            *value = static_cast<int16_t>(skipNoRoll(static_cast<uint8_t>(data.fxType),
                                                     clampInt(*value + delta, low, high),
                                                     delta >= 0 ? 1 : -1,
                                                     low,
                                                     high));
            refreshFillPopup();
            return;
        default: return;
    }

    const int next = *value + delta;
    *value =
      static_cast<int16_t>(next < low ? low : (next > high ? (rollOver ? low : high) : next));
    if (!isFillPlaceValid(fillPlace))
    {
        fillPlace = kPlaceType;
    }
    refreshFillPopup();
}

void cPlayModeEditor::setFillDataFromPad(uint8_t pad)
{
    FillData& data = fillData[fillKind];
    int low;
    int high;
    fillRange(fillKind, static_cast<uint8_t>(data.fxType), low, high);

    switch (fillPlace)
    {
        case kPlaceWhere:
            if (pad < kWhereCount)
            {
                data.where = pad;
            }
            break;
        case kPlaceDensity:
            if (data.where >= kWhereRandom)
            {
                data.density[data.where - kWhereRandom] =
                  static_cast<int16_t>(clampInt(pad + 1, 1, Sequencer::MAX_FILL_DENSITY));
            }
            break;
        case kPlaceFxType:
            if (fillKind == kFillFx && pad < fxTypeCount)
            {
                data.fxType = userPlayModes::allowedFxType(pad);
            }
            else if (fillKind == kFillStep && pad < 2)
            {
                data.countAllRows = pad;
            }
            break;
        case kPlaceType:
            if (pad < fillTypeCount(fillKind))
            {
                data.type = pad;
            }
            break;
        case kPlaceFrom:
        case kPlaceTo:
        {
            int value = fillKind == kFillStep ? pad + 1 : low + (high - low) * pad / 47;
            value = skipNoRoll(
              static_cast<uint8_t>(data.fxType), clampInt(value, low, high), 1, low, high);
            (fillPlace == kPlaceFrom ? data.from : data.to) = static_cast<int16_t>(value);
            break;
        }
        default: break;
    }
    if (!isFillPlaceValid(fillPlace))
    {
        fillPlace = kPlaceType;
    }
    refreshFillPopup();
}

// As Sequencer::fillLinear*() / fillRandom*(), on the selection (or the
// cursor's step).
void cPlayModeEditor::applyFill()
{
    const FillData& data = fillData[fillKind];
    const uint8_t fxType = static_cast<uint8_t>(data.fxType);
    uint8_t first;
    uint8_t last;
    actionRows(first, last);
    storeUndo();

    int low;
    int high;
    fillRange(fillKind, fxType, low, high);
    const int length = last - first + 1;
    const int from = data.from;
    const int to = data.type == kTypeConstant ? data.from : data.to;
    // "Up to" / "Down to" end at To: fill those from the last row back.
    const bool backwards = data.type == kTypeUpTo || data.type == kTypeDownTo;
    int filled = 0; // rows filled so far, for the sequences

    for (int i = 0; i < length; i++)
    {
        const int offset = backwards ? length - 1 - i : i;
        const uint8_t r = static_cast<uint8_t>(first + offset);
        const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, r);
        if (!isRowToFill(data, offset, length, info.fxType != 0))
        {
            continue;
        }

        int value;
        if (data.type >= kTypeUpFrom)
        {
            // How far along the sequence this row is: rows filled so far, or
            // (Count = All rows) rows passed, from the end for the "to" types.
            const int k = data.countAllRows ? i : filled;
            int step;
            switch (data.type)
            {
                case kTypeUpFrom: step = from + k; break;
                case kTypeUpTo: step = to - k; break;
                case kTypeDownFrom: step = from - k; break;
                case kTypeDownTo: step = to + k; break;
                case kTypeLoopUp: step = from + k % to; break;
                default: step = from - k % to; break; // kTypeLoopDown
            }
            // Step numbers wrap around, 128 -> 1 and 1 -> 128.
            value = ((step - 1) % kRowCount + kRowCount) % kRowCount + 1;
            filled++;
        }
        else if (data.type == kTypeRandom)
        {
            value = utilMathRandom::random(from < to ? from : to, (from < to ? to : from) + 1);
        }
        else
        {
            value = length > 1 ? from + (to - from) * offset / (length - 1) : from;
        }
        value = clampInt(value, low, high);

        bool ok = true;
        switch (fillKind)
        {
            case kFillStep:
                ok = userPlayModes::setStepNumber(lane, r, static_cast<uint8_t>(value));
                break;
            case kFillFx:
                value = skipNoRoll(fxType, value, 1, low, high);
                ok = userPlayModes::setStepFx(
                  lane, r, fxType, static_cast<int16_t>(value), info.fxType ? info.chance : 100);
                break;
            default:
                // Chance: only steps that have an FX.
                if (info.fxType != 0)
                {
                    ok = userPlayModes::setStepFx(
                      lane, r, info.fxType, info.value, static_cast<uint8_t>(value));
                }
                break;
        }
        if (!ok)
        {
            showFxListFull();
            break;
        }
    }

    closeFill();
}

//==============================================================================================
// FX list popup

void cPlayModeEditor::openFxPopup()
{
    if (fxPopupOpen || fillOpen)
    {
        return;
    }
    const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, row);
    fxPopupOpen = true;
    column = kColumnFx;
    refreshGrid();
    // The popup takes over the arrows, jog wheel and pads until it is hidden.
    mtPopups.showStepPopup(stepPopupFx,
                           static_cast<int8_t>(interfaceGlobals.fxIdToName(info.fxType)));
}

// As the Pattern editor's: the chosen FX is set in edit mode, on a single step.
void cPlayModeEditor::closeFxPopup()
{
    if (!fxPopupOpen)
    {
        return;
    }
    const int16_t listPosition = mtPopups.getStepPopupValue();
    const uint8_t type =
      (listPosition >= 0 && listPosition < FX_COUNT) ? interfaceGlobals.fxIDs[listPosition] : 0;

    mtPopups.hideStepPopups();
    fxPopupOpen = false;
    setDefaultScreenFunct();

    if (editMode && !isMultiSelection())
    {
        const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, row);
        if (type != 0 && userPlayModes::allowedFxIndex(type) < 0)
        {
            showResult(fxName(type), "can't be used in play modes.");
        }
        else if (type != info.fxType)
        {
            storeUndo();
            const bool ok = type == 0 ? userPlayModes::setStepFx(lane, row, 0, 0, 100)
                                      : userPlayModes::setStepFx(lane,
                                                                 row,
                                                                 type,
                                                                 userPlayModes::fxDefault(type),
                                                                 info.fxType ? info.chance : 100);
            if (!ok)
            {
                showFxListFull();
            }
        }
    }

    gridDirty = true;
    refreshTitle();
}

//==============================================================================================
// Actions

void cPlayModeEditor::save()
{
    char projectDir[PATCH_SIZE];
    const char* dir = nullptr;
    if ((mtProject.values.projectNotSavedFlag & 2) == 0 &&
        newFileManager.getCurrentProjectName()[0] != 0)
    {
        snprintf(projectDir,
                 sizeof(projectDir),
                 "%s/%s",
                 newFileManager.getCurrentProjectPath(),
                 newFileManager.getCurrentProjectName());
        dir = projectDir;
    }

    if (!userPlayModes::save(newFileManager.getFileSystem(), dir))
    {
        showResult("Could not save playmodes.txt.");
    }
    else if (dir == nullptr)
    {
        showResult("Play modes saved to the Workspace.", "Save the project to keep them.");
    }
    else
    {
        showResult("Play modes saved.");
    }
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::revert()
{
    // Errors in the file show the usual play modes popup.
    userPlayModes::loadFromWorkspace(newFileManager.getFileSystem());
    undoBuffer.state = 0; // the buffer may no longer match
    showResult("Back to the saved play modes.");
    gridDirty = true;
    refreshTitle();
    refreshLabels();
}

void cPlayModeEditor::resetLane()
{
    storeUndo();
    userPlayModes::resetMode(lane);
    showResult("Lane back to its built-in mode.", "Save to keep the change.");
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::showResult(const char* line1, const char* line2)
{
    if (line2 == nullptr)
    {
        mtPopups.show(0, line1, 2);
    }
    else
    {
        mtPopups.show(0, line1, line2, 3);
    }
}

void cPlayModeEditor::showFxListFull()
{
    showResult("The FX list is full.", "At most 64 different FX settings.");
}

//==============================================================================================
// Inputs

static void functPlayAction()
{
    if (sequencer.getSeqState() == Sequencer::SEQ_STATE_STOP)
    {
        sequencer.playPattern();
    }
    else
    {
        sequencer.stop();
    }
}

// Rec: edit mode on / off.
static void functRecAction(uint8_t state)
{
    if (state == buttonPress && !PME->fillOpen &&
        !PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        PME->toggleEditMode();
    }
}

// Up / Down: as the Pattern editor's, rows move only in edit mode or when
// stopped (otherwise the cursor follows the playhead). Shift selects.
static void verticalMove(int delta)
{
    if (PME->fillOpen)
    {
        // Lists run top to bottom, values bottom to top.
        const int place = PME->fillPlace;
        const bool isList = place == cPlayModeEditor::kPlaceWhere ||
                            place == cPlayModeEditor::kPlaceFxType ||
                            place == cPlayModeEditor::kPlaceType;
        PME->changeFillData(static_cast<int16_t>(isList ? delta : -delta));
        return;
    }
    if (PME->heldPlace == cPlayModeEditor::kHeldLane)
    {
        PME->moveLane(-delta);
        return;
    }
    if (PME->heldPlace == cPlayModeEditor::kHeldEditStep)
    {
        PME->changeEditStep(-delta);
        return;
    }
    if (!PME->editMode && !sequencer.isStop())
    {
        return;
    }

    if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        if (delta < 0 && PME->row == 0)
        {
            PME->selectWholeLane();
        }
        else
        {
            PME->extendSelection(delta);
        }
    }
    else
    {
        PME->clearSelection();
        PME->moveRow(delta);
    }
}

static void functUp()
{
    verticalMove(-1);
}

static void functDown()
{
    verticalMove(1);
}

// Left / Right: the lane (the selection stays in one lane, so Shift does nothing).
static void horizontalMove(int delta)
{
    if (PME->fillOpen)
    {
        PME->changeFillPlace(delta);
        return;
    }
    if (PME->heldPlace == cPlayModeEditor::kHeldLane)
    {
        PME->moveLane(delta);
        return;
    }
    if (PME->heldPlace == cPlayModeEditor::kHeldEditStep)
    {
        PME->changeEditStep(delta);
        return;
    }
    if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        return;
    }
    PME->moveLane(delta);
}

static void functLeft()
{
    horizontalMove(-1);
}

static void functRight()
{
    horizontalMove(1);
}

// Delete clears; Shift+Delete is backspace. Edit mode only.
static void functDelete(uint8_t state)
{
    if (PME->fillOpen || !PME->editMode || (state != buttonPress && state != buttonHold))
    {
        return;
    }
    if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        if (state == buttonPress)
        {
            PME->backspaceRow();
        }
    }
    else
    {
        PME->clearCells();
    }
}

// Insert adds a row (edit mode); Shift+Insert goes to the first row.
static void functInsert(uint8_t state)
{
    if (PME->fillOpen || (state != buttonPress && state != buttonHold))
    {
        return;
    }
    if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        PME->clearSelection();
        PME->row = 0;
        PME->gridDirty = true;
        PME->refreshTitle();
    }
    else if (PME->editMode)
    {
        PME->insertRow();
    }
}

static uint8_t columnForButton(uint8_t button)
{
    switch (button)
    {
        // As the Pattern editor's: [Vol] is Fx1 and [Fx] is Fx2 (whatever
        // interfaceButtonFx1/Fx2 say). Instr's column is skipped.
        case interfaceButtonNote: return cPlayModeEditor::kColumnStep;
        case interfaceButtonVol: return cPlayModeEditor::kColumnFx;
        case interfaceButtonFx: return cPlayModeEditor::kColumnChance;
        default: return cPlayModeEditor::kColumnCount;
    }
}

// Note / Fx1 / Fx2 pick the Step / FX / Chance column (Instr does nothing).
// Holding Fx1 shows the FX list, as in the Pattern editor; a pad pressed
// meanwhile picks the FX instead. With the fill popup open they switch what
// it fills.
static void functColumnButton(uint8_t button, uint8_t state)
{
    const uint8_t newColumn = columnForButton(button);
    if (newColumn == cPlayModeEditor::kColumnCount)
    {
        return;
    }
    if (state == buttonPress)
    {
        PME->selectColumn(newColumn);
        if (PME->fillOpen)
        {
            PME->fillKind = fillKindFor(PME->column);
            if (!PME->isFillPlaceValid(PME->fillPlace))
            {
                PME->fillPlace = cPlayModeEditor::kPlaceType;
            }
            PME->showFillPopup();
            return;
        }
        if (newColumn == cPlayModeEditor::kColumnFx)
        {
            PME->fxButtonHeld = button;
            PME->fxButtonTimer.value = 0;
        }
    }
    else if (state == buttonRelease)
    {
        if (button == PME->fxButtonHeld)
        {
            PME->fxButtonHeld = 0;
            PME->closeFxPopup();
        }
    }
}

// Copy copies the selection (or the cursor's step); Shift+Copy pastes, only
// the column whose button is held, if one is. Edit mode only, as in the
// Pattern editor.
static void functCopy(uint8_t state)
{
    if (state != buttonPress || PME->fillOpen || !PME->editMode)
    {
        return;
    }
    if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        uint8_t column = cPlayModeEditor::kColumnCount;
        const uint8_t buttons[] = { interfaceButtonNote, interfaceButtonVol, interfaceButtonFx };
        for (uint8_t button : buttons)
        {
            if (PME->buttons->isButtonPressed(button))
            {
                column = columnForButton(button);
            }
        }
        PME->pasteRows(column);
    }
    else
    {
        PME->copyRows();
    }
}

static void functEncoder(int16_t value)
{
    if (PME->fillOpen)
    {
        const int place = PME->fillPlace;
        const bool isList = place == cPlayModeEditor::kPlaceWhere ||
                            place == cPlayModeEditor::kPlaceFxType ||
                            place == cPlayModeEditor::kPlaceType;
        PME->changeFillData(
          static_cast<int16_t>(interfaceGlobals.invertValForList && isList ? -value : value));
        return;
    }
    if (PME->heldPlace == cPlayModeEditor::kHeldLane)
    {
        PME->moveLane(value);
        return;
    }
    if (PME->heldPlace == cPlayModeEditor::kHeldEditStep)
    {
        PME->changeEditStep(value);
        return;
    }

    if (!PME->editMode)
    {
        // Moves the cursor (up to 10 rows a turn), only when stopped.
        if (!sequencer.isStop())
        {
            return;
        }
        value = interfaceGlobals.invertValForList ? -value : value;
        PME->clearSelection();
        PME->moveRow(clampInt(value, -10, 10));
        return;
    }
    PME->changeValue(value);
}

static void functSwitchModule(uint8_t button)
{
    // Performance mode kept running for this screen: let it decide whether
    // it can go on in the module the button opens.
    performanceMode.leaveToModule(button);
    PME->eventFunct(eventSwitchModule, PME, &button, 0);
}

// Screen buttons: Lane (held) / Revert / Step jump (held) / Fill / Reset lane
// / Undo / Redo / Save; in the fill popup they pick its columns, then Cancel
// and Fill.
static void functActionButton(uint8_t button, uint8_t state)
{
    const uint8_t index = button - interfaceButton0;

    if (PME->fillOpen)
    {
        if (state != buttonPress)
        {
            return;
        }
        if (index == 6)
        {
            PME->closeFill();
        }
        else if (index == 7)
        {
            PME->applyFill();
        }
        else if (index == PME->fillPlace && index != cPlayModeEditor::kPlaceFrom &&
                 index != cPlayModeEditor::kPlaceTo)
        {
            // A second press steps through the list.
            PME->changeFillData(1, true);
        }
        else
        {
            PME->setFillPlace(index);
        }
        return;
    }

    if (index == cPlayModeEditor::kHeldLane || index == cPlayModeEditor::kHeldEditStep)
    {
        if (state == buttonPress)
        {
            PME->setHeldPlace(static_cast<int8_t>(index));
        }
        else if (state == buttonRelease && PME->heldPlace == index)
        {
            PME->setHeldPlace(cPlayModeEditor::kHeldNone);
        }
        return;
    }

    if (state != buttonPress)
    {
        return;
    }
    switch (index)
    {
        case 1: PME->revert(); break;
        case 3:
            if (PME->editMode)
            {
                PME->openFill();
            }
            break;
        case 4:
            if (PME->editMode)
            {
                PME->resetLane();
            }
            break;
        case 5:
            if (PME->editMode)
            {
                PME->undo(false);
            }
            break;
        case 6:
            if (PME->editMode)
            {
                PME->undo(true);
            }
            break;
        case 7: PME->save(); break;
        default: break;
    }
}

static void functPads(uint8_t pad, uint8_t state, int16_t velo)
{
    (void)velo;
    if (state != buttonPress)
    {
        return;
    }
    if (PME->fillOpen)
    {
        PME->setFillDataFromPad(pad);
        return;
    }
    if (PME->heldPlace == cPlayModeEditor::kHeldLane)
    {
        PME->setLane(pad);
        return;
    }
    if (PME->heldPlace == cPlayModeEditor::kHeldEditStep)
    {
        PME->changeEditStep(pad - mtProject.values.patternEditStep);
        return;
    }
    if (PME->editMode)
    {
        PME->setFromPad(pad);
    }
}

// Tracker Mini: the module menu, with Perform selected, so that a short press
// of Menu goes back to Performance mode. The menu's choice goes through
// functSwitchModule(), like the module buttons.
static void functMenu(uint8_t state)
{
    if (state == buttonPress)
    {
        mtMenuPopup.showModulePopup(&performanceMode, functSwitchModule);
    }
    else if (state == buttonRelease && mtMenuPopup.isPopupVisible())
    {
        mtMenuPopup.hideModulePopup(PME);
    }
}
