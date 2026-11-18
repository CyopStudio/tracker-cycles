#ifndef INTERFACE_PLAYMODEEDITOR_H_
#define INTERFACE_PLAYMODEEDITOR_H_

// Play mode editor (prototype): the 20 custom play modes C1-C20 as a
// 20-lane x 128-row grid, with four columns per lane: the step to play, an
// FX, its value and its chance. Opened with Shift+Pattern in Performance
// mode, which keeps running, so edits are heard straight away. Save writes
// playmodes.txt (see docs/user_play_modes.md).

#include "core/modulesBase.h"

class cPlayModeEditor : public ModuleBase
{
  public:
    enum Column : uint8_t
    {
        kColumnStep,
        kColumnFx,
        kColumnValue,
        kColumnChance,
        kColumnCount
    };

    static constexpr uint8_t kLaneCount = 20;
    static constexpr uint8_t kRowCount = 128;
    static constexpr uint8_t kVisibleLanes = 4;

    // Sets the state below once, at boot (it is not zeroed by start-up).
    static void initExternalRamState();

    virtual void update();
    virtual void start(uint32_t options);
    virtual void stop();

    virtual void initDisplayControls();
    virtual void destroyDisplayControls() {}

    void setDefaultScreenFunct();
    void showDefaultScreen();

    // Grid
    void refreshGrid();
    void refreshTitle();
    void moveRow(int delta);
    void moveColumn(int delta);
    void moveLane(int delta);
    void selectColumn(uint8_t column);
    void changeValue(int16_t delta);
    void clearCell();
    void setStepFromPad(uint8_t pad);

    // Actions
    void save();
    void revert();
    void resetLane();
    void showResult(const char* line1, const char* line2 = nullptr);

    // State. The editor is a single object, so its state is static and kept in
    // external RAM (defined in gPlayModeEdit.cpp, set in initExternalRamState()),
    // leaving only ModuleBase's few bytes in internal RAM.
    static uint8_t lane;      // 0-19 (C1-C20)
    static uint8_t row;       // 0-127
    static uint8_t column;    // Column
    static uint8_t firstLane; // leftmost visible lane

    static int16_t lastPlayhead;
    static bool gridDirty;
    static bool patternViewSaved; // see savePatternView()

    static char titleText[64];

    static hControl grid;
    static hControl label[8];
    static hControl bgLabel;

    // The grid shares the Pattern editor's text buffer
    // (cPatternEditor::trackerPattern); the two are never shown together.
    // Its cell text is rebuilt on every refresh, but the fields after it hold
    // the Pattern editor's cursor and selection: they are saved on entry and
    // put back on exit.
    static void savePatternView();
    static void restorePatternView();
};

extern cPlayModeEditor playModeEditor;

#endif /* INTERFACE_PLAYMODEEDITOR_H_ */
