// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
#include "view.h"

#include <string.h>

#include <string>
#include <vector>

#include "check.h"
#include "controls.h"
#include "output.h"
#include "keyboard.h"
#include "layout.h"
#include "render.h"

using namespace tc;

namespace proto {
using namespace tc;  // the editor model (tidal/editor)

namespace col {
constexpr uint32_t bg = 0x16181C;
constexpr uint32_t panel = 0x1F2228;
constexpr uint32_t current = 0x262A32;
constexpr uint32_t text = 0xD8DCE2;
constexpr uint32_t dim = 0x6E7580;
constexpr uint32_t fn = 0x79B8FF;
constexpr uint32_t num = 0xFFB86B;
constexpr uint32_t word = 0x9FE08F;
constexpr uint32_t quote = 0x6FA868;
constexpr uint32_t cursor = 0x22426E;  // darker (Editor9: contrast with the text)
constexpr uint32_t hole = 0xFF6B6B;
constexpr uint32_t err = 0xFF6B6B;
constexpr uint32_t ok = 0x7BD88F;
constexpr uint32_t edited = 0xF0C060;
constexpr uint32_t accent = 0xFF9F1C;
constexpr uint32_t lit = 0x553810;     // a step whose event is sounding
constexpr uint32_t litSel = 0x6E4A18;  // the same inside the cursor's selection
constexpr uint32_t cursorTok = 0x52306E;  // cursor mode's cursor (tree mode: cursor; user, RC11)
constexpr uint32_t litTok = 0x6E3A40;     // lit inside cursor mode's cursor
constexpr uint32_t pairTok = 0x2E2440;    // the partner of the bracket / quote at the cursor
}  // namespace col


void drawEditor(const Editor& ed, Framebuffer& fb) {
    fb.clear(col::bg);

    // code area: tidal/editor/layout.* (shared with the device's view)
    const Font& f = fb.font(FontId::Mono14);
    const int adv = f.widths['m'] ? f.widths['m'] : f.width;  // monospace advance
    // no title bar and no d1-d16 list (UI notes, Editor14): the code from
    // the top left, a slim scroll bar at the left edge, each block's dN in
    // its slot's state colour
    const int cx = 16, cw = kW - cx - 10, lineH = f.height + 3;
    const int areaTop = 6, areaBottom = 404;
    static CodeScreen scr;
    static LayoutScratch scratch;
    layoutCode(ed, steadySeconds(), CodeGeometry{cw / adv, lineH, 8, areaBottom - areaTop}, scr, scratch);
    // no current-block strip (Editor22 notes: the cursor shows where you are);
    // the evaluate flash (UI v3 C): green accepted, red refused
    if (scr.flash.on)
        fb.rect(cx - 6, areaTop + scr.flash.y - 2, cw + 12, scr.flash.h - 4, scr.flash.ok ? 0x1E4A2A : 0x5A1E1E);
    if (scr.contentH > areaBottom - areaTop) {  // scroll bar
        const int ah = areaBottom - areaTop, th = ah * ah / scr.contentH < 12 ? 12 : ah * ah / scr.contentH;
        fb.rect(4, areaTop, 3, ah, col::panel);
        fb.rect(4, areaTop + (ah - th) * scr.scroll / (scr.contentH - ah), 3, th, col::dim);
    }
    static const uint32_t kStyle[] = {col::text, col::fn,    col::num, col::word, col::quote,
                                      col::hole, col::ok,    col::edited, col::err, 0x5A7FD0};
    for (int li = 0; li < scr.n; li++) {
        const CodeLine& L = scr.line[li];
        const int yy = areaTop + L.y;
        if (L.kind == LineKind::Comment || L.kind == LineKind::Hint) {
            char t[kCodeCols + 1];
            for (int i = 0; i < L.n; i++) t[i] = L.cell[i].ch;
            t[L.n] = 0;
            int sel = 0;  // the empty current slot: "dN" under the cursor
            while (sel < L.n && (L.cell[sel].flags & kCellSel)) sel++;
            if (sel) fb.rect(cx, yy - 1, sel * adv, lineH, scr.tokenMode ? col::cursorTok : col::cursor);
            fb.text(cx, yy, t, col::dim);
            for (int i = 0; i < sel; i++) {
                char ch[2] = {L.cell[i].ch, 0};
                fb.text(cx + i * adv, yy, ch, 0xFFFFFF);
            }
            continue;
        }
        if (L.bar == LineBar::Playing) fb.rect(cx - 4, yy, 2, lineH, col::ok);
        if (L.bar == LineBar::CpsSent || L.bar == LineBar::CpsEdited)
            fb.rect(cx - 4, yy, 2, lineH, L.bar == LineBar::CpsSent ? col::ok : col::edited);
        const bool code = L.kind == LineKind::Code;
        if (code && (yy < areaTop - lineH || yy > areaBottom)) continue;
        for (int i = 0; i < L.n; i++) {
            const CodeCell& c = L.cell[i];
            const int px = cx + L.col[i] * adv;
            const bool sel = c.flags & kCellSel, lit = c.flags & kCellLit;
            if (sel) fb.rect(px, yy - 1, adv, lineH, scr.tokenMode ? (lit ? col::litTok : col::cursorTok) : lit ? col::litSel : col::cursor);
            else if (c.flags & kCellPair) fb.rect(px, yy - 1, adv, lineH, lit ? col::lit : col::pairTok);
            else if (lit) fb.rect(px, yy - 1, adv, lineH, col::lit);
            if (c.flags & kCellGap) fb.rect(px - 2, yy - 1, 3, lineH, col::accent);  // gap cursor
            char ch[2] = {c.ch, 0};
            fb.text(px, yy, ch, sel ? 0xFFFFFF : (code && L.dim) ? col::dim : kStyle[(int)c.style]);
        }
    }

    // bottom panel: the cursor info and the message (two slim lines), the
    // backend's state on the right
    fb.rect(0, 406, kW, 42, col::current);  // the soft keys' colour (Editor22 notes)
    fb.text(10, 410, ed.cursorInfo().c_str(), col::dim, FontId::Sans12, 90);
    fb.text(10, 427, ed.message().c_str(), ed.messageIsError() ? col::err : col::text, FontId::Sans12, 110);
    {
        std::string right;
        right += ed.cursorMode() ? "cursor mode   " : "tree mode   ";  // Rec switches (UI v3 D)
        if (ed.trackerMode()) right += "tracker mode   ";
        if (appBackend()) right += std::string(appBackend()->name()) + (appBackend()->alive() ? " ok" : " not running");
        else right += "no backend";
        fb.text(kW - 10 - fb.textWidth(right.c_str(), FontId::Sans12), 410, right.c_str(), col::dim, FontId::Sans12);
    }

    // soft-key bar: labels for the 8 screen buttons below the display (slim)
    const int sy = kH - 30;
    for (int i = 0; i < 8; i++) {
        Command k = controlCommand(ed, (Control)i, false);
        int x = i * 100;
        if (k.label.empty()) continue;
        fb.rect(x + 4, sy + 3, 92, 24, col::current);
        int w = fb.textWidth(k.label.c_str(), FontId::Sans12);
        fb.text(x + (100 - w) / 2, sy + 7, k.label.c_str(), col::text, FontId::Sans12);
    }

    // menu overlay
    if (ed.menuOpen()) {
        const auto& m = ed.menu();
        const int mx = 420, my = 40, mw = 370, rows = 15, rh = 22;
        fb.rect(mx, my, mw, rows * rh + 34, 0x101216);
        fb.frame(mx, my, mw, rows * rh + 34, col::accent);
        std::string title = std::string(ed.menuTitle().c_str()) + "  (" + std::to_string(ed.menuSel() + 1) + "/" +
                            std::to_string(m.size()) + ")";
        fb.text(mx + 10, my + 6, title.c_str(), col::accent, FontId::Sans12);
        int first = ed.menuSel() - rows / 2;
        if (first > (int)m.size() - rows) first = (int)m.size() - rows;
        if (first < 0) first = 0;
        for (int i = 0; i < rows && first + i < (int)m.size(); i++) {
            int idx = first + i;
            int yy = my + 28 + i * rh;
            if (idx == ed.menuSel()) fb.rect(mx + 2, yy - 2, mw - 4, rh, col::cursor);
            fb.text(mx + 10, yy, m[idx].label.c_str(), idx == ed.menuSel() ? 0xFFFFFF : col::text,
                    FontId::Mono14, (mw - 20) / adv);
        }
    }

    // keyboard screen (the Tracker's naming keyboard, keyboard.h): the text
    // field, then the 42 keys laid out on the 12 x 4 pads
    if (ed.keyboardOpen()) {
        const int kx = 20, ky = 40, kw = kW - 40, kh = 330;
        fb.rect(kx, ky, kw, kh, 0x101216);
        fb.frame(kx, ky, kw, kh, col::accent);
        fb.text(kx + 12, ky + 8, ed.kbdTitle().c_str(), col::accent, FontId::Sans12);
        const int fy = ky + 32;
        fb.rect(kx + 12, fy, kw - 24, lineH + 10, col::panel);
        std::string shown = std::string("-- ") + ed.kbdText().c_str();
        for (size_t i = 0; i < shown.size(); i++) {  // per character, like the code area
            char ch[2] = {shown[i], 0};
            fb.text(kx + 20 + (int)i * adv, fy + 5, ch, i < 3 ? col::dim : col::text);
        }
        fb.rect(kx + 20 + (3 + ed.kbdPos()) * adv, fy + 3, 2, lineH + 4, col::accent);  // text cursor
        const int gx = kx + 12, gy = fy + lineH + 26, cellW = (kw - 24) / kPadCols, cellH = 52;
        for (int k = 0; k < kbd::kKeys; k++) {
            const int pad = kbd::kKeyPad[k], r = pad / kPadCols, c = pad % kPadCols;
            const int x = gx + c * cellW, y = gy + r * cellH, w = kbd::keyWidth(k) * cellW - 4;
            const bool sel = k == ed.kbdSel(), on = k == kbd::kKeyCaps && ed.kbdCaps();
            fb.rect(x, y, w, cellH - 4, sel ? col::cursor : on ? 0x5A4A1E : col::current);
            const char ch = ed.kbdCaps() ? kbd::kBig[k] : kbd::kSmall[k];
            std::string label = k == kbd::kKeyBackspace ? "Bksp" : k == kbd::kKeyCaps ? "Caps"
                              : k == kbd::kKeySpace     ? "Space" : std::string(1, ch);
            const int tw = fb.textWidth(label.c_str(), FontId::Sans12);
            fb.text(x + (w - tw) / 2, y + (cellH - 4) / 2 - 7, label.c_str(), sel ? 0xFFFFFF : col::text,
                    FontId::Sans12);
        }
    }
}

}  // namespace proto
