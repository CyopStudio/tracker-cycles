// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// See mini.h.
#include "mini.h"

#include "cold.h"

TC_COLD_BEGIN
namespace tc {

namespace {
constexpr int kLastWindow = kPadCols * kPadRows - MiniInput::kWindow;  // 40: the rest pad is its last key

MiniTarget pressOf(Control c, bool shift) {
    MiniTarget t;
    t.kind = MiniTarget::Press;
    t.ctl = c;
    t.shift = shift;
    return t;
}

int clampWindow(int w, int last = kLastWindow) { return w > last ? last : w < 0 ? 0 : w; }

// the loaded instruments in slot order (RC3 notes: the soft keys listed 8
// consecutive slots, empty ones too): how many, and the slot of the k-th
int loadedCount(const Editor& ed) {
    int n = 0;
    for (int s = 0; s < kInstrumentSlots; s++)
        if (ed.instrument(s)[0]) n++;
    return n;
}
int loadedSlot(const Editor& ed, int k) {
    for (int s = 0; s < kInstrumentSlots; s++)
        if (ed.instrument(s)[0] && k-- == 0) return s;
    return -1;
}

bool isScreen(MiniKey k) { return k >= MiniKey::Screen0 && k <= MiniKey::Screen7; }

// the Mini keys the Original has too, by name
Control sameControl(MiniKey k) {
    switch (k) {
        case MiniKey::Note: return Control::Note;
        case MiniKey::Instr: return Control::Instr;
        case MiniKey::Fx1: return Control::Fx1;
        case MiniKey::Fx2: return Control::Fx2;
        case MiniKey::Play: return Control::Play;
        case MiniKey::Copy: return Control::Copy;
        case MiniKey::Insert: return Control::Insert;
        case MiniKey::Up: return Control::Up;
        case MiniKey::Delete: return Control::Delete;
        case MiniKey::Rec: return Control::Rec;
        case MiniKey::Shift: return Control::Shift;
        case MiniKey::Left: return Control::Left;
        case MiniKey::Down: return Control::Down;
        case MiniKey::Right: return Control::Right;
        default:
            if (isScreen(k)) return (Control)((int)Control::Screen0 + (int)k - (int)MiniKey::Screen0);
            return Control::Count;
    }
}
}  // namespace

Control MiniInput::screenControl(const Editor& ed, int i, bool shift) const {
    const Control screen = (Control)((int)Control::Screen0 + i);
    if (i < 0 || i >= kWindow || overlay(ed)) return screen;
    if (armed(ed)) return padCtl(armBank_ * kPadCols + i);  // the new block's dN
    const Control slot = padCtl(bank_ * kPadCols + i);  // d1-d8 / d9-d16 (slotPad's layout)
    if (ed.playHeld()) return slot;
    if (!noteEntry_) return screen;
    if (shift) return slot;
    if (ed.padInstruments()) {
        const int s = loadedSlot(ed, insWin_ + i);
        return s < 0 ? Control::Count : padCtl(s);  // past the last one: a blank key
    }
    if (ed.padBool()) {  // 0 / 1 on Screen 1 / 2, a rest on Screen 8
        if (i == 0 || i == 1) return padCtl(i);
        return i == kWindow - 1 ? padCtl(kRestPad) : Control::Count;
    }
    if (ed.padNumeric()) return padCtl(numWin_ + i);
    return slot;
}

bool MiniInput::moveWindow(const Editor& ed, MiniKey k) {
    const bool vertical = k == MiniKey::Up || k == MiniKey::Down;
    const int dir = (k == MiniKey::Right || k == MiniKey::Up) ? 1 : -1;
    if (ed.playHeld() || !(ed.padNumeric() || ed.padInstruments())) {
        if (!vertical) return false;
        bank_ = k == MiniKey::Down ? 1 : 0;  // the bank below: the MIDI slots
        return true;
    }
    if (ed.padInstruments()) {
        const int n = loadedCount(ed);
        insWin_ = clampWindow(insWin_ + dir * (vertical ? kWindow : 1), n > kWindow ? n - kWindow : 0);
    }
    else numWin_ = clampWindow(numWin_ + dir * (vertical ? kPadCols : 1));
    return true;
}

MiniTarget MiniInput::map(const Editor& ed, MiniKey k, bool shift) {
    switch (k) {
        case MiniKey::Fn1: return pressOf(Control::Pattern, shift);
        case MiniKey::Fn2: return pressOf(Control::Song, shift);
        case MiniKey::Fn3: return pressOf(Control::File, shift);
        case MiniKey::Fn4: return pressOf(Control::Config, shift);
        case MiniKey::Menu:
            if (shift) return pressOf(Control::SmpLoad, true);  // the Tidal folders
            {
                MiniTarget t;
                t.kind = MiniTarget::ModuleMenu;
                return t;
            }
        case MiniKey::Middle:
            if (overlay(ed) || ed.playHeld()) return pressOf(Control::Screen7, false);  // OK / unmute all
            if (shift) return pressOf(Control::Master, true);                         // the status view
            return pressOf(Control::Note, true);                                      // the chord menu
        case MiniKey::Note:
            if (shift && !overlay(ed)) {
                noteEntry_ = !noteEntry_;
                return {};
            }
            return pressOf(Control::Note, shift);
        case MiniKey::Insert:
            if (!overlay(ed) && ed.insertAddsBlock()) {  // a dN: Screen 1-8 become its slots
                armBank_ = shift ? 1 : 0;
                return pressOf(Control::Insert, false);
            }
            return pressOf(Control::Insert, shift);
        case MiniKey::Up:
        case MiniKey::Down:
        case MiniKey::Left:
        case MiniKey::Right:
            if (!overlay(ed) && (noteEntry_ || ed.playHeld()) && !shift && moveWindow(ed, k)) return {};
            if (ed.playHeld()) return {};  // nothing else while Fn1 is held
            return pressOf(sameControl(k), shift);
        default: break;
    }
    if (isScreen(k)) {
        const Control c = screenControl(ed, (int)k - (int)MiniKey::Screen0, shift);
        if (c == Control::Count) return {};  // a blank key
        return pressOf(c, screenShift(ed, shift));
    }
    const Control c = sameControl(k);
    if (c == Control::Count) return {};
    return pressOf(c, shift);
}

MiniTarget MiniInput::press(const Editor& ed, MiniKey k, bool shift) {
    if (k >= MiniKey::Count) return {};
    const MiniTarget t = map(ed, k, shift);
    down_[(int)k] = t;
    return t;
}

MiniTarget MiniInput::release(MiniKey k) {
    if (k >= MiniKey::Count) return {};
    const MiniTarget t = down_[(int)k];
    down_[(int)k] = MiniTarget{};
    return t;
}

}  // namespace tc
TC_COLD_END
