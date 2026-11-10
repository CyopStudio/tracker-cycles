#include "hardware.h"

#include <stdio.h>
#include <string.h>

#include "keyboard.h"
#include "tree.h"

namespace proto {

namespace {
struct CtlInfo {
    const char* name;
    const char* id;
    const char* fw;
};
const CtlInfo kInfo[] = {
    {"Screen 1", "s1", "interfaceButton0"},        {"Screen 2", "s2", "interfaceButton1"},
    {"Screen 3", "s3", "interfaceButton2"},        {"Screen 4", "s4", "interfaceButton3"},
    {"Screen 5", "s5", "interfaceButton4"},        {"Screen 6", "s6", "interfaceButton5"},
    {"Screen 7", "s7", "interfaceButton6"},        {"Screen 8", "s8", "interfaceButton7"},
    {"Params", "params", "interfaceButtonParams"}, {"Perform", "perform", "interfaceButtonPerformance"},
    {"File", "file", "interfaceButtonFile"},       {"Config", "config", "interfaceButtonConfig"},
    {"Master", "master", "interfaceButtonMaster"}, {"Smp Load", "smpload", "interfaceButtonSampleLoad"},
    {"Smp Play", "smpplay", "interfaceButtonSamplePlay"}, {"Smp Edit", "smpedit", "interfaceButtonSampleEdit"},
    {"Smp Rec", "smprec", "interfaceButtonSampleRec"},    {"Song", "song", "interfaceButtonSong"},
    {"Note", "note", "interfaceButtonNote"},       {"Instrument", "instr", "interfaceButtonInstr"},
    {"FX1", "fx1", "interfaceButtonVol (FX1)"},          {"FX2", "fx2", "interfaceButtonFx (FX2)"},
    {"Pattern", "pattern", "interfaceButtonPattern"}, {"Play", "play", "interfaceButtonPlay"},
    {"Copy", "copy", "interfaceButtonCopy"},       {"Insert", "insert", "interfaceButtonInsert"},
    {"Up", "up", "interfaceButtonUp"},             {"Delete", "delete", "interfaceButtonDelete"},
    {"Rec", "rec", "interfaceButtonRec"},          {"Shift", "shift", "interfaceButtonShift"},
    {"Left", "left", "interfaceButtonLeft"},       {"Down", "down", "interfaceButtonDown"},
    {"Right", "right", "interfaceButtonRight"},    {"Jog +", "jog+", "interfacePot0 (+)"},
    {"Jog -", "jog-", "interfacePot0 (-)"},
};
static_assert(sizeof kInfo / sizeof kInfo[0] == (size_t)Ctl::Pad0, "kInfo must cover every button");

char gPadName[48][8], gPadId[48][8];
bool gPadInit = false;
void initPads() {
    if (gPadInit) return;
    for (int i = 0; i < 48; i++) {
        snprintf(gPadName[i], sizeof gPadName[i], "Pad %d", i + 1);
        snprintf(gPadId[i], sizeof gPadId[i], "pad%d", i + 1);
    }
    gPadInit = true;
}

Command act(Action a, const char* label) {
    Command c;
    c.type = Command::Act;
    c.action = a;
    c.label = label;
    return c;
}
}  // namespace

const char* ctlName(Ctl c) {
    if (isPad(c)) return initPads(), gPadName[padIndex(c)];
    return c < Ctl::Pad0 ? kInfo[(int)c].name : "?";
}
const char* ctlId(Ctl c) {
    if (isPad(c)) return initPads(), gPadId[padIndex(c)];
    return c < Ctl::Pad0 ? kInfo[(int)c].id : "?";
}
const char* ctlFwName(Ctl c) {
    if (isPad(c)) return "interfacePad";
    return c < Ctl::Pad0 ? kInfo[(int)c].fw : "?";
}
bool ctlFromId(const std::string& s, Ctl& out) {
    for (int i = 0; i < kCtlCount; i++)
        if (s == ctlId((Ctl)i)) {
            out = (Ctl)i;
            return true;
        }
    return false;
}

// Pads depend on context. On a number / note / rest in a numeric pattern
// they enter values (-12 top left .. 34, then a rest pad bottom right; 0
// starts the second row, so for n/note it's a keyboard from c4). Elsewhere, and
// always with Shift, the first 8 pads of the top row select d1..d8 (sample
// tracks) and the first 8 of the second row d9..d16 (MIDI). In an s /
// sound pattern they are the 48 instrument slots (no rest pad there).
Ctl slotPad(int s) {
    if (s < 0 || s >= tc::kSlots) return Ctl::Count;
    int row = s / tc::kSampleSlots, col = s % tc::kSampleSlots;
    return padCtl(row * kPadCols + col);
}

Command controlCommand(const Editor& ed, Ctl c, bool shift) {
    const bool menu = ed.menuOpen();
    if (ed.keyboardOpen()) {
        // the Tracker's naming keyboard (keyboard.h): pads are keys, arrows
        // move the lit key, Screen 1 / Insert type it, Delete = Backspace,
        // jog moves the text cursor; Screen 2 clears, 7 cancels, 8 is OK
        if (isPad(c)) {
            Command k;
            k.type = Command::Key;
            k.key = kbd::kPadKey[padIndex(c)];
            const char ch = ed.kbdCaps() ? kbd::kBig[k.key] : kbd::kSmall[k.key];
            k.label = k.key == kbd::kKeyBackspace ? "Bksp" : k.key == kbd::kKeyCaps ? "Caps"
                    : k.key == kbd::kKeySpace     ? "Space" : std::string(1, ch);
            return k;
        }
        switch (c) {
            case Ctl::Left: return act(Action::Left, "key left");
            case Ctl::Right: return act(Action::Right, "key right");
            case Ctl::Up: return act(Action::Up, "key up");
            case Ctl::Down: return act(Action::Down, "key down");
            case Ctl::JogUp: return act(Action::WheelUp, "cursor >");
            case Ctl::JogDown: return act(Action::WheelDown, "cursor <");
            case Ctl::Screen0:
            case Ctl::Insert: return act(Action::KbdType, "Enter key");
            case Ctl::Screen1: return act(Action::KbdClear, "Clear");
            case Ctl::Delete: return act(Action::Delete, "Backspace");
            case Ctl::Screen6: return act(Action::KbdCancel, "Cancel");
            case Ctl::Screen7: return act(Action::KbdOk, "OK");
            default: return {};
        }
    }
    if (ed.playHeld()) {
        // Play held (v2): slot pads mute / unmute (Shift+Play: solo), Delete cancels
        if (isPad(c)) {
            int i = padIndex(c), row = i / kPadCols, col = i % kPadCols;
            if (row < 2 && col < tc::kSampleSlots) {
                Command k;
                k.type = Command::Mute;
                k.slot = row * tc::kSampleSlots + col;
                k.label = "mute d" + std::to_string(k.slot + 1);
                return k;
            }
            return {};
        }
        if (c == Ctl::Delete) return act(Action::PlayCancel, "cancel");
        return {};
    }
    if (isPad(c)) {
        int i = padIndex(c), row = i / kPadCols, col = i % kPadCols;
        if (!shift && ed.padInstruments()) {
            // instrument mode (s / sound patterns): pad N = instrument slot N
            Command k;
            k.type = Command::Instrument;
            k.instrument = i;
            const auto& ins = ed.instruments();
            k.label = i < (int)ins.size() ? ins[i] : "";
            return k;
        }
        if (!shift && ed.padNumeric()) {
            Command k;
            k.type = Command::Number;
            k.rest = i == kRestPad;
            k.number = padNumber(i);
            k.label = k.rest ? "~" : std::to_string(k.number);
            return k;
        }
        if (row < 2 && col < tc::kSampleSlots) {
            Command k;
            k.type = Command::Slot;
            k.slot = row * tc::kSampleSlots + col;
            k.label = "d" + std::to_string(k.slot + 1);
            return k;
        }
        return {};
    }
    if (menu) {
        // Menus: jog or arrows move, Play / Screen 8 apply, Screen 1 closes.
        switch (c) {
            case Ctl::JogUp: return shift ? act(Action::WheelUpFast, "+8 items") : act(Action::WheelUp, "next item");
            case Ctl::JogDown:
                return shift ? act(Action::WheelDownFast, "-8 items") : act(Action::WheelDown, "prev item");
            case Ctl::Up: return act(Action::Up, "prev item");
            case Ctl::Left: return act(Action::Left, "prev item");
            case Ctl::Down: return act(Action::Down, "next item");
            case Ctl::Right: return act(Action::Right, "next item");
            case Ctl::Play: return act(Action::Confirm, "OK");
            case Ctl::Pattern: return act(Action::Confirm, "OK");
            case Ctl::Screen7: return act(Action::Confirm, "OK");
            case Ctl::Screen0: return act(Action::Cancel, "Cancel");
            default: return {};
        }
    }
    switch (c) {
        case Ctl::Left: return act(Action::Left, "prev sibling");
        case Ctl::Right: return act(Action::Right, "next sibling");
        case Ctl::Up: return act(Action::Up, "parent");
        case Ctl::Down: return act(Action::Down, "first child");
        case Ctl::JogUp: return shift ? act(Action::WheelUpFast, "value ++") : act(Action::WheelUp, "value +");
        case Ctl::JogDown: return shift ? act(Action::WheelDownFast, "value --") : act(Action::WheelDown, "value -");
        // Play acts as a modifier (press / release, see press()); the label
        // says what releasing it alone does
        case Ctl::Play: {
            Command k;  // no action: press() / release() handle it
            k.label = shift ? "solo / hush" : "mute / unmute all";
            return k;
        }
        case Ctl::Insert: return shift ? act(Action::Home, "Home") : act(Action::Insert, "Insert");
        case Ctl::Delete: return shift ? act(Action::Unwrap, "Unwrap") : act(Action::Delete, "Delete");
        case Ctl::Copy: return shift ? act(Action::Paste, "Paste") : act(Action::Copy, "Copy");
        case Ctl::Note: return act(Action::JumpNotes, "n pattern");
        case Ctl::Instr: return act(Action::JumpSamples, "s pattern");
        case Ctl::Fx1: return act(Action::MenuModifiers, "modifiers");
        case Ctl::Fx2: return act(Action::MenuFx, "FX");
        case Ctl::Pattern: return act(Action::Evaluate, ed.onCps() ? "Send tempo" : "Evaluate");
        case Ctl::Params: return act(Action::EditComment, "comment");
        // Song: launch menu; Shift+Song: setcps (the Tracker's tempo is on
        // its Song screen); Master stays for the master settings
        case Ctl::Song:
            if (shift) return act(Action::JumpCps, ed.onCps() ? "back" : "setcps");
            return act(Action::MenuLaunch, "launch");
        // Screen 1-8: the structure menus $ # [] <> {} () | |+; with Shift
        // the old Replace / Wrap / Unwrap / Undo + context quick-buttons
        // (docs/tidal_ui_v2.md)
        case Ctl::Screen0:
        case Ctl::Screen1:
        case Ctl::Screen2:
        case Ctl::Screen3:
        case Ctl::Screen4:
        case Ctl::Screen5:
        case Ctl::Screen6:
        case Ctl::Screen7:
            if (!shift) {
                static const char* const kLabel[8] = {"$", "#", "[ ]", "< >", "{ }", "( )", "|", "|+"};
                const int q = (int)c - (int)Ctl::Screen0;
                return act((Action)((int)Action::Struct1 + q), kLabel[q]);
            }
            break;
        default: break;
    }
    switch (c) {
        case Ctl::Screen0: return act(Action::Replace, "Replace");
        case Ctl::Screen1: return act(Action::Wrap, "Wrap");
        case Ctl::Screen2: return act(Action::Unwrap, "Unwrap");
        case Ctl::Screen3: return act(Action::Undo, "Undo");
        case Ctl::Screen4:
        case Ctl::Screen5:
        case Ctl::Screen6:
        case Ctl::Screen7: {
            int q = (int)c - (int)Ctl::Screen4;
            std::string l = ed.quickLabel(q);
            if (l.empty()) return {};
            return act((Action)((int)Action::Quick1 + q), l.c_str());
        }
        case Ctl::File: return act(Action::Save, "Save");
        case Ctl::Config: return act(Action::ToggleTrackerMode, "Trk mode");
        default: return {};
    }
}

bool controlFor(const Editor& ed, Action a, Ctl& c, bool& shift) {
    if (a == Action::NextSlot || a == Action::PrevSlot) {
        int s = (ed.slot() + (a == Action::NextSlot ? 1 : tc::kSlots - 1)) % tc::kSlots;
        c = slotPad(s);
        shift = true;
        return true;
    }
    for (int sh = 0; sh < 2; sh++)
        for (int i = 0; i < (int)Ctl::Pad0; i++) {
            Command k = controlCommand(ed, (Ctl)i, sh);
            if (k.type == Command::Act && k.action == a) {
                c = (Ctl)i;
                shift = sh;
                return true;
            }
        }
    return false;
}

bool press(Editor& ed, Ctl c, bool shift) {
    if (c == Ctl::Play && !ed.menuOpen() && !ed.playHeld()) {
        ed.playDown(shift);
        return true;
    }
    Command k = controlCommand(ed, c, shift);
    switch (k.type) {
        case Command::Act: ed.act(k.action); return true;
        case Command::Slot: ed.gotoSlot(k.slot); return true;
        case Command::Number: ed.enterNumber(k.number, k.rest); return true;
        case Command::Instrument: ed.enterInstrument(k.instrument); return true;
        case Command::Mute: ed.mutePad(k.slot); return true;
        case Command::Key: ed.kbdKey(k.key); return true;
        default: return false;
    }
}

void release(Editor& ed, Ctl c) {
    if (c == Ctl::Play) ed.playUp();
}

bool typeChar(Editor& ed, char ch) {
    if (!ed.keyboardOpen()) return false;
    bool caps = false;
    const int key = kbd::keyFor(ch, caps);
    if (key < 0) return false;
    const bool letter = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
    const bool flip = letter && caps != ed.kbdCaps();
    if (flip) press(ed, padCtl(kbd::kKeyPad[kbd::kKeyCaps]), false);
    press(ed, padCtl(kbd::kKeyPad[key]), false);
    if (flip) press(ed, padCtl(kbd::kKeyPad[kbd::kKeyCaps]), false);
    return true;
}

}  // namespace proto
