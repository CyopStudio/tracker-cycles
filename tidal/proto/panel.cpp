#include "panel.h"

#include <stdio.h>

#include "tree.h"

namespace proto {

namespace pc {
constexpr uint32_t body = 0x0E0F12;
constexpr uint32_t bezel = 0x2A2D33;
constexpr uint32_t btn = 0x24272D;
constexpr uint32_t btnEdge = 0x3A3F48;
constexpr uint32_t btnLit = 0x55617A;
constexpr uint32_t name = 0x7D8592;
constexpr uint32_t label = 0xE6E8EC;
constexpr uint32_t unused = 0x4A4F58;
constexpr uint32_t key = 0x79B8FF;
constexpr uint32_t shift = 0xC792EA;
constexpr uint32_t accent = 0xFF9F1C;
constexpr uint32_t padOff = 0x1C1E22;
constexpr uint32_t padEmpty = 0x34383F;
constexpr uint32_t padEdited = 0x6B5420;
constexpr uint32_t padPlaying = 0x2E6B3A;
constexpr uint32_t padError = 0x7A2B2B;
constexpr uint32_t note = 0x4FD1C5, instr = 0xE6E29A, fx1 = 0xC88BE0, fx2 = 0x56CFE0, rec = 0xD9726A;
}  // namespace pc

// ---- layout (matches the Original Tracker's front panel) --------------------
// Screen top left with the 8 screen buttons under it and the 12x4 pads below.
// Right: three rows of five beside the screen, then the Play/Rec block,
// then the jog wheel at the bottom right.
constexpr int kSX = 30, kSY = 40;                  // screen origin
constexpr int kSbY = kSY + kH + 14, kSbW = 76, kSbH = 40;
constexpr int kRX = 866, kBW = 76, kBH = 66, kBStep = 82;  // right-hand button block
constexpr int kRowA = kSY, kRowB = kSY + 74, kRowC = kSY + 148;
constexpr int kRowD = 330, kRowE = 404;
constexpr int kPY = 600;                           // pads
constexpr int kPadW = 58, kPadH = 52, kPadStepX = 66, kPadStepY = 62;
constexpr int kJogX = kRX + 2 * kBStep + kBW / 2, kJogY = 720, kJogR = 110;

struct Rect {
    int x, y, w, h;
    bool in(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

static bool ctlRect(Ctl c, Rect& r) {
    int i = (int)c;
    if (c <= Ctl::Screen7) {
        r = {kSX + i * 100 + (100 - kSbW) / 2, kSbY, kSbW, kSbH};
        return true;
    }
    if (c >= Ctl::Params && c <= Ctl::Right) {
        int k = i - (int)Ctl::Params;  // 0..24, five rows of five
        static const int rows[5] = {kRowA, kRowB, kRowC, kRowD, kRowE};
        r = {kRX + (k % 5) * kBStep, rows[k / 5], kBW, kBH};
        return true;
    }
    if (c == Ctl::JogUp) {
        r = {kJogX - kJogR, kJogY - kJogR, 2 * kJogR, kJogR};
        return true;
    }
    if (c == Ctl::JogDown) {
        r = {kJogX - kJogR, kJogY, 2 * kJogR, kJogR};
        return true;
    }
    if (isPad(c)) {
        int p = padIndex(c);
        r = {kSX + (p % kPadCols) * kPadStepX, kPY + (p / kPadCols) * kPadStepY, kPadW, kPadH};
        return true;
    }
    return false;
}

bool panelHit(int x, int y, Ctl& out) {
    for (int i = 0; i < kCtlCount; i++) {
        Rect r;
        if (!ctlRect((Ctl)i, r) || !r.in(x, y)) continue;
        if ((Ctl)i == Ctl::JogUp || (Ctl)i == Ctl::JogDown) {
            int dx = x - kJogX, dy = y - kJogY;
            if (dx * dx + dy * dy > kJogR * kJogR) continue;
        }
        out = (Ctl)i;
        return true;
    }
    return false;
}

// ---- keyboard hints (keep in sync with sdl_window.cpp) ---------------------
static const char* fixedKey(Ctl c) {
    static const char* digits[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};
    static const char* sdigits[8] = {"S+1", "S+2", "S+3", "S+4", "S+5", "S+6", "S+7", "S+8"};
    static const char* fkeys[8] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8"};
    if (c <= Ctl::Screen7) return fkeys[(int)c];
    switch (c) {
        case Ctl::Left: return "Left";
        case Ctl::Right: return "Right";
        case Ctl::Up: return "Up";
        case Ctl::Down: return "Down";
        case Ctl::JogUp: return "+ / PgUp / wheel";
        case Ctl::JogDown: return "- / PgDn / wheel";
        case Ctl::Play: return "Space";
        case Ctl::Insert: return "i";
        case Ctl::Delete: return "x / Del";
        case Ctl::File: return "Ctrl+S";
        case Ctl::Config: return "t";
        case Ctl::Shift: return "Shift";
        case Ctl::Copy: return "Ctrl+C";
        case Ctl::Note: return "n";
        case Ctl::Instr: return "s";
        case Ctl::Fx1: return "f";
        case Ctl::Fx2: return "c";
        case Ctl::Pattern: return "l";
        default: break;
    }
    if (isPad(c)) {
        int p = padIndex(c);
        if (p < 8) return digits[p];
        if (p >= kPadCols && p < kPadCols + 8) return sdigits[p - kPadCols];
    }
    return nullptr;
}

static const char* actionKey(Action a) {
    switch (a) {
        case Action::Replace: return "r";
        case Action::Wrap: return "w";
        case Action::Unwrap: return "u";
        case Action::Undo: return "z";
        case Action::Hush: return "h";
        case Action::Confirm: return "Enter";
        case Action::Cancel: return "Esc";
        case Action::Paste: return "Ctrl+V";
        case Action::Home: return "Home";
        default: return nullptr;
    }
}

// Action-bound key shown on `c` only if the key would land on `c`.
static const char* intentKey(const Editor& ed, Ctl c, bool shift) {
    Command k = controlCommand(ed, c, shift);
    if (k.type != Command::Act) return nullptr;
    const char* key = actionKey(k.action);
    Ctl found;
    bool fs;
    if (!key || !controlFor(ed, k.action, found, fs) || found != c || fs != shift) return nullptr;
    return key;
}

std::string keyHint(const Editor& ed, Ctl c) {
    std::string s;
    if (const char* f = fixedKey(c)) s = f;
    if (const char* k = intentKey(ed, c, false)) s += (s.empty() ? "" : " / ") + std::string(k);
    return s;
}

static std::string shiftHint(const Editor& ed, Ctl c) {
    if (const char* k = intentKey(ed, c, true)) return k;
    const char* f = fixedKey(c);
    if (f && !isPad(c) && c != Ctl::Shift) return std::string("S+") + f;
    return "";
}

// ---- drawing -----------------------------------------------------------------
static void centered(Framebuffer& fb, const Rect& r, int y, const std::string& s, uint32_t c,
                     FontId f = FontId::Sans12) {
    int n = (int)s.size();
    while (n > 0 && fb.textWidth(s.c_str(), f, n) > r.w - 4) n--;
    int w = fb.textWidth(s.c_str(), f, n);
    fb.text(r.x + (r.w - w) / 2, y, s.c_str(), c, f, n);
}

static void drawButton(Framebuffer& fb, const Editor& ed, Ctl c, const PanelState& st) {
    Rect r;
    ctlRect(c, r);
    bool lit = st.lit == c || (c == Ctl::Shift && st.shift);
    fb.rect(r.x, r.y, r.w, r.h, lit ? pc::btnLit : pc::btn);
    fb.frame(r.x, r.y, r.w, r.h, lit ? pc::accent : pc::btnEdge);
    Command n = controlCommand(ed, c, false), s = controlCommand(ed, c, true);
    bool hasShift = s.type != Command::None && (s.action != n.action || s.type != n.type);
    if (c == Ctl::Shift) n.label = "modifier";
    bool showShift = st.shift && hasShift;
    if (c <= Ctl::Screen7) {
        // function label is on the screen's soft-key bar right above
        centered(fb, r, r.y + 4, n.label.empty() ? "-" : n.label, n.label.empty() ? pc::unused : pc::label);
        centered(fb, r, r.y + 21, keyHint(ed, c), pc::key);
        return;
    }
    uint32_t tint = 0;
    switch (c) {
        case Ctl::Note: tint = pc::note; break;
        case Ctl::Instr: tint = pc::instr; break;
        case Ctl::Fx1: tint = pc::fx1; break;
        case Ctl::Fx2: tint = pc::fx2; break;
        case Ctl::Rec: tint = pc::rec; break;
        default: break;
    }
    if (tint) fb.rect(r.x + 1, r.y + r.h - 5, r.w - 2, 4, tint);
    centered(fb, r, r.y + 3, ctlName(c), tint ? tint : pc::name);
    uint32_t lc = n.label.empty() ? pc::unused : (showShift ? pc::name : pc::label);
    centered(fb, r, r.y + 18, n.label.empty() ? "-" : n.label, lc);
    centered(fb, r, r.y + 33, keyHint(ed, c), pc::key);
    if (hasShift) {
        std::string sh = s.label;  // purple = with Shift
        std::string sk = shiftHint(ed, c);
        if (!sk.empty()) sh += " " + sk;
        centered(fb, r, r.y + 48, sh, showShift ? pc::label : pc::shift);
    }
}

static void drawPad(Framebuffer& fb, const Editor& ed, Ctl c, const PanelState& st) {
    Rect r;
    ctlRect(c, r);
    Command k = controlCommand(ed, c, st.shift);
    uint32_t fill = pc::padOff;
    bool cur = false;
    if (k.type == Command::Slot) {
        switch (ed.slotState(k.slot)) {
            case SlotState::Playing: fill = pc::padPlaying; break;
            case SlotState::Edited: fill = pc::padEdited; break;
            case SlotState::Error: fill = pc::padError; break;
            default: fill = pc::padEmpty; break;
        }
        cur = k.slot == ed.slot();
    } else if (k.type == Command::Number) {
        // rows alternate shades like octaves; 0 (second row start) stands out
        fill = (padIndex(c) / kPadCols) % 2 ? 0x2B3340 : 0x252B35;
        int v;
        cur = ed.cursorInt(v) && v == k.number;
        if (k.number == 0) fill = 0x35506B;
    }
    if (st.lit == c) fill = pc::btnLit;
    fb.rect(r.x, r.y, r.w, r.h, fill);
    fb.frame(r.x, r.y, r.w, r.h, cur ? 0xFFFFFF : (st.lit == c ? pc::accent : pc::btnEdge));
    if (cur) fb.frame(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 0xFFFFFF);
    if (k.type == Command::Slot) {
        centered(fb, r, r.y + 8, k.label, pc::label, FontId::Mono14);
        centered(fb, r, r.y + 30, keyHint(ed, c), pc::key);
    } else if (k.type == Command::Number) {
        centered(fb, r, r.y + 17, k.label, pc::label, FontId::Mono14);
    }
}

static void drawJog(Framebuffer& fb, const Editor& ed, const PanelState& st) {
    fb.disc(kJogX, kJogY, kJogR + 4, pc::btnEdge);
    fb.disc(kJogX, kJogY, kJogR, pc::btn);
    if (st.lit == Ctl::JogUp) fb.rect(kJogX - 60, kJogY - 90, 120, 4, pc::accent);
    if (st.lit == Ctl::JogDown) fb.rect(kJogX - 60, kJogY + 86, 120, 4, pc::accent);
    fb.rect(kJogX - kJogR + 8, kJogY, 2 * kJogR - 16, 1, pc::btnEdge);
    fb.disc(kJogX, kJogY, 22, pc::bezel);
    Rect up = {kJogX - 80, 0, 160, 0}, dn = up;
    for (int k = 0; k < 2; k++) {
        Ctl c = k ? Ctl::JogDown : Ctl::JogUp;
        const Rect& r = k ? dn : up;
        int y0 = k ? kJogY + 30 : kJogY - 80;
        Command n = controlCommand(ed, c, false), s = controlCommand(ed, c, true);
        centered(fb, r, y0, n.label, st.shift ? pc::name : pc::label);
        centered(fb, r, y0 + 15, keyHint(ed, c), pc::key);
        centered(fb, r, y0 + 30, s.label, st.shift ? pc::label : pc::shift);
    }

}

void drawPanel(const Editor& ed, const Framebuffer& screen, Framebuffer& out, const PanelState& st) {
    out.clear(pc::body);
    out.text(kSX, 10, "Polyend Tracker (original) - mock panel", pc::name, FontId::Sans12);
    // screen with bezel
    out.rect(kSX - 8, kSY - 8, kW + 16, kH + 16, pc::bezel);
    out.blit(screen, kSX, kSY);
    for (int i = 0; i < 8; i++) drawButton(out, ed, (Ctl)i, st);
    drawJog(out, ed, st);
    for (int i = (int)Ctl::Params; i <= (int)Ctl::Right; i++) drawButton(out, ed, (Ctl)i, st);
    for (int i = 0; i < 48; i++) drawPad(out, ed, padCtl(i), st);

    // legend between the Play/Rec block and the jog wheel
    int ly = kRowE + kBH + 10;
    const char* legend[] = {
        "Button: name / function / key / purple = with Shift",
        "Click to press; click Shift to latch it.",
        "Pads: numbers on a value, else slots; Shift+pad = slot",
        "Tab, Shift+Tab: next / prev slot pad",
        "Ctrl+Q quits (no Tracker equivalent)",
    };
    for (const char* l : legend) {
        out.text(kRX, ly, l, pc::name, FontId::Sans12);
        ly += 15;
    }

    // last input
    std::string last = st.last.empty() ? "press a key or click a control" : st.last;
    out.text(kSX, kPanelH - 30, last.c_str(), pc::accent, FontId::Mono14);
}

}  // namespace proto
