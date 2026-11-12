#include "controls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "keyboard.h"
#include "../core/tree.h"

namespace tc {

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
static_assert(sizeof kInfo / sizeof kInfo[0] == (size_t)Control::Pad0, "kInfo must cover every button");

// pad names are formatted on demand (no 48-entry tables in RAM): the
// result is valid until the next few calls (callers use it at once)
const char* padText(const char* fmt, int i) {
    static char buf[4][8];
    static int next = 0;
    char* b = buf[next++ & 3];
    snprintf(b, sizeof buf[0], fmt, i + 1);
    return b;
}

Command act(Action a, const char* label) {
    Command c;
    c.type = Command::Act;
    c.action = a;
    c.label = label;
    return c;
}
}  // namespace

const char* ctlName(Control c) {
    if (isPad(c)) return padText("Pad %d", padIndex(c));
    return c < Control::Pad0 ? kInfo[(int)c].name : "?";
}
const char* ctlId(Control c) {
    if (isPad(c)) return padText("pad%d", padIndex(c));
    return c < Control::Pad0 ? kInfo[(int)c].id : "?";
}
const char* ctlFwName(Control c) {
    if (isPad(c)) return "interfacePad";
    return c < Control::Pad0 ? kInfo[(int)c].fw : "?";
}
bool ctlFromId(const char* s, Control& out) {
    if (!strncmp(s, "pad", 3) && s[3] >= '1' && s[3] <= '9') {
        const int n = atoi(s + 3);
        if (n < 1 || n > kPadCols * kPadRows) return false;
        out = padCtl(n - 1);
        return true;
    }
    for (int i = 0; i < (int)Control::Pad0; i++)
        if (!strcmp(s, ctlId((Control)i))) {
            out = (Control)i;
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
Control slotPad(int s) {
    if (s < 0 || s >= tc::kSlots) return Control::Count;
    int row = s / tc::kSampleSlots, col = s % tc::kSampleSlots;
    return padCtl(row * kPadCols + col);
}

Command controlCommand(const Editor& ed, Control c, bool shift) {
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
                    : k.key == kbd::kKeySpace     ? "Space" : Str<24>(&ch, 1);
            return k;
        }
        switch (c) {
            case Control::Left: return act(Action::Left, "key left");
            case Control::Right: return act(Action::Right, "key right");
            case Control::Up: return act(Action::Up, "key up");
            case Control::Down: return act(Action::Down, "key down");
            case Control::JogUp: return act(Action::WheelUp, "cursor >");
            case Control::JogDown: return act(Action::WheelDown, "cursor <");
            case Control::Screen0:
            case Control::Insert: return act(Action::KbdType, "Enter key");
            case Control::Screen1: return act(Action::KbdClear, "Clear");
            case Control::Delete: return act(Action::Delete, "Backspace");
            case Control::Screen6: return act(Action::KbdCancel, "Cancel");
            case Control::Screen7: return act(Action::KbdOk, "OK");
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
                k.label.printf("mute d%d", k.slot + 1);
                return k;
            }
            return {};
        }
        if (c == Control::Delete) return act(Action::PlayCancel, "cancel");  // Pattern held
        if (c == Control::Screen7) return act(Action::UnmuteAll, "unmute all");
        return {};
    }
    if (isPad(c)) {
        int i = padIndex(c), row = i / kPadCols, col = i % kPadCols;
        if (!shift && ed.padInstruments()) {
            // instrument mode (s / sound patterns): pad N = instrument slot N
            Command k;
            k.type = Command::Instrument;
            k.instrument = i;
            k.label = ed.instrument(i);
            return k;
        }
        if (!shift && ed.padNumeric()) {
            Command k;
            k.type = Command::Number;
            k.rest = i == kRestPad;
            k.number = padNumber(i);
            if (k.rest) k.label = "~";
            else k.label.printf("%d", k.number);
            return k;
        }
        if (row < 2 && col < tc::kSampleSlots) {
            Command k;
            k.type = Command::Slot;
            k.slot = row * tc::kSampleSlots + col;
            k.label.printf("d%d", k.slot + 1);
            return k;
        }
        return {};
    }
    if (menu) {
        // Menus: jog or arrows move, Play / Screen 8 apply, Screen 1 closes.
        switch (c) {
            case Control::JogUp: return shift ? act(Action::WheelUpFast, "+8 items") : act(Action::WheelUp, "next item");
            case Control::JogDown:
                return shift ? act(Action::WheelDownFast, "-8 items") : act(Action::WheelDown, "prev item");
            case Control::Up: return act(Action::Up, "prev item");
            case Control::Left: return act(Action::Left, "prev item");
            case Control::Down: return act(Action::Down, "next item");
            case Control::Right: return act(Action::Right, "next item");
            case Control::Play: return act(Action::Confirm, "OK");
            case Control::Pattern: return act(Action::Confirm, "OK");
            case Control::Screen7: return act(Action::Confirm, "OK");
            case Control::Screen0: return act(Action::Cancel, "Cancel");
            default: return {};
        }
    }
    switch (c) {
        case Control::Left:
            if (shift && ed.canSelect()) return act(Action::SelLeft, "select <");
            return act(Action::Left, ed.cursorMode() ? "prev token" : "prev sibling");
        case Control::Right:
            if (shift && ed.canSelect()) return act(Action::SelRight, "select >");
            return act(Action::Right, ed.cursorMode() ? "next token" : "next sibling");
        case Control::Up:
            return shift ? act(Action::BlockUp, "block up") : act(Action::Up, ed.cursorMode() ? "line up" : "parent");
        case Control::Down:
            return shift ? act(Action::BlockDown, "block down")
                         : act(Action::Down, ed.cursorMode() ? "line down" : "first child");
        // Rec: cursor mode <-> tree mode (UI v3 D)
        case Control::Rec: return act(Action::ToggleCursorMode, ed.cursorMode() ? "tree mode" : "cursor mode");
        case Control::JogUp: return shift ? act(Action::WheelUpFast, "value ++") : act(Action::WheelUp, "value +");
        case Control::JogDown: return shift ? act(Action::WheelDownFast, "value --") : act(Action::WheelDown, "value -");
        // Play acts as a modifier (press / release, see press()); the label
        // says what releasing it alone does
        // Pattern is the mute modifier (press / release, see press()), Play
        // evaluates (RC14 user: swapped, Play on a block is "evaluate");
        // the label says what releasing Pattern alone does
        case Control::Pattern: {
            Command k;  // no action: press() / release() handle it
            k.label = shift ? "solo / hush" : "mute / unmute all";
            return k;
        }
        case Control::Insert: return shift ? act(Action::Home, "Home") : act(Action::Insert, "Insert");
        case Control::Delete:
            if (shift) return ed.cursorMode() ? act(Action::Backspace, "Backspace") : act(Action::Unwrap, "Unwrap");
            return act(Action::Delete, "Delete");
        case Control::Copy: return shift ? act(Action::Paste, "Paste") : act(Action::Copy, "Copy");
        case Control::Note: return shift ? act(Action::MenuChords, "chord") : act(Action::JumpNotes, "n pattern");
        case Control::Instr: return act(Action::JumpSamples, "s pattern");
        case Control::Fx1: return act(Action::MenuModifiers, "modifiers");
        case Control::Fx2: return act(Action::MenuFx, "FX");
        case Control::Play:
            if (shift && !ed.onCps()) return act(Action::SilenceSlot, "Silence dN");
            return act(Action::Evaluate, ed.onCps() ? "Send tempo" : "Evaluate");
        // Params stays free (the instrument parameters); comments are on
        // Shift+Config
        // Song: launch menu; Shift+Song: setcps (the Tracker's tempo is on
        // its Song screen); Master stays for the master settings
        // Sample Loader opens the stock loader (the device leaves the editor);
        // Shift+Sample Loader: the Tidal sample folders
        case Control::SmpLoad:
            if (shift) return act(Action::MenuFolders, "Tidal folders");
            return {};
        case Control::Song:
            if (shift) return act(Action::JumpCps, ed.onCps() ? "back" : "setcps");
            return act(Action::MenuLaunch, "launch");
        // Screen 1-8: the structure menus $ # [] <> {} () | |+; with Shift
        // the old Replace / Wrap / Unwrap / Undo + context quick-buttons
        // (docs/tidal_ui_v2.md)
        case Control::Screen0:
        case Control::Screen1:
        case Control::Screen2:
        case Control::Screen3:
        case Control::Screen4:
        case Control::Screen5:
        case Control::Screen6:
        case Control::Screen7:
            if (!shift) {
                static const char* const kLabel[8] = {"$", "#", "[ ]", "< >", "{ }", "( )", "|", "|+"};
                const int q = (int)c - (int)Control::Screen0;
                return act((Action)((int)Action::Struct1 + q), kLabel[q]);
            }
            break;
        default: break;
    }
    switch (c) {
        // Replace / Wrap / Unwrap retired (UI v3: by context instead: the jog
        // and menus replace, Insert / the Screen menus wrap, Delete unwraps)
        case Control::Screen0: return act(Action::QuickGain, "# gain 0.9");
        case Control::Screen1: return act(Action::QuickRoom, "# room 0.3");
        case Control::Screen2: return act(Action::QuickDelay, "# delay 0.3");
        // RC14 (user): Shift+Screen 4 # shape, 6 ! (repeat the step), 7 @
        // (hold it longer), 8 Undo; the context quick buttons (Quick2-4)
        // keep their actions but no key
        case Control::Screen3: return act(Action::QuickShape, "# shape 0.3");
        case Control::Screen4: return act(Action::LineBreak, "<CR>");  // a line break (UI notes, Editor14)
        case Control::Screen5: return act(Action::StepRepeat, "! repeat");
        case Control::Screen6: return act(Action::StepWeight, "@ longer");
        case Control::Screen7: return act(Action::Undo, "Undo");
        case Control::File: return act(Action::Save, "Save");
        case Control::Config: return shift ? act(Action::EditComment, "comment") : act(Action::ToggleTrackerMode, "Trk mode");
        default: return {};
    }
}

bool controlFor(const Editor& ed, Action a, Control& c, bool& shift) {
    if (a == Action::NextSlot || a == Action::PrevSlot) {
        int s = (ed.slot() + (a == Action::NextSlot ? 1 : tc::kSlots - 1)) % tc::kSlots;
        c = slotPad(s);
        shift = true;
        return true;
    }
    for (int sh = 0; sh < 2; sh++)
        for (int i = 0; i < (int)Control::Pad0; i++) {
            Command k = controlCommand(ed, (Control)i, sh);
            if (k.type == Command::Act && k.action == a) {
                c = (Control)i;
                shift = sh;
                return true;
            }
        }
    return false;
}

bool press(Editor& ed, Control c, bool shift) {
    if (c == Control::Pattern && !ed.menuOpen() && !ed.playHeld()) {  // the mute modifier
        ed.playDown(shift);
        return true;
    }
    Command k = controlCommand(ed, c, shift);
    switch (k.type) {
        case Command::Act: ed.act(k.action); return true;
        case Command::Slot: ed.gotoSlot(k.slot); return true;
        case Command::Number:
            ed.enterNumber(k.number, k.rest);
            if (!k.rest) ed.entryPadDown(padIndex(c));  // held: the next pads make a chord
            return true;
        case Command::Instrument:
            ed.enterInstrument(k.instrument);
            ed.entryPadDown(padIndex(c));
            return true;
        case Command::Mute: ed.mutePad(k.slot); return true;
        case Command::Key: ed.kbdKey(k.key); return true;
        default: return false;
    }
}

void release(Editor& ed, Control c) {
    if (c == Control::Pattern) ed.playUp();
    if (isPad(c)) ed.entryPadUp(padIndex(c));
}

PadLight padLight(const Editor& ed, int pad, bool shift) {
    const Command k = controlCommand(ed, padCtl(pad), shift);
    switch (k.type) {
        case Command::Slot:
        case Command::Mute: {
            const SlotState s = ed.slotState(k.slot);
            if (s == SlotState::Error) return PadLight::Blink;
            if (k.type == Command::Slot && k.slot == ed.slot()) return PadLight::Front;
            if (ed.muted(k.slot)) return PadLight::Dim;
            if (s == SlotState::Playing) return PadLight::Back;
            return s == SlotState::Edited ? PadLight::Dim : PadLight::Off;
        }
        case Command::Number: {
            int v;
            if (k.rest ? ed.cursorRest() : (ed.cursorInt(v) && v == k.number)) return PadLight::Front;
            return k.number == 0 && !k.rest ? PadLight::Dim : PadLight::Off;
        }
        case Command::Instrument:
            if (ed.cursorInstrument() == k.instrument) return PadLight::Front;
            return k.label.empty() ? PadLight::Off : PadLight::Dim;
        case Command::Key: return k.key == ed.kbdSel() ? PadLight::Front : PadLight::Dim;
        default: return PadLight::Off;
    }
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

}  // namespace tc
