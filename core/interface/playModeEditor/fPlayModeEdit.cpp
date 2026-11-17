#include "playModeEditor/playModeEditor.h"

#include <cstdio>
#include <cstring>

#include "core/interfaceMenuPopup.h"
#include "core/interfacePopups.h"
#include "display.h"
#include "fileManager.h"
#include "mtPadBoard.h"
#include "mtPadsBacklight.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "performanceMode/performanceMode.h"
#include "userPlayModes.h"

cPlayModeEditor playModeEditor;
static cPlayModeEditor* PME = &playModeEditor;

extern strTrackerPattern playModeEditorGrid;

static void functPlayAction();
static void functUp();
static void functDown();
static void functLeft();
static void functRight();
static void functDelete();
static void functColumnButton(uint8_t button);
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

constexpr int kRollStride = Sequencer::strFxConsts::ROLL_PERIOD_MAX + 1;
constexpr uint8_t kGridRows = 15;
constexpr uint8_t kCursorGridRow = 7;

int clampInt(int value, int low, int high)
{
    return value < low ? low : (value > high ? high : value);
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
    showDefaultScreen();
    setDefaultScreenFunct();
}

void cPlayModeEditor::stop()
{
    moduleRefresh = 0;
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

    ip->setButtonObj(interfaceButtonNote, buttonPress, functColumnButton);
    ip->setButtonObj(interfaceButtonInstr, buttonPress, functColumnButton);
    ip->setButtonObj(interfaceButtonVol, buttonPress, functColumnButton);
    ip->setButtonObj(interfaceButtonFx, buttonPress, functColumnButton);

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

    strTrackerPattern& g = playModeEditorGrid;
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

    g.firstVisibleTrack = 0;
    g.actualTrack = lane - firstLane;
    g.actualStep = row;
    g.middleVisibleStep = row;
    g.patternLength = kRowCount;
    g.stepDevider = 4;
    g.selectState = 1;
    g.selectedParam = column;
    g.selectColor = 0;
    g.selectColumn = 0;
    g.popupMode = 0;
    g.showTrackNames = true;

    g.showPlayhead = lastPlayhead >= 0;
    g.playheadPosition = lastPlayhead >= 0 ? lastPlayhead : 0;
    g.playheadStartTrack = lane - firstLane;
    g.playheadEndTrack = lane - firstLane;

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
    row = (row + kRowCount + delta % kRowCount) % kRowCount;
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::moveColumn(int delta)
{
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
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::moveLane(int delta)
{
    lane = static_cast<uint8_t>(clampInt(lane + delta, 0, kLaneCount - 1));
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::selectColumn(uint8_t newColumn)
{
    column = newColumn < kColumnCount ? newColumn : uint8_t(kColumnStep);
    gridDirty = true;
}

void cPlayModeEditor::changeValue(int16_t delta)
{
    const userPlayModes::StepInfo info = userPlayModes::getStepInfo(lane, row);
    bool ok = true;

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
        showResult("The FX list is full.", "At most 64 different FX settings.");
    }
    gridDirty = true;
    refreshTitle();
}

void cPlayModeEditor::clearCell()
{
    bool ok = column == kColumnStep ? userPlayModes::setStepNumber(lane, row, row + 1)
                                    : userPlayModes::setStepFx(lane, row, 0, 0, 100);
    if (!ok)
    {
        showResult("The FX list is full.", "At most 64 different FX settings.");
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
    if (!userPlayModes::setStepNumber(lane, row, pad + 1))
    {
        showResult("The FX list is full.", "At most 64 different FX settings.");
        return;
    }
    moveRow(1);
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

static void functUp()
{
    PME->moveRow(PME->buttons->isButtonPressed(interfaceButtonShift) ? -16 : -1);
}

static void functDown()
{
    PME->moveRow(PME->buttons->isButtonPressed(interfaceButtonShift) ? 16 : 1);
}

static void functLeft()
{
    if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        PME->moveLane(-1);
    }
    else
    {
        PME->moveColumn(-1);
    }
}

static void functRight()
{
    if (PME->buttons->isButtonPressed(interfaceButtonShift))
    {
        PME->moveLane(1);
    }
    else
    {
        PME->moveColumn(1);
    }
}

static void functDelete()
{
    PME->clearCell();
}

static void functColumnButton(uint8_t button)
{
    switch (button)
    {
        case interfaceButtonNote: PME->selectColumn(cPlayModeEditor::kColumnStep); break;
        case interfaceButtonInstr: PME->selectColumn(cPlayModeEditor::kColumnFx); break;
        case interfaceButtonVol: PME->selectColumn(cPlayModeEditor::kColumnValue); break;
        case interfaceButtonFx: PME->selectColumn(cPlayModeEditor::kColumnChance); break;
        default: break;
    }
}

static void functEncoder(int16_t value)
{
    PME->changeValue(value);
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
    switch (button)
    {
        case interfaceButton0: PME->selectColumn(cPlayModeEditor::kColumnStep); break;
        case interfaceButton1: PME->selectColumn(cPlayModeEditor::kColumnFx); break;
        case interfaceButton2: PME->selectColumn(cPlayModeEditor::kColumnValue); break;
        case interfaceButton3: PME->selectColumn(cPlayModeEditor::kColumnChance); break;
        case interfaceButton5: PME->resetLane(); break;
        case interfaceButton6: PME->revert(); break;
        case interfaceButton7: PME->save(); break;
        default: break;
    }
}

static void functPads(uint8_t pad, uint8_t state, int16_t velo)
{
    (void)velo;
    if (state == buttonPress)
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
