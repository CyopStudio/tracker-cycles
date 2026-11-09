#include "hardware.h"

#include <stdio.h>
#include <string.h>

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

// Pads: the first 8 pads of the top row are d1..d8 (sample tracks), the
// first 8 of the second row d9..d16 (MIDI). The rest are free for now.
Ctl slotPad(int s) {
    if (s < 0 || s >= tc::kSlots) return Ctl::Count;
    int row = s / tc::kSampleSlots, col = s % tc::kSampleSlots;
    return padCtl(row * kPadCols + col);
}

Command controlCommand(const Editor& ed, Ctl c, bool shift) {
    const bool menu = ed.menuOpen();
    if (isPad(c)) {
        int i = padIndex(c), row = i / kPadCols, col = i % kPadCols;
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
        case Ctl::Play: return shift ? act(Action::Hush, "Hush") : act(Action::Evaluate, "Evaluate");
        case Ctl::Insert: return shift ? act(Action::Home, "Home") : act(Action::Insert, "Insert");
        case Ctl::Delete: return shift ? act(Action::Unwrap, "Unwrap") : act(Action::Delete, "Delete");
        case Ctl::Copy: return shift ? act(Action::Paste, "Paste") : act(Action::Copy, "Copy");
        case Ctl::Note: return act(Action::MenuNotes, "notes");
        case Ctl::Instr: return act(Action::MenuSamples, "samples");
        case Ctl::Fx1: return act(Action::MenuFunctions, "functions");
        case Ctl::Fx2: return act(Action::MenuControls, "# control");
        case Ctl::Screen0: return act(Action::Replace, "Replace");
        case Ctl::Screen1: return act(Action::Wrap, "Wrap");
        case Ctl::Screen2: return act(Action::Unwrap, "Unwrap");
        case Ctl::Screen3: return act(Action::Undo, "Undo");
        case Ctl::File: return act(Action::Save, "Save");
        case Ctl::Config: return act(Action::ToggleTrackerMode, "Trk mode");
        default: return {};
    }
}

bool controlFor(const Editor& ed, Action a, Ctl& c, bool& shift) {
    if (a == Action::NextSlot || a == Action::PrevSlot) {
        int s = (ed.slot() + (a == Action::NextSlot ? 1 : tc::kSlots - 1)) % tc::kSlots;
        c = slotPad(s);
        shift = false;
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
    Command k = controlCommand(ed, c, shift);
    switch (k.type) {
        case Command::Act: ed.act(k.action); return true;
        case Command::Slot: ed.gotoSlot(k.slot); return true;
        default: return false;
    }
}

}  // namespace proto
