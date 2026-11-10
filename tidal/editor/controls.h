// Original Tracker controls and what each one does in the editor.
//
// This is the button -> editor mapping the device editor (plan step 4d)
// will use. The prototype routes every keyboard shortcut through it, so a
// shortcut only works if some Tracker control does the same thing; the mock
// panel (panel.h) shows the mapping live.
//
// Control names follow core/interface/core/interfaceDefs.h
// (interfaceButton0..7 are the screen buttons, 8..32 the function buttons,
// interfacePot0 the jog wheel, 48 pads).
#pragma once
#include "editor.h"

namespace tc {

enum class Control : uint8_t {
    Screen0, Screen1, Screen2, Screen3, Screen4, Screen5, Screen6, Screen7,  // btn 0..7
    Params, Perform, File, Config, Master,                                  // btn 8..12
    SmpLoad, SmpPlay, SmpEdit, SmpRec, Song,                                // btn 13..17
    Note, Instr, Fx1, Fx2, Pattern,                                         // btn 18..22 (FX1 = interfaceButtonVol)
    Play, Copy, Insert, Up, Delete,                                         // btn 23..27
    Rec, Shift, Left, Down, Right,                                          // btn 28..32
    JogUp, JogDown,                                                         // pot 0
    Pad0,                                                                   // pads 0..47
    Count = Pad0 + 48,
};
constexpr int kCtlCount = (int)Control::Count;
constexpr int kPadCols = 12, kPadRows = 4;

inline Control padCtl(int i) { return (Control)((int)Control::Pad0 + i); }
inline bool isPad(Control c) { return c >= Control::Pad0 && c < Control::Count; }
inline int padIndex(Control c) { return (int)c - (int)Control::Pad0; }

const char* ctlName(Control c);    // panel label, e.g. "Smp Load", "Pad 13"
const char* ctlId(Control c);      // headless token, e.g. "smpload", "pad13"
const char* ctlFwName(Control c);  // firmware enum, e.g. "interfaceButtonSampleLoad"
bool ctlFromId(const char* s, Control& out);

struct Command {
    enum Type { None, Act, Slot, Number, Instrument, Mute, Key } type = None;
    Action action = Action::Cancel;
    int slot = -1;
    int number = 0;  // Type::Number: value a pad enters
    bool rest = false;  // Type::Number: the rest pad (~)
    int instrument = -1;  // Type::Instrument: slot 0..47
    int key = -1;         // Type::Key: keyboard screen key (keyboard.h)
    Str<24> label;  // short, for the panel / screen soft-key bar
};

// What pressing `c` (with Shift held or not) does right now.
Command controlCommand(const Editor& ed, Control c, bool shift);
// Pad that selects slot s (d1..d16) with Shift held, or Control::Count.
Control slotPad(int s);
// Value a pad enters in number mode: -12 top left, 0 at the second row;
// the bottom right pad enters a rest instead of 35.
inline int padNumber(int pad) { return pad - kPadCols; }
constexpr int kRestPad = kPadCols * kPadRows - 1;
// Find a control (+ shift) that performs `a` in the current context.
bool controlFor(const Editor& ed, Action a, Control& c, bool& shift);
// Runs the command; returns false if the control does nothing here.
bool press(Editor& ed, Control c, bool shift);
// Release of a control that acts on release (Play: the modifier, v2; pads:
// the end of a held chord). Every pad press needs its release.
void release(Editor& ed, Control c);
// Keyboard screen: press the pad(s) that type `c` (Caps on / off around
// it as needed). False if no key types it.
bool typeChar(Editor& ed, char c);

}  // namespace tc
