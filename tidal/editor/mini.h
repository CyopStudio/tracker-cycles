// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Tracker Mini controls -> the Original Tracker controls the editor knows
// (controls.h). The Mini has no pads, no jog wheel and no Pattern / Song /
// File / Config / Master keys; this layer stands in for them:
//
//   Fn1 = Pattern (mute modifier; Shift: solo / hush)   Fn2 = Song (Shift: setcps)
//   Fn3 = File (Shift: save)                            Fn4 = Config (Shift: comment)
//   Menu = the stock module menu (the device handles it; Shift+Menu = Tidal folders)
//   Middle = OK in menus and the keyboard, else the chord menu (Shift: status)
//   +1 / -1 / +10 / -10 = the jog wheel (the stock firmware already sends them as pot steps)
//   Shift+Note = note entry on / off: Screen 1-8 are 8 pads of a window
//     numbers / notes: Left / Right move it 1, Up / Down an octave (12)
//     instruments (s patterns): Left / Right 1, Up / Down 8 slots
//     elsewhere, and with Shift: d1-d8 / d9-d16 (Up / Down switch the bank)
//   Fn1 held: Screen 1-8 mute / solo d1-d8 (Up / Down: d9-d16), Middle unmutes all
//
// Every other key is the Original's (Screen 1-8, Note, Instrument, FX1,
// FX2, Play, Rec, Copy, Insert, Delete, Shift, the arrows).
#pragma once
#include "controls.h"

namespace tc {

enum class MiniKey : uint8_t {
    Screen0, Screen1, Screen2, Screen3, Screen4, Screen5, Screen6, Screen7,
    Note, Instr, Fx1, Fx2,
    Play, Copy, Insert, Up, Delete,
    Rec, Shift, Left, Down, Right,
    Middle, Menu, Fn1, Fn2, Fn3, Fn4,
    Count,
};
constexpr int kMiniKeyCount = (int)MiniKey::Count;

// What a Mini key press does.
struct MiniTarget {
    enum Kind : uint8_t {
        None,        // nothing (or handled here: note entry, a window / bank move)
        Press,       // the Original control `ctl` with `shift`
        ModuleMenu,  // the stock module menu (the device's Menu handling)
    } kind = None;
    Control ctl = Control::Count;
    bool shift = false;
};

class MiniInput {
public:
    static constexpr int kWindow = 8;  // pads on the screen keys
    // A key went down / up. Every press needs its release (held pads make
    // chords, Fn1 is the mute modifier): release returns what that press
    // pressed, wherever the window has moved since.
    MiniTarget press(const Editor& ed, MiniKey k, bool shift);
    MiniTarget release(MiniKey k);
    // What screen key `i` presses now (for its soft key label), with Shift
    // held or not: a pad in note entry / while Fn1 is held, else Screen i.
    Control screenControl(const Editor& ed, int i, bool shift) const;

    bool noteEntry() const { return noteEntry_; }
    void setNoteEntry(bool on) { noteEntry_ = on; }
    int numberWindow() const { return numWin_; }    // first pad: value padNumber(it)
    int instrumentWindow() const { return insWin_; }  // first pad: instrument slot it
    int bank() const { return bank_; }               // 0: d1-d8, 1: d9-d16

private:
    bool overlay(const Editor& ed) const { return ed.menuOpen() || ed.keyboardOpen(); }
    bool moveWindow(const Editor& ed, MiniKey k);
    MiniTarget map(const Editor& ed, MiniKey k, bool shift);
    bool noteEntry_ = false;
    int numWin_ = kPadCols;  // 0 .. 7 (c4 .. g4 for n / note)
    int insWin_ = 0;
    int bank_ = 0;
    MiniTarget down_[kMiniKeyCount];
};

}  // namespace tc
