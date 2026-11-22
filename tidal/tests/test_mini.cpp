// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// The Tracker Mini key layer (tidal/editor/mini.h): note entry on the
// screen keys, the Fn keys, the mute modifier, releases after a window move.
#include <stdio.h>
#include <string.h>

#include <string>

#include "controls.h"
#include "editor.h"
#include "mini.h"

using namespace tc;

#define EXPECT(cond, ...)                          \
    do {                                           \
        if (cond) {                                \
            pass++;                                \
        } else {                                   \
            fail++;                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                   \
            printf("\n");                          \
        }                                          \
    } while (0)

namespace {

struct MuteOutput : EditorOutput {
    std::string calls;
    bool evaluate(const Program&, int slot, Label&) override {
        calls += "eval " + std::to_string(slot + 1) + ";";
        return true;
    }
    void silence(int) override {}
    void hush() override { calls += "hush;"; }
    void mute(int slot, bool on) override { calls += (on ? "mute " : "unmute ") + std::to_string(slot + 1) + ";"; }
    void unmuteAll() override { calls += "unmuteAll;"; }
    void setCps(const Program&) override {}
    bool save(const Program&, Label&) override { return true; }
};

Program gMiniProg;
Checkpoint gMiniCp;
uint8_t gMiniUndo[64 * 1024];

std::string fileOf(const Editor& ed) {
    static char buf[16384];
    ed.fileText(buf, sizeof buf);
    return buf;
}

// a Mini key press + release, as the device would run it
void tap(Editor& ed, MiniInput& m, MiniKey k, bool shift = false) {
    MiniTarget t = m.press(ed, k, shift);
    if (t.kind == MiniTarget::Press) press(ed, t.ctl, t.shift);
    t = m.release(k);
    if (t.kind == MiniTarget::Press) release(ed, t.ctl);
}

}  // namespace

void testMiniInput(int& pass, int& fail) {
    static Editor ed(gMiniProg, gMiniCp, gMiniUndo, (int)sizeof gMiniUndo);
    MuteOutput out;
    ed.setOutput(&out);
    const char* text = "d1 $ n \"0 3\" # s \"bd sn\"\n\nd2 $ s \"hh*4\"\n";
    Label err;
    EXPECT(ed.loadText(text, (int)strlen(text), err), "load: %s", err.c_str());
    MiniInput m;

    // the same keys as the Original, by name
    MiniTarget t = m.press(ed, MiniKey::Screen0, false);
    EXPECT(t.kind == MiniTarget::Press && t.ctl == Control::Screen0, "Screen 1 is Screen 1");
    m.release(MiniKey::Screen0);
    t = m.press(ed, MiniKey::Fn1, true);
    EXPECT(t.kind == MiniTarget::Press && t.ctl == Control::Pattern && t.shift, "Shift+Fn1 = Shift+Pattern");
    m.release(MiniKey::Fn1);
    t = m.press(ed, MiniKey::Fn3, true);
    EXPECT(t.ctl == Control::File && t.shift, "Shift+Fn3 = Shift+File (save)");
    m.release(MiniKey::Fn3);
    t = m.press(ed, MiniKey::Menu, false);
    EXPECT(t.kind == MiniTarget::ModuleMenu, "Menu = the module menu");
    m.release(MiniKey::Menu);
    t = m.press(ed, MiniKey::Middle, false);
    EXPECT(t.ctl == Control::Note && t.shift, "Middle = the chord menu (Shift+Note)");
    m.release(MiniKey::Middle);

    // Shift+Note: note entry on the screen keys
    ed.gotoSlot(0);
    ed.act(Action::JumpNotes);
    EXPECT(ed.padNumeric(), "on the n pattern");
    tap(ed, m, MiniKey::Note, true);
    EXPECT(m.noteEntry(), "note entry on");
    EXPECT(m.screenControl(ed, 0, false) == padCtl(kPadCols), "Screen 1 = the 0 pad");
    tap(ed, m, MiniKey::Screen4);  // 4
    EXPECT(fileOf(ed).find("n \"4 3\"") != std::string::npos, "Screen 5 entered 4: %s", fileOf(ed).c_str());
    tap(ed, m, MiniKey::Up);  // an octave up: 12 .. 19
    EXPECT(m.numberWindow() == 2 * kPadCols, "window an octave up (%d)", m.numberWindow());
    ed.act(Action::Right);  // the next step
    tap(ed, m, MiniKey::Screen0);
    EXPECT(fileOf(ed).find("n \"4 12\"") != std::string::npos, "Screen 1 entered 12: %s", fileOf(ed).c_str());
    for (int i = 0; i < 10; i++) tap(ed, m, MiniKey::Up);
    EXPECT(m.numberWindow() == kPadCols * kPadRows - MiniInput::kWindow, "window clamped (%d)", m.numberWindow());
    EXPECT(m.screenControl(ed, 7, false) == padCtl(kRestPad), "the rest pad on Screen 8");
    tap(ed, m, MiniKey::Down);
    tap(ed, m, MiniKey::Left);
    EXPECT(m.numberWindow() == kPadCols * kPadRows - MiniInput::kWindow - kPadCols - 1, "down / left (%d)",
           m.numberWindow());

    // a held screen key releases the pad it pressed, after the window moved
    t = m.press(ed, MiniKey::Screen2, false);
    EXPECT(t.kind == MiniTarget::Press && isPad(t.ctl), "a pad");
    const Control held = t.ctl;
    m.press(ed, MiniKey::Up, false);
    m.release(MiniKey::Up);
    t = m.release(MiniKey::Screen2);
    EXPECT(t.ctl == held, "released the pad it pressed");

    // with Shift: the slots
    EXPECT(m.screenControl(ed, 1, true) == slotPad(1), "Shift+Screen 2 = d2");

    // instruments in an s pattern: the loaded ones, 8 a page (RC3 notes:
    // empty slots were listed too)
    const int loaded[] = {0, 3, 9, 20, 30, 31, 32, 33, 34, 35, 36, 37};
    for (int s = 0; s < kInstrumentSlots; s++) ed.setInstrument(s, "");  // the host's default kit off
    for (int s : loaded) ed.setInstrument(s, ("i" + std::to_string(s)).c_str());
    ed.act(Action::JumpSamples);
    EXPECT(ed.padInstruments(), "on the s pattern");
    EXPECT(m.screenControl(ed, 0, false) == padCtl(0), "Screen 1 = instrument slot 1");
    EXPECT(m.screenControl(ed, 1, false) == padCtl(3), "Screen 2 = the next loaded slot (4)");
    EXPECT(m.screenControl(ed, 3, false) == padCtl(20), "Screen 4 = slot 21");
    tap(ed, m, MiniKey::Up);
    EXPECT(m.instrumentWindow() == 4, "page down clamps to the last 8 of 12 (%d)", m.instrumentWindow());
    EXPECT(m.screenControl(ed, 0, false) == padCtl(30) && m.screenControl(ed, 7, false) == padCtl(37),
           "the last 8 loaded");
    tap(ed, m, MiniKey::Left);
    EXPECT(m.screenControl(ed, 0, false) == padCtl(20), "Left: one back (slot 21)");
    tap(ed, m, MiniKey::Screen1);  // slot 31 = i30
    EXPECT(fileOf(ed).find("i30") != std::string::npos, "Screen 2 entered i30: %s", fileOf(ed).c_str());
    // Instrument on a sample name: the sample menu (RC3 notes)
    ed.act(Action::JumpSamples);
    EXPECT(ed.cursorOnSample(), "on a sample name");
    EXPECT(!strcmp(controlCommand(ed, Control::Instr, false).label.c_str(), "samples"), "Instr label: %s",
           controlCommand(ed, Control::Instr, false).label.c_str());
    tap(ed, m, MiniKey::Instr);
    EXPECT(ed.menuOpen() && !strcmp(ed.menuTitle().c_str(), "samples"), "the sample menu (%s)",
           ed.menuTitle().c_str());
    ed.act(Action::Cancel);
    EXPECT(!ed.menuOpen(), "menu closed");
    for (int s : loaded) ed.setInstrument(s, "");
    EXPECT(m.screenControl(ed, 0, false) == Control::Count, "no instruments: blank keys");
    EXPECT(m.press(ed, MiniKey::Screen0, false).kind == MiniTarget::None, "a blank key does nothing");
    m.release(MiniKey::Screen0);

    // note entry off: the screen keys are the structure keys again
    tap(ed, m, MiniKey::Note, true);
    EXPECT(!m.noteEntry() && m.screenControl(ed, 0, false) == Control::Screen0, "note entry off");

    // Fn1 held: the screen keys mute, Down switches to d9-d16, Middle unmutes all
    out.calls.clear();
    t = m.press(ed, MiniKey::Fn1, false);
    press(ed, t.ctl, t.shift);
    EXPECT(ed.playHeld(), "Fn1 is the mute modifier");
    tap(ed, m, MiniKey::Screen1);
    tap(ed, m, MiniKey::Down);
    EXPECT(m.bank() == 1 && m.screenControl(ed, 0, false) == slotPad(8), "bank d9-d16");
    tap(ed, m, MiniKey::Up);
    tap(ed, m, MiniKey::Middle);
    t = m.release(MiniKey::Fn1);
    release(ed, t.ctl);
    EXPECT(!ed.playHeld(), "Fn1 released");
    EXPECT(out.calls == "mute 2;unmuteAll;", "mute calls: %s", out.calls.c_str());

    // Insert on a dN: Screen 1-8 are the slots for a new block (Shift+Insert: d9-d16)
    ed.gotoSlot(1);
    for (int i = 0; i < 6 && !ed.insertAddsBlock(); i++) ed.act(Action::Up);
    EXPECT(ed.insertAddsBlock(), "on d2's top");
    EXPECT(m.screenControl(ed, 2, false) == Control::Screen2, "not armed yet: Screen 3");
    tap(ed, m, MiniKey::Insert);
    EXPECT(ed.newBlockArmed(), "Insert armed a new block");
    EXPECT(m.screenControl(ed, 2, false) == slotPad(2), "Screen 3 = d3");
    EXPECT(!strcmp(controlCommand(ed, m.screenControl(ed, 2, false), m.screenShift(ed, false)).label.c_str(), "d3"),
           "its label");
    tap(ed, m, MiniKey::Insert, true);
    EXPECT(ed.newBlockArmed() && m.screenControl(ed, 0, false) == slotPad(8), "Shift+Insert: Screen 1 = d9");
    tap(ed, m, MiniKey::Insert);
    tap(ed, m, MiniKey::Screen2);
    EXPECT(!ed.newBlockArmed(), "used up");
    EXPECT(fileOf(ed).find("d3 silence\n\nd2 $") != std::string::npos, "a new d3 block above d2: %s",
           fileOf(ed).c_str());
    EXPECT(m.screenControl(ed, 2, false) == Control::Screen2, "Screen 3 again");
}
