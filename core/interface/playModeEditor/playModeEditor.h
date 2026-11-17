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

    uint8_t lane = 0; // 0-19 (C1-C20)
    uint8_t row = 0;  // 0-127
    uint8_t column = kColumnStep;
    uint8_t firstLane = 0; // leftmost visible lane

    int16_t lastPlayhead = -1;
    bool gridDirty = true;

    char titleText[64];

    hControl grid = -1;
    hControl label[8];
    hControl bgLabel = -1;
};

extern cPlayModeEditor playModeEditor;

#endif /* INTERFACE_PLAYMODEEDITOR_H_ */
