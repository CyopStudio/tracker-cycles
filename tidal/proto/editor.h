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
};
const char* actionName(Action a);
bool actionFromName(const std::string& s, Action& out);

enum class SlotState { Empty, Edited, Playing, Error };

struct MenuItem {
    std::string label;
    std::shared_ptr<tc::Program> result;  // program after applying this item
    uint16_t cursor;                      // cursor in `result`
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
    void snapshot();
    void commit(std::unique_ptr<tc::Program> p, uint16_t cursor);  // checks, sets state
    bool recheck(tc::Program& p, std::string& err) const;
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
                         bool mini) const;
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
    std::shared_ptr<tc::Program> tryReplace(const std::string& code, bool mini, uint16_t& newNode) const;
    std::shared_ptr<tc::Program> tryWrapApp(const std::string& head, uint16_t& newNode) const;
    std::shared_ptr<tc::Program> tryWrapOp(const char* op, const std::string& rhs, bool xOnLeft,
                                           uint16_t& newNode) const;
    std::string renderNode(const tc::Program& p, uint16_t n) const;
    bool isTransparent(const tc::Program& p, uint16_t n) const;

    std::unique_ptr<tc::Program> prog_;
    std::vector<std::unique_ptr<tc::Program>> undo_;
    int slot_ = 0;
    uint16_t cursor_ = 0;
    bool menuOpen_ = false;
    std::string menuTitle_;
    std::vector<MenuItem> menu_;
    int menuSel_ = 0;
    std::string message_;
    bool messageError_ = false;
    SlotState state_[tc::kSlots];
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
