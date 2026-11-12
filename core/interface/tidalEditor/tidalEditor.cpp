// See tidalEditor.h. Drawing follows the prototype's view (tidal/proto/view.cpp)
// through the shared layout (tidal/editor/layout.*): the editor timing build
// showed one CMD_TEXT per colour run costs ~24-29 B of display list, so the
// code is drawn as bitmap glyphs (4 B each) with a colour change only when
// the colour changes (docs/tidal_device_editor.md 4b).
#include "tidalEditor.h"

#ifdef TIDAL_EDITOR
#include <stdio.h>
#include <string.h>

#include "FT812.h"
#include "core/interface.h"
#include "core/interfaceMenuPopup.h"
#include "display.h"
#include "mtPadsBacklight.h"
#include "mtStructs.h"
#include "../../tidal/tidal_editor.h"
#include "../../../tidal/editor/controls.h"
#include "../../../tidal/editor/keyboard.h"
#include "../../../tidal/editor/layout.h"

cTidalEditorModule tidalEditorModule;

namespace {

using tc::Control;

// the prototype's palette
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
constexpr uint32_t lit = 0x553810;
constexpr uint32_t litSel = 0x6E4A18;
constexpr uint32_t cursorTok = 0x52306E;  // cursor mode's cursor (tree mode: cursor; user, RC11)
constexpr uint32_t litTok = 0x6E3A40;
constexpr uint32_t pairTok = 0x2E2440;  // the partner of the bracket / quote at the cursor
constexpr uint32_t muted = 0x5A7FD0;
}  // namespace col
// the last four: a block's dN by its slot's state (playing, edited, error, muted)
const uint32_t kStyle[] = {col::text, col::fn, col::num,    col::word, col::quote,
                           col::hole, col::ok, col::edited, col::err,  col::muted};

// screen geometry (800 x 480): no title bar and no d1-d16 list (UI notes,
// Editor14): the code from the top left, a slim scroll bar at the left
// edge, a slim info box and slim soft keys below
constexpr int kCodeX = 16, kCodeW = 800 - kCodeX - 10;
constexpr int kAreaTop = 6, kAreaBottom = 404;
constexpr int kInfoY = 410, kInfoLine2 = kInfoY + 17;  // Editor22: the first line's bottom 4 px were cut
constexpr int kKeysY = 449, kKeysH = 31;

const strFont& codeFont() { return fonts[2]; }   // Roboto Mono Regular 14 (the default font set)
const strFont& smallFont() { return fonts[4]; }  // Regular 12
int advance() { return codeFont().data[(uint8_t)'m'] ? codeFont().data[(uint8_t)'m'] : 8; }
int lineH() { return codeFont().height + 3; }
int firstChar(const strFont& f) {
    int c = 0;
    while (c < 127 && !f.data[c]) c++;
    return c;
}

tc::CodeGeometry geometry() { return tc::CodeGeometry{kCodeW / advance(), lineH(), 8, kAreaBottom - kAreaTop}; }

void rect(int x, int y, int w, int h, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    API_COLOR(c);
    API_VERTEX2F(x, y);
    API_VERTEX2F(x + w, y + h);
}

// Our controls: their display-list size for the status line.
class TcControl : public cDisplayControl
{
  public:
    TcControl(strControlProperties* p, uint8_t stages, int slot) : cDisplayControl(controlIdSimpleText, stages)
    {
        style = p ? p->style : 0;
        views[slot] = this;
    }
    ~TcControl() override
    {
        for (auto& v : views)
            if (v == this) v = nullptr;
    }
    int bytes() const { return ramSize; }
    void setStyle(uint32_t s) override { style = s; }
    void setText(const char*) override {}
    void setValue(int) override {}
    void setColors(const uint32_t*) override {}
    void setData(void*) override {}
    static TcControl* views[3];
};
TcControl* TcControl::views[3] = {};

// ---- the code area ------------------------------------------------------------------
class TcCodeView : public TcControl
{
  public:
    explicit TcCodeView(strControlProperties* p) : TcControl(p, 2, 0) {}
    void update() override { draw(0); }
    void update2() override { draw(1); }
    void setData(void* d) override { scr_ = static_cast<const tc::CodeScreen*>(d); }

  private:
    const tc::CodeScreen* scr_ = nullptr;

    void draw(int half)
    {
        if (!scr_) return;
        const tc::CodeScreen& s = *scr_;
        const int from = half ? s.n / 2 : 0, to = half ? s.n : s.n / 2;
        const int adv = advance(), lh = lineH();
        // The frame's display list is ~8 KB and overlays are appended last
        // (they draw on top): with several patterns playing, the highlights
        // filled it and the menu was cut off (1.0.0 device pass). So under an
        // open menu / keyboard nothing is drawn, highlights are left out, and
        // backgrounds are one rect per run of cells.
        const tc::Editor* ed = tidalEditorModel();
        const bool menu = ed && ed->menuOpen(), kbd = ed && ed->keyboardOpen();
        auto covered = [&](int x, int y) {
            if (kbd) return x >= 20 && x < 780 && y >= 40 - lh && y < 370;
            if (menu) return x >= 420 - adv && y >= 40 - lh && y < 40 + 13 * 22 + 34;
            return false;
        };
        // A hard display-list budget per half: a control's slot in RAM_G is
        // 8 KB (an overrun corrupts the next control's list), and the whole
        // frame must stay under ~8 KB with the menu / keyboard appended last.
        // Past it the remaining lines are left out (never a broken frame).
        const int budget = (menu || kbd) ? 1800 : 3500;  // bytes per half (2 halves)
        int used = 64;                                    // context, scissor, begin / end
        auto afford = [&](int bytes) {
            if (used + bytes > budget) return false;
            used += bytes;
            return true;
        };
        API_SAVE_CONTEXT();
        API_SCISSOR_XY(0, kAreaTop);
        API_SCISSOR_SIZE(800, kAreaBottom - kAreaTop);
        // backgrounds: the current block, bars, selection / lit / gap
        API_BEGIN(RECTS);
        // no current-block strip (Editor22 notes: the cursor shows where you are; keep the screen dark);
        // the evaluate flash (UI v3 C): green accepted, red refused
        if (!half && s.flash.on && afford(12))
            rect(kCodeX - 6, kAreaTop + s.flash.y - 2, kCodeW + 12, s.flash.h - 4, s.flash.ok ? 0x1E4A2A : 0x5A1E1E);
        const int areaH = kAreaBottom - kAreaTop;
        if (!half && s.contentH > areaH) {  // the scroll bar
            const int th = areaH * areaH / s.contentH < 12 ? 12 : areaH * areaH / s.contentH;
            rect(4, kAreaTop, 3, areaH, col::panel);
            rect(4, kAreaTop + (areaH - th) * s.scroll / (s.contentH - areaH), 3, th, col::dim);
        }
        for (int li = from; li < to; li++) {
            const tc::CodeLine& L = s.line[li];
            const int y = kAreaTop + L.y;
            if (y < kAreaTop - lh || y > kAreaBottom) continue;  // scrolled out: no display list for it
            if (L.bar != tc::LineBar::None && afford(12))
                rect(kCodeX - 4, y, 2, lh, L.bar == tc::LineBar::CpsEdited ? col::edited : col::ok);
            if (L.kind == tc::LineKind::Hint) {
                // the empty current slot: a cursor box behind "dN"
                for (int i = 0; i < L.n && (L.cell[i].flags & tc::kCellSel); i++)
                    if (afford(12)) rect(kCodeX + i * adv, y - 1, adv, lh, s.tokenMode ? col::cursorTok : col::cursor);
                continue;
            }
            if (L.kind != tc::LineKind::Code && L.kind != tc::LineKind::Cps) continue;
            // backgrounds as runs: adjacent cells (same row, next column) of one colour
            auto bgOf = [&](const tc::CodeCell& c) -> uint32_t {
                // highlights stay on under an open menu (RC4 user ask); the budget keeps the frame safe
                const bool sel = c.flags & tc::kCellSel, lit = c.flags & tc::kCellLit;
                if (sel && s.tokenMode) return lit ? col::litTok : col::cursorTok;
                if (!sel && !lit && (c.flags & tc::kCellPair)) return col::pairTok;
                return sel ? (lit ? col::litSel : col::cursor) : lit ? col::lit : 0;
            };
            for (int i = 0; i < L.n;) {
                const uint32_t bg = bgOf(L.cell[i]);
                int j = i + 1;
                while (bg && j < L.n && L.col[j] == L.col[j - 1] + 1 && bgOf(L.cell[j]) == bg) j++;
                const int x = kCodeX + L.col[i] * adv;
                if (bg && !covered(x, y) && afford(12)) rect(x, y - 1, (L.col[j - 1] - L.col[i] + 1) * adv, lh, bg);
                for (int k = i; k < j; k++)
                    if (L.cell[k].flags & tc::kCellGap) {
                        const int gx = kCodeX + L.col[k] * adv;
                        if (!covered(gx, y) && afford(12)) rect(gx - 2, y - 1, 3, lh, col::accent);
                    }
                i = j;
            }
        }
        API_END();
        // glyphs: bitmaps, a colour change only when it changes; past
        // x 511 (VERTEX2II's range) through VERTEX_TRANSLATE_X
        const strFont& f = codeFont();
        const int first = firstChar(f);
        API_BEGIN(BITMAPS);
        API_BITMAP_HANDLE((uint8_t)f.handle);
        uint32_t colour = 0xFFFFFFFE;
        int tx = 0;
        auto glyph = [&](int x, int y, char ch, uint32_t c) {
            if (ch <= ' ' || ch < first || ch > 126 || y < 0 || y > 479 || covered(x, y)) return;
            const int want = x >= 512 ? 256 : 0;
            if (!afford(4 + (c != colour ? 4 : 0) + (want != tx ? 4 : 0))) return;  // vertex, colour, translate
            if (c != colour) {
                API_COLOR(c);
                colour = c;
            }
            if (want != tx) {
                API_VERTEX_TRANSLATE_X((uint32_t)(want * 16));
                tx = want;
            }
            // the cell is the character code: SETFONT2 set the handle up for it
            // (Editor1 drew ch - firstChar: every glyph 32 codes early)
            API_VERTEX2II((uint16_t)(x - tx), (uint16_t)y, (uint8_t)f.handle, (uint8_t)ch);
        };
        for (int li = from; li < to; li++) {
            const tc::CodeLine& L = s.line[li];
            const int y = kAreaTop + L.y;
            if (y < kAreaTop - lh || y > kAreaBottom) continue;
            if (L.kind == tc::LineKind::Comment || L.kind == tc::LineKind::Hint) {
                for (int i = 0; i < L.n && kCodeX + i * adv < 790; i++)
                    glyph(kCodeX + i * adv, y, L.cell[i].ch, (L.cell[i].flags & tc::kCellSel) ? 0xFFFFFF : col::dim);
                continue;
            }
            const bool code = L.kind == tc::LineKind::Code;
            for (int i = 0; i < L.n; i++) {
                const tc::CodeCell& c = L.cell[i];
                const bool sel = c.flags & tc::kCellSel;
                glyph(kCodeX + L.col[i] * adv, y, c.ch,
                      sel ? 0xFFFFFF : (code && L.dim) ? col::dim : kStyle[(int)c.style]);
            }
        }
        API_END();
        if (tx) API_VERTEX_TRANSLATE_X(0);
        API_RESTORE_CONTEXT();
    }
};

// ---- the info box (cursor info / status, message) ------------------------------------------


class TcSideView : public TcControl
{
  public:
    explicit TcSideView(strControlProperties* p) : TcControl(p, 1, 1) {}
    void update() override
    {
        const tc::Editor* ed = tidalEditorModel();
        API_SAVE_CONTEXT();
        API_BEGIN(RECTS);
        // the soft keys' colour (Editor22 notes)
        rect(0, kInfoY - 4, 800, kKeysY - (kInfoY - 4), display.getTheme().bottomButtonsBgColors[1]);
        API_END();
        const int16_t h = smallFont().handle;
        // right: the editing mode (Rec switches, UI v3 D), then the load status
        char right[140];
        const char* status = tidalEditorStatus();
        snprintf(right, sizeof right, "%s%s%s", ed && !ed->cursorMode() ? "tree mode" : "cursor mode",
                 status[0] ? "   " : "", status);
        if (ed) {
            if (tidalEditorStatusOpen()) {  // status (Shift+Master) in place of the cursor info
                char st[200];
                tidalEditorStatusSummary(st, sizeof st);
                API_COLOR(col::accent);
                API_CMD_TEXT(10, kInfoY, h, 0, st);
            } else {
                // cut short of the status on the right (Regular 12: <= 8 px a glyph)
                const int statusW = ((int)strlen(right) + 4) * 8;
                API_SCISSOR_XY(0, kInfoY - 4);
                API_SCISSOR_SIZE(800 - 10 - statusW > 100 ? 800 - 10 - statusW : 100, 20);
                API_COLOR(col::dim);
                API_CMD_TEXT(10, kInfoY, h, 0, ed->cursorInfo().c_str());
                API_SCISSOR_XY(0, 0);
                API_SCISSOR_SIZE(800, 480);
            }
            API_COLOR(ed->messageIsError() ? col::err : col::text);
            API_CMD_TEXT(10, kInfoLine2, h, 0, ed->message().c_str());
        }
        if (!tidalEditorStatusOpen()) {
            API_COLOR(col::dim);
            API_CMD_TEXT(790, kInfoY, h, OPT_RIGHTX, right);
        }
        API_RESTORE_CONTEXT();
    }
};

// ---- menu overlay / keyboard screen ----------------------------------------------------
class TcMenuView : public TcControl
{
  public:
    explicit TcMenuView(strControlProperties* p) : TcControl(p, 1, 2) {}
    void update() override
    {
        const tc::Editor* ed = tidalEditorModel();
        if (!ed) return;
        API_SAVE_CONTEXT();
        if (ed->keyboardOpen()) drawKeyboard(*ed);
        else if (ed->menuOpen()) drawMenu(*ed);
        API_RESTORE_CONTEXT();
    }

  private:
    static void frame(int x, int y, int w, int h, uint32_t c)
    {
        rect(x, y, w, 1, c);
        rect(x, y + h - 1, w, 1, c);
        rect(x, y, 1, h, c);
        rect(x + w - 1, y, 1, h, c);
    }

    void drawMenu(const tc::Editor& ed)
    {
        const auto& m = ed.menu();
        const int mx = 420, my = 40, mw = 370, rows = 13, rh = 22;
        int first = ed.menuSel() - rows / 2;
        if (first > m.size() - rows) first = m.size() - rows;
        if (first < 0) first = 0;
        API_BEGIN(RECTS);
        rect(mx, my, mw, rows * rh + 34, 0x101216);
        frame(mx, my, mw, rows * rh + 34, col::accent);
        if (ed.menuSel() >= first && ed.menuSel() < first + rows)
            rect(mx + 2, my + 26 + (ed.menuSel() - first) * rh, mw - 4, rh, col::cursor);
        API_END();
        char title[96];
        snprintf(title, sizeof title, "%s  (%d/%d)", ed.menuTitle().c_str(), ed.menuSel() + 1, m.size());
        API_COLOR(col::accent);
        API_CMD_TEXT(mx + 10, my + 6, smallFont().handle, 0, title);
        for (int i = 0; i < rows && first + i < m.size(); i++) {
            const tc::MenuItem& it = m[first + i];
            API_COLOR(first + i == ed.menuSel() ? 0xFFFFFF : it.checked ? col::text : col::dim);
            API_CMD_TEXT(mx + 10, my + 28 + i * rh, codeFont().handle, 0, it.label.c_str());
        }
    }

    void drawKeyboard(const tc::Editor& ed)
    {
        const int kx = 20, ky = 40, kw = 760, kh = 330;
        const int fy = ky + 32, lh = lineH();
        const int gx = kx + 12, gy = fy + lh + 26, cellW = (kw - 24) / 12, cellH = 52;
        API_BEGIN(RECTS);
        rect(kx, ky, kw, kh, 0x101216);
        frame(kx, ky, kw, kh, col::accent);
        rect(kx + 12, fy, kw - 24, lh + 10, col::panel);
        rect(kx + 20 + (3 + ed.kbdPos()) * advance(), fy + 3, 2, lh + 4, col::accent);  // text cursor
        for (int k = 0; k < tc::kbd::kKeys; k++) {
            const int pad = tc::kbd::kKeyPad[k], r = pad / 12, c = pad % 12;
            const bool sel = k == ed.kbdSel(), on = k == tc::kbd::kKeyCaps && ed.kbdCaps();
            rect(gx + c * cellW, gy + r * cellH, tc::kbd::keyWidth(k) * cellW - 4, cellH - 4,
                 sel ? col::cursor : on ? 0x5A4A1E : col::current);
        }
        API_END();
        API_COLOR(col::accent);
        API_CMD_TEXT(kx + 12, ky + 8, smallFont().handle, 0, ed.kbdTitle().c_str());
        char field[160];
        snprintf(field, sizeof field, "-- %s", ed.kbdText().c_str());
        API_COLOR(col::text);
        API_CMD_TEXT(kx + 20, fy + 5, codeFont().handle, 0, field);
        for (int k = 0; k < tc::kbd::kKeys; k++) {
            const int pad = tc::kbd::kKeyPad[k], r = pad / 12, c = pad % 12;
            const int w = tc::kbd::keyWidth(k) * cellW - 4;
            const char ch = ed.kbdCaps() ? tc::kbd::kBig[k] : tc::kbd::kSmall[k];
            char label[8] = {ch, 0};
            if (k == tc::kbd::kKeyBackspace) snprintf(label, sizeof label, "Bksp");
            else if (k == tc::kbd::kKeyCaps) snprintf(label, sizeof label, "Caps");
            else if (k == tc::kbd::kKeySpace) snprintf(label, sizeof label, "Space");
            API_COLOR(k == ed.kbdSel() ? 0xFFFFFF : col::text);
            API_CMD_TEXT(gx + c * cellW + w / 2, gy + r * cellH + (cellH - 4) / 2, smallFont().handle,
                         OPT_CENTER, label);
        }
    }
};

char gLabel[8][24];
bool gShiftShown = false;

// ---- pad lights (tc::padLight: the shared rules) ---------------------------------------
constexpr int kPads = tc::kPadCols * tc::kPadRows;
tc::PadLight gPadShown[kPads];
bool gPadsValid = false;

void setPad(int p, tc::PadLight l)
{
    const uint8_t front = mtConfig.values.padsLightFront, back = mtConfig.values.padsLightBack,
                  dim = mtConfig.values.padsLightBackDim;
    if (gPadsValid && gPadShown[p] == tc::PadLight::Blink) padsBacklight.stopBlink(p);  // ours only
    padsBacklight.setFrontLayer(0, 0, p);
    switch (l) {
        case tc::PadLight::Off: padsBacklight.setBackLayer(0, 0, p); break;
        case tc::PadLight::Dim: padsBacklight.setBackLayer(1, dim, p); break;
        case tc::PadLight::Back: padsBacklight.setBackLayer(1, back, p); break;
        case tc::PadLight::Front:
            padsBacklight.setBackLayer(0, 0, p);
            padsBacklight.setFrontLayer(1, front, p);
            break;
        case tc::PadLight::Blink:
            padsBacklight.setBackLayer(0, 0, p);
            padsBacklight.startBlink(front, p);
            break;
    }
    gPadShown[p] = l;
}

void updatePads(const tc::Editor& ed, bool shift)
{
    for (int p = 0; p < kPads; p++) {
        const tc::PadLight l = tc::padLight(ed, p, shift);
        if (!gPadsValid || l != gPadShown[p]) setPad(p, l);
    }
    gPadsValid = true;
}

void clearPads()
{
    if (!gPadsValid) return;
    for (int p = 0; p < kPads; p++)
        if (gPadShown[p] == tc::PadLight::Blink) padsBacklight.stopBlink(p);
    padsBacklight.clearGlobalPads(1, 1, 0);  // not its blink branch: it miscounts pads that don't blink
    gPadsValid = false;
}

bool shiftHeld() { return mtInterface.getPlatform()->getButtons()->isButtonPressed(interfaceButtonShift); }

// ---- input --------------------------------------------------------------------------------
void functButton(uint8_t button, uint8_t state)
{
    tc::Editor* ed = tidalEditorModel();
    if (!ed || button > (uint8_t)Control::Right) return;
    const Control c = (Control)button;  // the Control enum is the firmware's button order
    const bool shift = shiftHeld();
    if (c == Control::Master && shift) {  // the status view
        if (state == buttonPress) tidalEditorToggleStatus();
        return;
    }
    // keys that leave the editor: Params (the instrument parameters), Perform,
    // Master, the Sample keys, Config and File (Shift+Config = the block's
    // comment, Shift+File saves, Shift+Sample Loader = the Tidal folders)
    const bool leave = c == Control::Params || c == Control::Perform || c == Control::Master ||
                       (c >= Control::SmpLoad && c <= Control::SmpRec && !(c == Control::SmpLoad && shift)) ||
                       (c == Control::Config && !shift) || (c == Control::File && !shift);
    if (leave) {
        if (state == buttonPress) tidalEditorModule.eventFunct(eventSwitchModule, &tidalEditorModule, &button, 0);
        return;
    }
    if (c == Control::Shift) {
        tidalEditorSetDirty();  // the soft keys show the Shift functions
        return;
    }
    if (c == Control::Pattern) {  // the mute modifier acts on press and release (RC14: was Play)
        if (state == buttonPress) tc::press(*ed, c, shift);
        else if (state == buttonRelease) tc::release(*ed, c);
        tidalEditorSetDirty();
        return;
    }
    if (state != buttonPress) return;
    tc::press(*ed, c, shift);
    tidalEditorSetDirty();
}

void functPads(uint8_t pad, uint8_t state, int16_t)
{
    tc::Editor* ed = tidalEditorModel();
    if (!ed || pad >= tc::kPadCols * tc::kPadRows) return;
    if (state == buttonRelease) {
        tc::release(*ed, tc::padCtl(pad));  // ends a held chord
        return;
    }
    if (state != buttonPress) return;
    tc::press(*ed, tc::padCtl(pad), shiftHeld());
    tidalEditorSetDirty();
}

void functPot(int16_t value)
{
    tc::Editor* ed = tidalEditorModel();
    if (!ed || !value) return;
    // collected and applied once per main-loop pass (tidalEditorUpdate): a
    // fast spin ran up to 16 full edits per event and dropped audio
    // (1.0.0 device pass)
    tidalEditorJog(value, shiftHeld());
}

}  // namespace

void cTidalEditorModule::start(uint32_t)
{
    moduleRefresh = 1;
    for (uint8_t b = 0; b <= (uint8_t)Control::Right; b++) ip->setButtonObj(b, functButton);
    ip->setPadsGlobal(functPads);
    ip->setPotObj(interfacePot0, functPot, false);
    titleBar->hide();  // no title bar: more room for the code (UI notes, Editor14)
    forceAll_ = true;  // every control once
    display.refreshControl(bg_);
    tidalEditorSetDirty();
    redraw();
}

void cTidalEditorModule::stop()
{
    moduleRefresh = 0;
    clearPads();
    display.setControlHide(code_);
    display.setControlHide(side_);
    display.setControlHide(menu_);
}

void cTidalEditorModule::update()
{
    if (!moduleRefresh) return;
    const uint32_t ms = mtInterface.getPlatform()->getTimer()->millis();
    const bool shift = shiftHeld();
    if (shift != gShiftShown) tidalEditorSetDirty();
    if (tidalEditorStatusOpen() && ms - lastStatusMs_ >= 250) {  // live numbers
        lastStatusMs_ = ms;
        tidalEditorSetDirty();
    }
    if (ms - lastDrawMs_ < 30) return;  // at most ~33 redraws a second (one display frame each)
    if (tidalEditorTakeDirty()) {
        lastDrawMs_ = lastLitMs_ = ms;
        tidalEditorTakeLitDirty();
        redraw();
        return;
    }
    // highlights only: the code view, at most ~16 times a second (each
    // redraw is a display frame and 3.5-5.7 ms of main loop, editor bench v1)
    if (ms - lastLitMs_ >= 60 && tidalEditorTakeLitDirty()) {
        lastDrawMs_ = lastLitMs_ = ms;
        const tc::CodeScreen& scr = tidalEditorLayout(geometry());
        display.setControlData(code_, (void*)&scr);
        display.refreshControl(code_);
        display.synchronizeRefresh();
        tidalEditorLitQueued(ms);
    }
}

namespace {
// FNV-1a over a few inputs: a control's "what I show" fingerprint
struct Fnv {
    uint32_t h = 2166136261u;
    void bytes(const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
    }
    void str(const char* s) { bytes(s, strlen(s) + 1); }
    void num(int v) { bytes(&v, sizeof v); }
};
}  // namespace

// Refresh only the controls whose content changed: the display works one
// step per main-loop pass (draw / copy a control, swap the frame), so a
// full redraw of the 13 controls took ~1 s while patterns played (RC5: the
// highlights stalled behind it, a menu's jog step waited for it).
void cTidalEditorModule::redraw()
{
    tc::Editor* ed = tidalEditorModel();
    const tc::CodeScreen& scr = tidalEditorLayout(geometry());
    const bool overlay = ed && (ed->menuOpen() || ed->keyboardOpen());
    bool any = false;
    auto refresh = [&](hControl c) {
        display.refreshControl(c);
        any = true;
    };
    {  // the code view (its budget depends on an open overlay)
        Fnv f;
        f.num(scr.n);
        f.num(scr.scroll);
        f.num(scr.flash.on | scr.flash.ok << 1 | scr.flash.y << 2);
        f.num(overlay);
        f.num(scr.tokenMode);  // the cursor colour follows the mode
        for (int i = 0; i < scr.n; i++) {
            const tc::CodeLine& L = scr.line[i];
            f.num(L.y);
            f.num((int)L.kind | (int)L.bar << 8 | (int)L.dim << 16);
            f.bytes(L.cell, L.n * sizeof L.cell[0]);
            f.bytes(L.col, L.n);
        }
        display.setControlData(code_, (void*)&scr);
        display.setControlShow(code_);
        if (forceAll_ || f.h != codeHash_) {
            codeHash_ = f.h;
            refresh(code_);
        }
    }
    {  // the info box
        Fnv f;
        if (ed) {
            f.str(ed->cursorInfo().c_str());
            f.str(ed->message().c_str());
            f.num(ed->messageIsError());
            f.num(ed->cursorMode());
        }
        f.num(tidalEditorStatusOpen());
        if (tidalEditorStatusOpen()) {
            char st[200];
            tidalEditorStatusSummary(st, sizeof st);
            f.str(st);
        } else {
            f.str(tidalEditorStatus());
        }
        display.setControlShow(side_);
        if (forceAll_ || f.h != sideHash_) {
            sideHash_ = f.h;
            refresh(side_);
        }
    }
    if (overlay) {  // the menu / keyboard
        Fnv f;
        f.num(ed->menuOpen());
        f.str(ed->menuTitle().c_str());
        f.num(ed->menuSel());
        f.num(ed->menu().size());
        f.num(ed->keyboardOpen());
        f.str(ed->kbdText().c_str());
        f.num(ed->kbdPos() | ed->kbdSel() << 8 | (int)ed->kbdCaps() << 16);
        display.setControlShow(menu_);
        if (forceAll_ || !menuShown_ || f.h != menuHash_) {
            menuHash_ = f.h;
            refresh(menu_);
        }
        menuShown_ = true;
    } else if (menuShown_ || forceAll_) {
        display.setControlHide(menu_);
        menuShown_ = false;
        any = true;  // the frame without it
    }
    gShiftShown = shiftHeld();
    if (ed) updatePads(*ed, gShiftShown);
    for (int i = 0; i < 8; i++) {
        gLabel[i][0] = 0;
        if (ed) {
            const tc::Command k = tc::controlCommand(*ed, (Control)i, gShiftShown);
            snprintf(gLabel[i], sizeof gLabel[i], "%s", k.label.c_str());
        }
        Fnv f;
        f.str(gLabel[i]);
        if (forceAll_ || f.h != labelHash_[i]) {
            labelHash_[i] = f.h;
            display.setControlText(label_[i], gLabel[i]);
            refresh(label_[i]);
        }
    }
    forceAll_ = false;
    if (any) display.synchronizeRefresh();
}

void cTidalEditorModule::initDisplayControls()
{
    strControlProperties prop;
    for (uint8_t i = 0; i < 8; i++) {
        prop.colors = display.getTheme().activeLabelsColors;
        prop.text = (char*)"";
        prop.style = (controlStyleShow | controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
        prop.x = (800 / 8) * i + (800 / 16);
        prop.y = kKeysY;
        prop.w = 800 / 8 - 6;
        prop.h = kKeysH;
        prop.value = 0;
        label_[i] = display.createControl<cLabel>(&prop);
    }
    strControlProperties p2;
    p2.value = 255;
    p2.style = controlStyleNoTransparency | controlStyleShow;
    p2.x = 0;
    p2.w = 800;
    p2.y = kKeysY;
    p2.h = 480 - kKeysY;
    bg_ = display.createControl<cBgLabel>(&p2);

    // the display draws lower-numbered controls last (on top): the overlay
    // is created first so it covers the code (Editor1 drew it underneath)
    strControlProperties p3;
    p3.style = 0;
    menu_ = display.createControl<TcMenuView>(&p3);
    p3.style = controlStyleShow;
    code_ = display.createControl<TcCodeView>(&p3);
    side_ = display.createControl<TcSideView>(&p3);
}

void cTidalEditorModule::destroyDisplayControls() {}
#endif
