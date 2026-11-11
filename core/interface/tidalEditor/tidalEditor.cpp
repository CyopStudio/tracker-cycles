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
constexpr uint32_t muted = 0x5A7FD0;
}  // namespace col
// the last four: a block's dN by its slot's state (playing, edited, error, muted)
const uint32_t kStyle[] = {col::text, col::fn, col::num,    col::word, col::quote,
                           col::hole, col::ok, col::edited, col::err,  col::muted};

// screen geometry (800 x 480): no title bar and no d1-d16 list (UI notes,
// Editor14): the code from the top left, a slim scroll bar at the left
// edge, a slim info box and slim soft keys below
constexpr int kCodeX = 16, kCodeW = 800 - kCodeX - 10;
constexpr int kAreaTop = 6, kAreaBottom = 410;
constexpr int kInfoY = 415, kInfoLine2 = kInfoY + 15;
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
        API_SAVE_CONTEXT();
        API_SCISSOR_XY(0, kAreaTop);
        API_SCISSOR_SIZE(800, kAreaBottom - kAreaTop);
        // backgrounds: the current block, bars, selection / lit / gap
        API_BEGIN(RECTS);
        if (!half && s.box.on) rect(kCodeX - 6, kAreaTop + s.box.y - 2, kCodeW + 12, s.box.h - 4, col::current);
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
            if (L.bar == tc::LineBar::Playing) rect(kCodeX - 4, y, 2, lh, col::ok);
            if (L.bar == tc::LineBar::CpsSent) rect(kCodeX - 4, y, 2, lh, col::ok);
            if (L.bar == tc::LineBar::CpsEdited) rect(kCodeX - 4, y, 2, lh, col::edited);
            if (L.kind == tc::LineKind::Hint) {
                // the empty current slot: a cursor box behind "dN"
                for (int i = 0; i < L.n && (L.cell[i].flags & tc::kCellSel); i++)
                    rect(kCodeX + i * adv, y - 1, adv, lh, col::cursor);
                continue;
            }
            if (L.kind != tc::LineKind::Code && L.kind != tc::LineKind::Cps) continue;
            for (int i = 0; i < L.n; i++) {
                const tc::CodeCell& c = L.cell[i];
                const int x = kCodeX + L.col[i] * adv;
                const bool sel = c.flags & tc::kCellSel, lit = c.flags & tc::kCellLit;
                if (sel) rect(x, y - 1, adv, lh, lit ? col::litSel : col::cursor);
                else if (lit) rect(x, y - 1, adv, lh, col::lit);
                if (c.flags & tc::kCellGap) rect(x - 2, y - 1, 3, lh, col::accent);
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
            if (ch <= ' ' || ch < first || ch > 126 || y < 0 || y > 479) return;
            if (c != colour) {
                API_COLOR(c);
                colour = c;
            }
            const int want = x >= 512 ? 256 : 0;
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

int gDlBytes = 0;  // our controls' display lists, for the status line

class TcSideView : public TcControl
{
  public:
    explicit TcSideView(strControlProperties* p) : TcControl(p, 1, 1) {}
    void update() override
    {
        const tc::Editor* ed = tidalEditorModel();
        API_SAVE_CONTEXT();
        API_BEGIN(RECTS);
        rect(0, kInfoY - 3, 800, kKeysY - (kInfoY - 3), col::panel);
        API_END();
        const int16_t h = smallFont().handle;
        if (ed) {
            if (tidalEditorStatusOpen()) {  // status (Shift+Master) in place of the cursor info
                char st[200];
                const int n = tidalEditorStatusSummary(st, sizeof st);
                snprintf(st + n, sizeof st - n, "   dl %d B", gDlBytes);
                API_COLOR(col::accent);
                API_CMD_TEXT(10, kInfoY, h, 0, st);
            } else {
                // cut short of the status on the right (Regular 12: <= 8 px a glyph)
                const int statusW = ((int)strlen(tidalEditorStatus()) + 12) * 8;
                API_SCISSOR_XY(0, kInfoY - 4);
                API_SCISSOR_SIZE(800 - 10 - statusW > 100 ? 800 - 10 - statusW : 100, 16);
                API_COLOR(col::dim);
                API_CMD_TEXT(10, kInfoY, h, 0, ed->cursorInfo().c_str());
                API_SCISSOR_XY(0, 0);
                API_SCISSOR_SIZE(800, 480);
            }
            API_COLOR(ed->messageIsError() ? col::err : col::text);
            API_CMD_TEXT(10, kInfoLine2, h, 0, ed->message().c_str());
        }
        if (!tidalEditorStatusOpen()) {
            char st[120];
            snprintf(st, sizeof st, "%s   dl %d B", tidalEditorStatus(), gDlBytes);
            API_COLOR(col::dim);
            API_CMD_TEXT(790, kInfoY, h, OPT_RIGHTX, st);
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
    if (c == Control::Play) {
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
    const bool shift = shiftHeld();
    const int n = value > 0 ? value : -value;
    for (int i = 0; i < n && i < 16; i++) tc::press(*ed, value > 0 ? Control::JogUp : Control::JogDown, shift);
    tidalEditorSetDirty();
}

}  // namespace

void cTidalEditorModule::start(uint32_t)
{
    moduleRefresh = 1;
    for (uint8_t b = 0; b <= (uint8_t)Control::Right; b++) ip->setButtonObj(b, functButton);
    ip->setPadsGlobal(functPads);
    ip->setPotObj(interfacePot0, functPot, false);
    titleBar->hide();  // no title bar: more room for the code (UI notes, Editor14)
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
    }
}

void cTidalEditorModule::redraw()
{
    tc::Editor* ed = tidalEditorModel();
    const tc::CodeScreen& scr = tidalEditorLayout(geometry());
    display.setControlData(code_, (void*)&scr);
    display.setControlShow(code_);
    display.refreshControl(code_);
    display.setControlShow(side_);
    display.refreshControl(side_);
    if (ed && (ed->menuOpen() || ed->keyboardOpen())) {
        display.setControlShow(menu_);
        display.refreshControl(menu_);
    } else {
        display.setControlHide(menu_);
    }
    gShiftShown = shiftHeld();
    if (ed) updatePads(*ed, gShiftShown);
    for (int i = 0; i < 8; i++) {
        gLabel[i][0] = 0;
        if (ed) {
            const tc::Command k = tc::controlCommand(*ed, (Control)i, gShiftShown);
            snprintf(gLabel[i], sizeof gLabel[i], "%s", k.label.c_str());
        }
        display.setControlText(label_[i], gLabel[i]);
        display.refreshControl(label_[i]);
    }
    gDlBytes = 0;
    for (TcControl* v : TcControl::views) gDlBytes += v ? v->bytes() : 0;
    display.synchronizeRefresh();
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
