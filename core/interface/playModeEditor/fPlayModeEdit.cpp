#include "playModeEditor/playModeEditor.h"

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
static void functUp();
static void functDown();
static void functLeft();
static void functRight();
static void functDelete();
static void functColumnButton(uint8_t button);
static void functFxButton(uint8_t state);
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

// Fill types. Step column: up, down, random; Value and Chance: ramp, random.
const char* const kStepFillTypes[] = { "Up", "Down", "Random" };
const char* const kValueFillTypes[] = { "Ramp", "Random" };
enum : uint8_t
{
    kFillUp,
    kFillDown,
    kFillStepRandom,
};
enum : uint8_t
{
    kFillRamp,
    kFillValueRandom,
};

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

    if (info.fxType == 0)
    {
        strcpy(cell.instr, "--");
        strcpy(cell.fx[0], "---");
        strcpy(cell.fx[1], "---");
        return;
    }

    // The FX letter as in the pattern editor. instr[2] must stay 0 (it marks MIDI).
    cell.instr[0] = fxName(info.fxType)[0];
    cell.instr[1] = ' ';
    cell.instr[2] = 0;
    cell.instr[3] = 0;

    sequencer.makeFxValLabelWithAlgin(cell.fx[0],
                                      sizeof(cell.fx[0]),
                                      info.fxType,
                                      static_cast<uint8_t>(info.value),
                                      Sequencer::fxValLabelAlign_right);
    snprintf(cell.fx[1], sizeof(cell.fx[1]), "%3u%%", info.chance);
}

// Order position (0-127) of the first playing track in `lane`'s mode, or -1.
int16_t playheadFor(uint8_t lane)
{
    if (!sequencer.isPlay() || !sequencer.isPerformanceMode())
    {
        return -1;
    }
    const auto& tracks = sequencer.ptrPlayer->track;
    const int trackCount = sizeof(tracks) / sizeof(tracks[0]);
    for (int t = 0; t < trackCount; t++)
    {
        if (tracks[t].performancePlayMode == Sequencer::PLAYMODE_CUSTOM_1 + lane)
        {
            return tracks[t].actual_pos_absolute % cPlayModeEditor::kRowCount;
        }
    }
    return -1;
}

uint8_t fillTypeCount(uint8_t column)
{
    return column == cPlayModeEditor::kColumnStep ? 3 : 2;
}

void fillRange(uint8_t column, uint8_t fxType, int& low, int& high)
{
    switch (column)
    {
        case cPlayModeEditor::kColumnStep:
            low = 1;
            high = cPlayModeEditor::kRowCount;
            break;
        case cPlayModeEditor::kColumnValue:
            low = userPlayModes::fxMin(fxType);
            high = userPlayModes::fxMax(fxType);
            break;
        default:
            low = 0;
            high = 100;
            break;
    }
}

void formatFillValue(char* out, size_t size, uint8_t column, uint8_t fxType, int16_t value)
{
    if (column == cPlayModeEditor::kColumnValue)
    {
        sequencer.makeFxValLabelWithAlgin(
          out, size, fxType, static_cast<uint8_t>(value), Sequencer::fxValLabelAlign_center);
    }
    else if (column == cPlayModeEditor::kColumnChance)
    {
        snprintf(out, size, "%d%%", value);
    }
    else
    {
        snprintf(out, size, "%d", value);
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

    const int16_t playhead = playheadFor(lane);

    // Follow the playhead, as the pattern editor does while playing.
    if (followPlayhead && playhead >= 0 && playhead != row && !selecting && !fillOpen &&
        !fxPopupOpen)
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
    followPlayhead = true;
    selecting = false;
    selectionWholeLane = false;
    fillOpen = false;
    fxPopupOpen = false;

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
    fillOpen = false;
    restorePatternView();
    mtPadBoard.releaseAllInstruments();
    padsBacklight.clearGlobalPads(1, 1, 1);
}

void cPlayModeEditor::setDefaultScreenFunct()
{
    ip->clearButtonsRange(interfaceButton0, interfaceButton7);
    ip->clearAllPots();

    ip->setButtonObj(interfaceButtonPlay, buttonPress, functPlayAction);

    ip->setButtonObj(interfaceButtonUp, buttonPress, functUp);
    ip->setButtonObj(interfaceButtonDown, buttonPress, functDown);
    ip->setButtonObj(interfaceButtonLeft, buttonPress, functLeft);
    ip->setButtonObj(interfaceButtonRight, buttonPress, functRight);
    ip->setButtonObj(interfaceButtonDelete, buttonPress, functDelete);
    ip->setButtonObj(interfaceButtonCopy, functCopy);

    ip->setButtonObj(interfaceButtonNote, buttonPress, functColumnButton);
    ip->setButtonObj(interfaceButtonInstr, buttonPress, functColumnButton);
    ip->setButtonObj(interfaceButtonVol, buttonPress, functColumnButton);
    ip->setButtonObj(interfaceButtonFx, functFxButton);

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
    g.selectColor = 0;
    g.selectColumn = 0;
    g.showTrackNames = true;

    // The FX list popup covers the right of the grid.
    g.popupMode = fxPopupOpen ? (2 | (mtConfig.interface.fxPopupDescription ? 4 : 0)) : 0;

    if (selecting)
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
    g.playheadStartTrack = cursorTrack;
    g.playheadEndTrack = cursorTrack;

    display.refreshControl(grid);
}

void cPlayModeEditor::refreshTitle()
{
    if (fillOpen)
    {
        uint8_t first = 0;
        uint8_t last = kRowCount - 1;
        if (selecting)
        {
            actionRows(first, last);
        }
        snprintf(
          titleText, sizeof(titleText), "Fill C%u  rows %u-%u", lane + 1, first + 1, last + 1);
        titleBar->setTitle(titleText);
        return;
    }

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

void cPlayModeEditor::moveRow(int delta, bool user)
{
    if (user)
    {
        followPlayhead = false;
    }
    row = (row + kRowCount + delta % kRowCount) % kRowCount;
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::moveColumn(int delta)
{
    const uint8_t oldLane = lane;
    int newColumn = column + delta;
    if (newColumn < 0)
    {
        if (lane > 0)
        {
            lane--;
            newColumn = kColumnCount - 1;
        }
        else
        {
            newColumn = 0;
        }
    }
    else if (newColumn >= kColumnCount)
    {
        if (lane < kLaneCount - 1)
        {
            lane++;
            newColumn = 0;
        }
        else
        {
            newColumn = kColumnCount - 1;
        }
    }
    column = static_cast<uint8_t>(newColumn);
    if (lane != oldLane)
    {
        // A new lane: drop the selection and follow its playhead.
        selecting = false;
        selectionWholeLane = false;
        followPlayhead = true;
    }
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::moveLane(int delta)
{
    lane = static_cast<uint8_t>(clampInt(lane + delta, 0, kLaneCount - 1));
    selecting = false;
    selectionWholeLane = false;
    followPlayhead = true;
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::selectColumn(uint8_t newColumn)
{
    column = newColumn < kColumnCount ? newColumn : uint8_t(kColumnStep);
    gridDirty = true;
}

//==============================================================================================
// Selection

void cPlayModeEditor::extendSelection(int delta)
{
    if (!selecting || selectionWholeLane)
    {
        selecting = true;
        selectionWholeLane = false;
        selectionAnchor = row;
    }
    // No wrapping while selecting: a selection is one run of rows.
    row = static_cast<uint8_t>(clampInt(row + delta, 0, kRowCount - 1));
    followPlayhead = false;
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::selectWholeLane()
{
    selecting = true;
    selectionWholeLane = true;
    followPlayhead = false;
    gridDirty = true;
}

void cPlayModeEditor::clearSelection()
{
    if (selecting)
    {
        selecting = false;
        selectionWholeLane = false;
        gridDirty = true;
    }
}

void cPlayModeEditor::actionRows(uint8_t& first, uint8_t& last) const
{
    if (selecting && selectionWholeLane)
    {
        first = 0;
        last = kRowCount - 1;
    }
    else if (selecting)
    {
        first = selectionAnchor < row ? selectionAnchor : row;
        last = selectionAnchor < row ? row : selectionAnchor;
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
    const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, row);
    bool ok = true;
    followPlayhead = false;

    switch (column)
    {
        case kColumnStep:
        {
            const int step = clampInt(info.stepNumber + delta, 1, kRowCount);
            ok = userPlayModes::setStepNumber(lane, row, static_cast<uint8_t>(step));
            break;
        }
        case kColumnFx:
        {
            // 0 = no FX, then the allowed FX in menu order.
            const int count = userPlayModes::allowedFxCount();
            const int current = info.fxType ? userPlayModes::allowedFxIndex(info.fxType) + 1 : 0;
            const int next = clampInt(current + delta, 0, count);
            if (next == current)
            {
                return;
            }
            const uint8_t type = next ? userPlayModes::allowedFxType(next - 1) : 0;
            const int16_t value = type ? userPlayModes::fxDefault(type) : 0;
            ok = userPlayModes::setStepFx(lane, row, type, value, info.fxType ? info.chance : 100);
            break;
        }
        case kColumnValue:
        {
            if (info.fxType == 0)
            {
                return;
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
            if (value == info.value)
            {
                return;
            }
            ok = userPlayModes::setStepFx(
              lane, row, info.fxType, static_cast<int16_t>(value), info.chance);
            break;
        }
        case kColumnChance:
        {
            if (info.fxType == 0)
            {
                return;
            }
            const int chance = clampInt(info.chance + delta, 0, 100);
            ok = userPlayModes::setStepFx(
              lane, row, info.fxType, info.value, static_cast<uint8_t>(chance));
            break;
        }
        default: return;
    }

    if (!ok)
    {
        showFxListFull();
    }
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::clearCells()
{
    uint8_t first;
    uint8_t last;
    actionRows(first, last);
    followPlayhead = false;

    for (int r = first; r <= last; r++)
    {
        const bool ok = column == kColumnStep
                          ? userPlayModes::setStepNumber(lane, r, static_cast<uint8_t>(r + 1))
                          : userPlayModes::setStepFx(lane, r, 0, 0, 100);
        if (!ok)
        {
            showFxListFull();
            break;
        }
    }
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::setStepFromPad(uint8_t pad)
{
    if (column != kColumnStep || pad >= kRowCount)
    {
        return;
    }
    clearSelection();
    if (!userPlayModes::setStepNumber(lane, row, pad + 1))
    {
        showFxListFull();
        return;
    }
    moveRow(1);
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
        const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, first + i);
        clipboard.stepNumber[i] = info.stepNumber;
        clipboard.fxType[i] = info.fxType;
        clipboard.value[i] = info.value;
        clipboard.chance[i] = info.chance;
    }

    char text[32];
    snprintf(
      text, sizeof(text), "Copied %u step%s.", clipboard.count, clipboard.count > 1 ? "s" : "");
    showResult(text);
}

void cPlayModeEditor::pasteRows()
{
    if (clipboard.count == 0)
    {
        showResult("Nothing copied yet.");
        return;
    }

    // From the cursor down; modes loop, so it wraps past row 128.
    followPlayhead = false;
    for (uint8_t i = 0; i < clipboard.count; i++)
    {
        const uint8_t r = (row + i) % kRowCount;
        const bool ok = userPlayModes::setStepNumber(lane, r, clipboard.stepNumber[i]) &&
                        userPlayModes::setStepFx(
                          lane, r, clipboard.fxType[i], clipboard.value[i], clipboard.chance[i]);
        if (!ok)
        {
            showFxListFull();
            break;
        }
    }
    clearSelection();
    gridDirty = true;
    refreshTitle();
}

//==============================================================================================
// Fill

void cPlayModeEditor::openFill()
{
    if (column == kColumnFx)
    {
        showResult("Fill works in the Step, Value", "and Chance columns.");
        return;
    }

    // The selection, or the whole lane.
    uint8_t first = 0;
    uint8_t last = kRowCount - 1;
    if (selecting)
    {
        actionRows(first, last);
    }

    fillColumn = column;
    fillParam = 0;
    fillType = 0;
    fillFxType = 0;

    switch (fillColumn)
    {
        case kColumnStep:
            fillFrom = userPlayModes::getStepInfo(lane, first).stepNumber;
            fillTo = static_cast<int16_t>(clampInt(fillFrom + (last - first), 1, kRowCount));
            break;
        case kColumnValue:
        {
            // The FX of the first selected step that has one.
            for (int r = first; r <= last && fillFxType == 0; r++)
            {
                const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, r);
                fillFxType = info.fxType;
                fillFrom = info.value;
            }
            if (fillFxType == 0)
            {
                showResult("Give a step an FX first:", "Fill writes values for that FX.");
                return;
            }
            fillTo = userPlayModes::fxMax(fillFxType);
            break;
        }
        default:
            fillFrom = 100;
            fillTo = 0;
            break;
    }

    fillOpen = true;
    followPlayhead = false;
    changeFillValue(0);
    refreshTitle();
}

void cPlayModeEditor::closeFill()
{
    fillOpen = false;
    refreshLabels();
    gridDirty = true;
    refreshTitle();
    display.synchronizeRefresh();
}

void cPlayModeEditor::selectFillParam(uint8_t param)
{
    fillParam = param < 3 ? param : fillParam;
    refreshLabels();
    display.synchronizeRefresh();
}

void cPlayModeEditor::changeFillValue(int16_t delta)
{
    int low;
    int high;
    fillRange(fillColumn, fillFxType, low, high);
    const int direction = delta >= 0 ? 1 : -1;

    switch (fillParam)
    {
        case 0:
            fillType =
              static_cast<uint8_t>(clampInt(fillType + delta, 0, fillTypeCount(fillColumn) - 1));
            break;
        case 1:
            fillFrom = static_cast<int16_t>(
              skipNoRoll(fillFxType, clampInt(fillFrom + delta, low, high), direction, low, high));
            break;
        default:
            fillTo = static_cast<int16_t>(
              skipNoRoll(fillFxType, clampInt(fillTo + delta, low, high), direction, low, high));
            break;
    }

    snprintf(fillText[0],
             sizeof(fillText[0]),
             "%s",
             fillColumn == kColumnStep ? kStepFillTypes[fillType] : kValueFillTypes[fillType]);
    formatFillValue(fillText[1], sizeof(fillText[1]), fillColumn, fillFxType, fillFrom);
    formatFillValue(fillText[2], sizeof(fillText[2]), fillColumn, fillFxType, fillTo);
    refreshLabels();
    display.synchronizeRefresh();
}

void cPlayModeEditor::applyFill()
{
    uint8_t first = 0;
    uint8_t last = kRowCount - 1;
    if (selecting)
    {
        actionRows(first, last);
    }

    const int count = last - first + 1;
    const int low = fillFrom < fillTo ? fillFrom : fillTo;
    const int high = fillFrom < fillTo ? fillTo : fillFrom;
    const int span = high - low + 1;

    for (int i = 0; i < count; i++)
    {
        const uint8_t r = static_cast<uint8_t>(first + i);
        const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, r);

        int value;
        if (fillColumn == kColumnStep)
        {
            value = fillType == kFillUp     ? low + i % span
                    : fillType == kFillDown ? high - i % span
                                            : utilMathRandom::random(low, high + 1);
        }
        else if (fillType == kFillRamp)
        {
            value = count > 1 ? fillFrom + (fillTo - fillFrom) * i / (count - 1) : fillFrom;
        }
        else
        {
            value = utilMathRandom::random(low, high + 1);
        }

        bool ok = true;
        switch (fillColumn)
        {
            case kColumnStep:
                ok = userPlayModes::setStepNumber(lane, r, static_cast<uint8_t>(value));
                break;
            case kColumnValue:
                value = skipNoRoll(fillFxType, value, 1, low, high);
                ok = userPlayModes::setStepFx(lane,
                                              r,
                                              fillFxType,
                                              static_cast<int16_t>(value),
                                              info.fxType ? info.chance : 100);
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
    followPlayhead = false;
    column = kColumnFx;
    refreshGrid();
    // The popup takes over the arrows, jog wheel and pads until it is hidden.
    mtPopups.showStepPopup(stepPopupFx,
                           static_cast<int8_t>(interfaceGlobals.fxIdToName(info.fxType)));
}

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

    if (type != 0 && userPlayModes::allowedFxIndex(type) < 0)
    {
        showResult(fxName(type), "can't be used in play modes.");
    }
    else
    {
        uint8_t first;
        uint8_t last;
        actionRows(first, last);
        for (int r = first; r <= last; r++)
        {
            const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, r);
            if (info.fxType == type)
            {
                continue; // keep its value and chance
            }
            const bool ok =
              type == 0
                ? userPlayModes::setStepFx(lane, r, 0, 0, 100)
                : userPlayModes::setStepFx(
                    lane, r, type, userPlayModes::fxDefault(type), info.fxType ? info.chance : 100);
            if (!ok)
            {
                showFxListFull();
                break;
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
    showResult("Back to the saved play modes.");
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::resetLane()
{
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
        PME->followPlayhead = true;
        sequencer.playPattern();
    }
    else
    {
        sequencer.stop();
    }
}

static void functUp()
{
    if (PME->fillOpen)
    {
        return;
    }
    if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        PME->extendSelection(-1);
    }
    else
    {
        PME->clearSelection();
        PME->moveRow(-1);
    }
}

static void functDown()
{
    if (PME->fillOpen)
    {
        return;
    }
    if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        PME->extendSelection(1);
    }
    else
    {
        PME->clearSelection();
        PME->moveRow(1);
    }
}

static void functLeft()
{
    if (PME->fillOpen)
    {
        PME->selectFillParam(PME->fillParam > 0 ? PME->fillParam - 1 : 0);
    }
    else if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        PME->selectWholeLane();
    }
    else
    {
        PME->moveColumn(-1);
    }
}

static void functRight()
{
    if (PME->fillOpen)
    {
        PME->selectFillParam(PME->fillParam + 1);
    }
    else if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        PME->selectWholeLane();
    }
    else
    {
        PME->moveColumn(1);
    }
}

static void functDelete()
{
    if (!PME->fillOpen)
    {
        PME->clearCells();
    }
}

static void functColumnButton(uint8_t button)
{
    if (PME->fillOpen)
    {
        return;
    }
    switch (button)
    {
        case interfaceButtonNote: PME->selectColumn(cPlayModeEditor::kColumnStep); break;
        case interfaceButtonInstr: PME->selectColumn(cPlayModeEditor::kColumnFx); break;
        case interfaceButtonVol: PME->selectColumn(cPlayModeEditor::kColumnValue); break;
        default: break;
    }
}

// [Fx]: the pattern editor's FX list while held; releasing it applies the FX.
static void functFxButton(uint8_t state)
{
    if (state == buttonPress)
    {
        PME->openFxPopup();
    }
    else if (state == buttonRelease)
    {
        PME->closeFxPopup();
    }
}

// [Copy] copies the selection (or the cursor's step); [Shift]+[Copy] pastes.
static void functCopy(uint8_t state)
{
    if (state != buttonPress || PME->fillOpen)
    {
        return;
    }
    if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        PME->pasteRows();
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
        PME->changeFillValue(value);
    }
    else if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        PME->clearSelection();
        PME->moveRow(value > 0 ? 16 : -16);
    }
    else
    {
        PME->changeValue(value);
    }
}

static void functSwitchModule(uint8_t button)
{
    // Performance mode kept running for this screen: let it decide whether
    // it can go on in the module the button opens.
    performanceMode.leaveToModule(button);
    PME->eventFunct(eventSwitchModule, PME, &button, 0);
}

static void functActionButton(uint8_t button, uint8_t state)
{
    if (state != buttonPress)
    {
        return;
    }
    if (PME->fillOpen)
    {
        switch (button)
        {
            case interfaceButton0:
            case interfaceButton1:
            case interfaceButton2: PME->selectFillParam(button - interfaceButton0); break;
            case interfaceButton6: PME->closeFill(); break;
            case interfaceButton7: PME->applyFill(); break;
            default: break;
        }
        return;
    }
    switch (button)
    {
        case interfaceButton0: PME->selectColumn(cPlayModeEditor::kColumnStep); break;
        case interfaceButton1: PME->selectColumn(cPlayModeEditor::kColumnFx); break;
        case interfaceButton2: PME->selectColumn(cPlayModeEditor::kColumnValue); break;
        case interfaceButton3: PME->selectColumn(cPlayModeEditor::kColumnChance); break;
        case interfaceButton4: PME->openFill(); break;
        case interfaceButton5: PME->resetLane(); break;
        case interfaceButton6: PME->revert(); break;
        case interfaceButton7: PME->save(); break;
        default: break;
    }
}

static void functPads(uint8_t pad, uint8_t state, int16_t velo)
{
    (void)velo;
    if (state == buttonPress && !PME->fillOpen)
    {
        PME->setStepFromPad(pad);
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
