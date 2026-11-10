// Structured editor model for the prototype. Operates on the shared code
// tree (tidal/core); everything typed, every edit re-checked.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "backend.h"
#include "tree.h"

namespace proto {

enum class Action {
    Left, Right, Up, Down,             // navigate siblings / parent / child
    WheelUp, WheelDown,                // change value (or menu selection)
    WheelUpFast, WheelDownFast,        // bigger steps / sample index
    Replace, Wrap, Insert, Delete, Unwrap,
    Undo, Evaluate, Hush, NextSlot, PrevSlot,
    Confirm, Cancel, Save, ToggleTrackerMode,
    Copy, Paste, Home,                 // clipboard, jump to the block's top
    MenuNotes, MenuSamples,            // filtered replace menus
    MenuFunctions, MenuControls,       // filtered wrap menus
    Quick1, Quick2, Quick3, Quick4,    // context buttons: brackets / operators
    MenuLaunch,                        // dN / jumpMod / jumpIn for the block
    // v2 (docs/tidal_ui_v2.md): Screen 1-8 structure menus, FX1 / FX2 lists
    Struct1, Struct2, Struct3, Struct4, Struct5, Struct6, Struct7, Struct8,
    MenuModifiers, MenuFx,
    JumpNotes, JumpSamples,            // Note / Instrument: the block's n / s pattern
    PlayCancel,                        // Delete while Play is held
    JumpCps,                           // Shift+Song: to `setcps` (created if missing) and back
    EditComment,                       // Params: the block's comment on the keyboard screen
    KbdType, KbdOk, KbdCancel, KbdClear,  // keyboard screen (Screen 1 / 8 / 7 / 2)
};
const char* actionName(Action a);
bool actionFromName(const std::string& s, Action& out);

enum class SlotState { Empty, Edited, Playing, Error };

// One edit, as data: what a menu item does when it is applied. The editor
// edits its Program in place (docs/tidal_device_editor.md 4b): a menu item is
// tried (applied, type-checked, rolled back) when the menu is built and
// applied again when chosen; the rollback restores the free list, so the
// second time gives the same node ids.
enum class EditKind : uint8_t {
    Replace,     // target := code (mini: mini-notation)
    WrapApp,     // target := code $ target (code = function head)
    WrapList,    // target := code [target, copy]
    WrapOp,      // target := target op code (left) / code op target
    InsertGap,   // a one-step mini code into the "..." gap (seq, pos)
    InsertList,  // an expression into the list gap (seq, pos)
    Launch,      // block target: launch mode / cycles
};
struct EditSpec {
    EditKind kind = EditKind::Replace;
    bool mini = false;
    bool left = false;
    uint16_t target = 0;
    uint16_t seq = 0;
    int pos = 0;
    int launch = 0;
    uint32_t cycles = 0;
    std::string code;
    std::string op;
};

struct MenuItem {
    std::string label;
    EditSpec spec;    // what choosing it does
    uint16_t cursor;  // the cursor afterwards (node ids repeat, see EditSpec)
};

class Editor {
public:
    Editor();
    bool load(const std::string& path, std::string& err);
    bool loadText(const std::string& text, std::string& err);
    bool save(const std::string& path, std::string& err);
    std::string fileText() const;

    void act(Action a);
    void selectMenu(int i);   // jump to a menu entry (scripted use)
    void gotoSlot(int s);     // select d(s+1) (Shift + pad on the Tracker)
    // Pad number entry: active anywhere in a numeric pattern (Time / Double /
    // Int / Note: a step, a group, the quotes) or on a number argument.
    bool padNumeric() const;
    bool cursorInt(int& v) const;  // current value, if a whole number / note
    bool cursorRest() const;
    // Replace the value at the cursor (rest: ~), then select the next step's
    // value, or append a copy at the end of the sequence (Delete removes it).
    void enterNumber(int v, bool rest = false);
    // Instrument mode: in an s / sound pattern the 48 pads are instrument
    // slots (samples_), entered the same way.
    bool padInstruments() const;
    int cursorInstrument() const;  // slot of the sample at the cursor, or -1
    void enterInstrument(int slot);
    const std::vector<std::string>& instruments() const { return samples_; }

    // state for the view
    const tc::Program& prog() const { return *prog_; }
    int slot() const { return slot_; }
    uint16_t cursor() const { return cursor_; }
    bool menuOpen() const { return menuOpen_; }
    const std::string& menuTitle() const { return menuTitle_; }
    const std::vector<MenuItem>& menu() const { return menu_; }
    int menuSel() const { return menuSel_; }
    const std::string& message() const { return message_; }
    bool messageIsError() const { return messageError_; }
    SlotState slotState(int s) const { return state_[s]; }
    // Play is a modifier (v2, docs/tidal_ui_v2.md): pads pressed while it is
    // held mute / unmute their dN (Shift+Play: solo); released with no pad
    // pressed: unmute all (Shift+Play: hush); Delete while held cancels.
    void playDown(bool shift);
    void playUp();
    bool playHeld() const { return playHeld_; }
    void mutePad(int slot);
    bool muted(int s) const { return muted_[s]; }
    // Gap cursor (v2 step 5): between the steps of a "..." sequence; 0 = on
    // a node (cursor()). gapPos 0 = before the first step, count = after the last.
    uint16_t gapSeq() const { return gapSeq_; }
    // Several blocks per dN (v2 step 6): the block dN last evaluated (0 = none)
    uint16_t playingBlock(int s) const { return playing_[s]; }
    // Per-event highlighting: steps of playing blocks lit while their
    // events sound (backends that report them: engine). pollHighlights()
    // drains the backend; true if what is lit may have changed.
    struct LitStep {
        int slot;
        uint16_t node;
        double on, off;  // steadySeconds()
    };
    bool pollHighlights(double now);
    const std::vector<LitStep>& litSteps() const { return lit_; }
    // Headless: each slot's playing block with every step lit since the
    // last call marked as [[step]] (smoke tests); "" if none.
    std::string litReport();
    int gapPos() const { return gapPos_; }
    // `setcps` (Shift+Song): the cursor is in its expression; jog / Replace
    // edit it, Pattern sends it, Shift+Song or a slot pad goes back.
    bool onCps() const { return cps_; }
    bool cpsSent() const;  // the setcps shown is the one last sent
    // Keyboard screen (the Tracker's naming keyboard, keyboard.h): editing
    // the last comment line above the current block.
    bool keyboardOpen() const { return kbdOpen_; }
    const std::string& kbdText() const { return kbdText_; }
    int kbdPos() const { return kbdPos_; }     // text cursor
    int kbdSel() const { return kbdSel_; }     // highlighted key
    bool kbdCaps() const { return kbdCaps_; }
    const std::string& kbdTitle() const { return kbdTitle_; }
    void kbdKey(int key);                      // a key's pad pressed
    bool trackerMode() const { return trackerMode_; }
    std::string cursorInfo() const;
    // Context buttons (Screen 5..8): inside "..." brackets [ ] < > { }%4 (3,8),
    // on a pattern # <last control>, |+, |*, <~. Empty label: nothing here.
    std::string quickLabel(int i) const;
    std::string evalText(int slot, std::string* err = nullptr) const;  // what gets sent

    void setBackend(Backend* b) { backend_ = b; }
    Backend* backend() const { return backend_; }
    void setSamples(const std::vector<std::string>& s) { samples_ = s; }
    std::string path;

private:
    void info(const std::string& m) { message_ = m; messageError_ = false; }
    void error(const std::string& m) { message_ = m; messageError_ = true; }
    // In-place edits: beginEdit() opens a checkpoint around the current block
    // and setcps; the edit then changes *prog_ directly; endEdit() type-checks
    // and keeps it as one undo step (a packed checkpoint), or rolls it back
    // and reports the error. abortEdit() rolls back silently.
    bool beginEdit();
    bool endEdit(uint16_t cursor, bool check = true);
    void abortEdit();
    uint16_t applySpec(const EditSpec& s);           // inside an open edit; 0 = doesn't apply
    bool trySpec(const EditSpec& s, uint16_t& nn);   // apply, check, roll back
    bool openSpec(const EditSpec& s, uint16_t& nn);  // apply, check, leave open (endEdit later)
    bool doSpec(const EditSpec& s, uint16_t& nn);    // apply, check, keep
    bool recheck(tc::Program& p, std::string& err) const;
    bool trackerBefore_ = true;  // Tracker mode held before the open edit
    uint16_t exprRoot() const;
    void enterSlot(int s);

    void navigate(Action a);
    void wheel(int dir, bool fast);
    void openReplace();
    enum class WrapMode { All, Functions, Controls, Dollar, Modifiers, Fx, Ops };
    void openStruct(int i);   // Screen 1-8: $ # [] <> {} () | |+
    void openMini(int k);     // [] <> {} () | around the step(s)
    void jumpControl(bool notes);  // to the block's n / s pattern (adds it if missing)
    void openWrap(WrapMode mode);
    void openNotes();
    void openLaunch();
    std::string launchHead(int slot) const;
    void openSamples();
    void addReplaceItems(std::vector<MenuItem>& items, const std::string& label, const std::string& code,
                         bool mini);
    void ensureBlock();
    void doCopy();
    void quick(int i);
    void doPaste();
    void doDelete();
    void enterStep(const std::string& miniCode, const std::string& exprCode, const std::string& label);
    void deleteNode();  // remove / hole the cursor node (Delete outside "...")
    void doUnwrap();
    void evaluate();
    void openMenu(const std::string& title, std::vector<MenuItem> items);
    void wheelFn(int dir);

    // candidate helpers: build a copy with the cursor node replaced by parsed
    // code (expression or mini-notation). Returns null if it doesn't check.
    // (filling `spec` with the edit when it fits)
    bool tryReplace(const std::string& code, bool mini, uint16_t& newNode, EditSpec* spec = nullptr);
    bool tryWrapApp(const std::string& head, uint16_t& newNode, EditSpec* spec = nullptr);
    // x -> `fn [x, x]` (stack / cat / fastcat), cursor on the copy
    bool tryWrapList(const char* fn, uint16_t& newNode, EditSpec* spec = nullptr);
    bool tryWrapOp(const char* op, const std::string& rhs, bool xOnLeft, uint16_t& newNode,
                   EditSpec* spec = nullptr);
    std::string renderNode(const tc::Program& p, uint16_t n) const;
    bool isTransparent(const tc::Program& p, uint16_t n) const;

    std::unique_ptr<tc::Program> prog_;
    std::unique_ptr<tc::Checkpoint> cp_;         // the open edit's checkpoint
    // undo steps, newest last: 'C' + a packed checkpoint, or 'M' + block
    // index (4 B) + the comment before (comments aren't in checkpoints)
    std::vector<std::vector<uint8_t>> undo_;
    int slot_ = 0;
    uint16_t cursor_ = 0;
    bool menuOpen_ = false;
    std::string menuTitle_;
    std::vector<MenuItem> menu_;
    int menuSel_ = 0;
    std::string message_;
    bool messageError_ = false;
    SlotState state_[tc::kSlots];
    bool muted_[tc::kSlots] = {};
    uint16_t playing_[tc::kSlots] = {};
    struct Evaluated {
        uint32_t stmt;                 // stmtHash of what was sent
        std::vector<uint16_t> atoms;   // its steps, atomOrder() of the block's expression
    };
    std::vector<Evaluated> evaluated_[tc::kSlots];  // newest last, a few kept
    std::vector<LitStep> lit_;
    bool cps_ = false;
    void jumpCps();
    bool kbdOpen_ = false, kbdCaps_ = false;
    std::string kbdText_, kbdTitle_;
    int kbdPos_ = 0, kbdSel_ = 0, kbdBlock_ = -1, kbdMax_ = 0;
    void openCommentKeyboard();
    void kbdAction(Action a);
    std::vector<LitStep> litSeen_;
    bool newBlockArmed_ = false;  // Insert at a block's top: the next slot pad adds a block
    void selectBlock(uint16_t b);  // make b its slot's selected block, cursor on its top
    void newBlockAbove(int slot);
    uint16_t gapSeq_ = 0;
    int gapPos_ = 0;
    void leaveGap();
    // a copy of the program with the one-step mini `code` inserted at the
    // gap (nullptr if it doesn't parse / type-check)
    bool tryInsertAtGap(const std::string& code, uint16_t& newStep, EditSpec* spec = nullptr);
    void insertAtGap(const std::string& code, const std::string& label);
    void deleteAtGap();
    // gaps in a Haskell list (`stack [a, b]`): elements, not steps
    bool listGap() const;
    bool linkAtGap(tc::Program& p, uint16_t node, uint16_t seq, int pos) const;  // put node into a gap
    bool tryInsertListAt(const std::string& code, uint16_t& newEl, EditSpec* spec = nullptr);
    void insertListCopy();
    bool playHeld_ = false, playShift_ = false, playUsed_ = false;
    void setMuted(int s, bool on);
    bool trackerMode_ = true;
    Backend* backend_ = nullptr;
    std::vector<std::string> samples_;
    std::string lastCps_;  // last setcps sent to the backend
    std::string clip_;     // copied code (rendered text)
    bool clipMini_ = false;  // clip_ is mini-notation (a step inside "...")
    std::string lastControl_ = "gain 1";  // for the # context button
    uint16_t pending_ = 0;  // step appended by pad entry; Delete removes it
};

}  // namespace proto
