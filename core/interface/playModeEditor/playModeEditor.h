#ifndef INTERFACE_PLAYMODEEDITOR_H_
#define INTERFACE_PLAYMODEEDITOR_H_

// Play mode editor (prototype): the 20 custom play modes C1-C20 as a
// 20-lane x 128-row grid, with four columns per lane: the step to play, an
// FX, its value and its chance. Opened from Performance mode, which keeps
// running, so edits are heard straight away. Save writes playmodes.txt (see
// docs/user_play_modes.md). Its controls follow the Pattern editor's: Rec
// toggles edit mode, Fill opens the Pattern editor's fill popup, and so on.

#include "core/modulesBase.h"
#include "display.h"
#include "elapsedMillis.h"
#include "mtStructs.h"

// Storage for an object kept in __NOINIT(EXTERNAL_RAM) whose type has a
// constructor: the empty constructor runs no code before main(), when the
// external RAM is not set up yet (writing to it there hangs the Original at
// boot). The object is set up in cPlayModeEditor::initExternalRamState().
template<typename T>
union NoInit
{
    NoInit() {}
    T value;
};

class cPlayModeEditor : public ModuleBase
{
  public:
    // The grid's columns, as the Pattern editor's: Note = the step to play,
    // Fx1 = the FX and its value, Fx2 = the chance. The Instr column (1) is
    // left empty and skipped.
    enum Column : uint8_t
    {
        kColumnStep = 0,
        kColumnFx = 2,
        kColumnChance = 3,
        kColumnCount
    };

    // What a fill writes: the step, the FX (with its value) or the chance.
    enum FillKind : uint8_t
    {
        kFillStep,
        kFillFx,
        kFillChance,
        kFillKindCount
    };

    // Fill popup places (columns), as in the Pattern editor.
    enum FillPlace : uint8_t
    {
        kPlaceWhere,
        kPlaceDensity,
        kPlaceFxType,
        kPlaceType,
        kPlaceFrom,
        kPlaceTo,
        kPlaceCount
    };

    // Fill types: the Pattern editor's three, then sequences for Fill Steps
    // only. A sequence counts the rows it fills, or every row with Count = All
    // rows (wrapping 128 -> 1): Up/Down from From, Up/Down ending at To, or
    // From.. looping every To ("Loop") steps.
    enum FillType : uint8_t
    {
        kTypeConstant,
        kTypeFromTo,
        kTypeRandom,
        kTypeUpFrom,
        kTypeUpTo,
        kTypeDownFrom,
        kTypeDownTo,
        kTypeLoopUp,
        kTypeLoopDown,
        kTypeCount,
        kTypeCountBasic = kTypeUpFrom, // the types Fill Fx and Fill Chance have
    };
    static uint8_t fillTypeCount(uint8_t kind)
    {
        return kind == kFillStep ? uint8_t(kTypeCount) : uint8_t(kTypeCountBasic);
    }
    static bool fillTypeUsesFrom(int type) { return type != kTypeUpTo && type != kTypeDownTo; }
    static bool fillTypeUsesTo(int type)
    {
        return type == kTypeFromTo || type == kTypeRandom || type == kTypeUpTo ||
               type == kTypeDownTo || type >= kTypeLoopUp;
    }

    // The screen buttons that are held to change a value (as Pattern / Step jump).
    enum HeldPlace : int8_t
    {
        kHeldNone = -1,
        kHeldLane = 0,
        kHeldEditStep = 2,
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
    void refreshLabels();

    // Grid
    void refreshGrid();
    void refreshTitle();
    void moveRow(int delta);
    void moveLane(int delta);
    void setLane(uint8_t newLane);
    void selectColumn(uint8_t column);
    void toggleEditMode();
    // Moves the cursor down by the edit step (plus extra), like the Pattern editor.
    void moveCursorByStep(uint8_t extra = 0);
    void changeEditStep(int delta);
    void setHeldPlace(int8_t place);

    // Editing (the selection, or the cursor's step)
    void changeValue(int16_t delta);
    void clearCells();
    void setFromPad(uint8_t pad);
    void insertRow();
    void backspaceRow();

    // Selection: rows of the cursor's lane.
    void extendSelection(int delta);
    void selectWholeLane();
    void clearSelection();
    bool isMultiSelection() const;
    // The rows an action applies to: the selection, or the cursor's row.
    void actionRows(uint8_t& first, uint8_t& last) const;

    // Clipboard (one lane, up to 128 rows)
    void copyRows();
    // column: kColumnCount pastes all columns.
    void pasteRows(uint8_t column);

    // Undo: one level, which Redo swaps back.
    void storeUndo();
    void undo(bool redo);

    // Fill popup (the Pattern editor's, rebuilt from the same controls)
    void openFill();
    void closeFill();
    void showFillPopup();
    void refreshFillPopup();
    bool isFillPlaceValid(int place) const;
    void setFillPlace(int place);
    void changeFillPlace(int delta);
    void changeFillData(int16_t delta, bool rollOver = false);
    void setFillDataFromPad(uint8_t pad);
    void applyFill();

    // The Pattern editor's FX list, shown while [Fx1] is held
    void openFxPopup();
    void closeFxPopup();

    // Actions
    void save();
    void revert();
    void resetLane();
    void showResult(const char* line1, const char* line2 = nullptr);
    void showFxListFull();

    // State. The editor is a single object, so its state is static and kept in
    // external RAM (defined in gPlayModeEdit.cpp, set in initExternalRamState()),
    // leaving only ModuleBase's few bytes in internal RAM.
    static uint8_t lane;      // 0-19 (C1-C20)
    static uint8_t row;       // 0-127
    static uint8_t column;    // Column
    static uint8_t firstLane; // leftmost visible lane
    static bool editMode;     // Rec: red cursor; otherwise it follows the playhead

    static int16_t lastPlayhead;
    static bool gridDirty;
    static bool patternViewSaved; // see savePatternView()

    static bool selecting; // a selection from selectionAnchor to selectionEnd
    static uint8_t selectionAnchor;
    static uint8_t selectionEnd;
    static int8_t heldPlace; // HeldPlace

    static bool fxPopupOpen;
    static uint8_t fxButtonHeld;                // the button held for the FX list, or 0
    static NoInit<elapsedMillis> fxButtonTimer; // since the button was pressed

    static bool fillOpen;
    static uint8_t fillKind; // FillKind
    static int8_t fillPlace; // FillPlace
    struct FillData
    {
        int16_t where;
        int16_t type;
        int16_t from;
        int16_t to;
        int16_t fxType; // kFillFx: the FX written
        int16_t density[3];
        int16_t countAllRows; // Fill Steps sequences: count every row, not only those filled
    };
    static FillData fillData[kFillKindCount];
    static char fillText[3][12]; // density, from, to

    static char titleText[64];
    static char resultText[20]; // a formatted popup line (the popup keeps the pointer)
    static char laneText[6];
    static char editStepText[6];

    static hControl grid;
    static hControl label[8];
    static hControl bgLabel;
    static hControl frame;
    static hControl popupBg;
    static hControl popupTitle;
    static hControl listWhere;
    static hControl listFxType;
    static hControl listType;
    static hControl barDensity;
    static hControl barFrom;
    static hControl barTo;
    static hControl barCancel;
    static hControl barFill;
    static strFrameData frameData;
    static NoInit<strList> whereList;
    static NoInit<strList> typeList;
    static NoInit<strList> fxTypeList;
    static NoInit<strList> countList; // Fill Steps: the Count list, in the Fx Type column
    static const char* fxTypeNames[FX_COUNT];
    static uint8_t fxTypeCount;

    // The grid shares the Pattern editor's text buffer
    // (cPatternEditor::trackerPattern); the two are never shown together.
    // Its cell text is rebuilt on every refresh, but the fields after it hold
    // the Pattern editor's cursor and selection: they are saved on entry and
    // put back on exit.
    static void savePatternView();
    static void restorePatternView();

    // A lane's rows, as the clipboard and the undo buffer keep them.
    struct Rows
    {
        uint8_t stepNumber[kRowCount];
        uint8_t fxType[kRowCount];
        uint8_t chance[kRowCount];
        int16_t value[kRowCount];
    };
    struct Clipboard
    {
        uint8_t count;
        Rows rows;
    };
    struct Undo
    {
        uint8_t state; // 0 nothing, 1 undo available, 2 redo available
        uint8_t lane;
        bool userMode;     // the lane was user-defined
        uint32_t lastEdit; // the cell the jog wheel last edited, to group its turns
        Rows rows;
    };
    static Clipboard clipboard;
    static Undo undoBuffer;
};

extern cPlayModeEditor playModeEditor;

#endif /* INTERFACE_PLAYMODEEDITOR_H_ */
