#include "view.h"

#include <string.h>

#include <string>
#include <vector>

#include "check.h"
#include "hardware.h"
#include "render.h"

using namespace tc;

namespace proto {

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
constexpr uint32_t cursor = 0x2F5FA8;
constexpr uint32_t hole = 0xFF6B6B;
constexpr uint32_t err = 0xFF6B6B;
constexpr uint32_t ok = 0x7BD88F;
constexpr uint32_t edited = 0xF0C060;
constexpr uint32_t accent = 0xFF9F1C;
}  // namespace col

static Span gSpans[kMaxNodes];

static uint32_t stateColor(SlotState s) {
    switch (s) {
        case SlotState::Playing: return col::ok;
        case SlotState::Edited: return col::edited;
        case SlotState::Error: return col::err;
        default: return col::dim;
    }
}

struct BlockLayout {
    std::string text;
    std::vector<uint32_t> color;
    std::vector<bool> sel;
    std::vector<int> row, colx;  // per char
    int rows = 0;
};

static void paint(const Program& p, uint16_t idx, BlockLayout& L) {
    const Node& n = p.at(idx);
    uint32_t c = 0;
    switch (n.kind) {
        case Kind::Fn: c = col::fn; break;
        case Kind::Num:
        case Kind::MNum: c = col::num; break;
        case Kind::MWord:
        case Kind::MNote:
        case Kind::MChord:
        case Kind::MBool: c = col::word; break;
        case Kind::Mini: c = col::quote; break;
        case Kind::Hole: c = col::hole; break;
        default: break;
    }
    const Span& s = gSpans[idx];
    if (c && s.end > s.start) {
        if (n.kind == Kind::Mini) {
            // only the quote characters
            if (s.start < L.color.size()) L.color[s.start] = c;
            if (s.end >= 1 && (size_t)(s.end - 1) < L.color.size()) L.color[s.end - 1] = c;
        } else {
            for (int i = s.start; i < s.end && i < (int)L.color.size(); i++) L.color[i] = c;
        }
    }
    for (uint16_t ch = n.first; ch; ch = p.at(ch).next) paint(p, ch, L);
}

static BlockLayout layoutBlock(const Program& p, int slot, uint16_t cursor, int cols) {
    BlockLayout L;
    char buf[8192];
    memset(gSpans, 0, sizeof gSpans);
    Renderer r(buf, sizeof buf, gSpans);
    buf[0] = 0;
    r.block(p, slot);
    L.text = buf;
    int n = (int)L.text.size();
    L.color.assign(n, col::text);
    L.sel.assign(n, false);
    paint(p, p.block[slot], L);
    if (cursor) {
        const Span& s = gSpans[cursor];
        for (int i = s.start; i < s.end && i < n; i++) L.sel[i] = true;
    }
    // wrap at spaces, continuation lines indented by 4
    L.row.assign(n, 0);
    L.colx.assign(n, 0);
    int row = 0, c = 0, i = 0;
    while (i < n) {
        // find the end of the next word
        int j = i;
        while (j < n && L.text[j] != ' ') j++;
        int wl = j - i;
        if (c > 4 && c + wl > cols) {
            row++;
            c = 4;
        }
        for (int k = i; k < j; k++) {
            if (c >= cols) {
                row++;
                c = 4;
            }
            L.row[k] = row;
            L.colx[k] = c++;
        }
        if (j < n) {  // the space
            L.row[j] = row;
            L.colx[j] = c++;
        }
        i = j + 1;
    }
    L.rows = row + 1;
    return L;
}

void drawEditor(const Editor& ed, Framebuffer& fb) {
    const Program& p = ed.prog();
    fb.clear(col::bg);

    // header
    fb.rect(0, 0, kW, 28, col::panel);
    fb.text(10, 3, "TRACKER CYCLES", col::accent, FontId::MonoBold17);
    std::string right;
    if (ed.trackerMode()) right += "tracker mode   ";
    if (ed.backend()) {
        right += std::string(ed.backend()->name()) + (ed.backend()->alive() ? " ok" : " not running");
    } else {
        right += "no backend";
    }
    fb.text(kW - 10 - fb.textWidth(right.c_str(), FontId::Sans12), 7, right.c_str(), col::dim, FontId::Sans12);
    if (p.cpsExpr) {
        char buf[64];
        Renderer r(buf, sizeof buf);
        buf[0] = 0;
        r.expr(p, p.cpsExpr);
        std::string cps = std::string("cps ") + buf;
        fb.text(260, 7, cps.c_str(), col::dim, FontId::Sans12);
    }

    // slot list
    const int top = 34, rowH = 20;
    for (int s = 0; s < kSlots; s++) {
        int y = top + s * rowH;
        if (s == ed.slot()) fb.rect(0, y - 2, 64, rowH, col::current);
        char name[8];
        snprintf(name, sizeof name, "d%d", s + 1);
        fb.rect(8, y + 6, 6, 6, stateColor(ed.slotState(s)));
        fb.text(20, y, name, s == ed.slot() ? col::text : col::dim);
        if (s == kSampleSlots - 1) fb.rect(4, y + rowH - 2, 56, 1, col::dim);
    }
    fb.text(8, top + kSlots * rowH + 2, "smp|midi", col::dim, FontId::Sans12);

    // code area
    const Font& f = fb.font(FontId::Mono14);
    const int adv = f.widths['m'] ? f.widths['m'] : f.width;  // monospace advance
    const int cx = 76, cw = kW - cx - 10, lineH = f.height + 3;
    const int cols = cw / adv;
    const int areaTop = 34, areaBottom = 376;
    std::vector<int> slots;
    for (int s = 0; s < kSlots; s++)
        if (p.block[s] || s == ed.slot()) slots.push_back(s);
    // layouts and scroll so the current block is visible
    std::vector<BlockLayout> lays;
    std::vector<int> heights;
    int curY = 0, y = 0;
    for (int s : slots) {
        lays.push_back(layoutBlock(p, s, s == ed.slot() ? ed.cursor() : 0, cols));
        int h = (p.comment[s][0] ? lineH : 0) + lays.back().rows * lineH + 8;
        if (!p.block[s]) h = lineH + 8;
        if (s == ed.slot()) curY = y;
        heights.push_back(h);
        y += h;
    }
    int scroll = 0;
    int curH = 0;
    for (size_t k = 0; k < slots.size(); k++)
        if (slots[k] == ed.slot()) curH = heights[k];
    if (curY + curH > areaBottom - areaTop) scroll = curY + curH - (areaBottom - areaTop);
    y = areaTop - scroll;
    for (size_t k = 0; k < slots.size(); k++) {
        int s = slots[k];
        const BlockLayout& L = lays[k];
        int h = heights[k];
        if (y + h >= areaTop && y < areaBottom) {
            if (s == ed.slot()) fb.rect(cx - 6, y - 2, cw + 12, h - 4, col::current);
            int yy = y;
            if (p.comment[s][0]) {
                fb.text(cx, yy, p.comment[s], col::dim);
                yy += lineH;
            }
            if (!p.block[s]) {
                std::string hint = "d" + std::to_string(s + 1) + "   (empty: replace to start)";
                fb.text(cx, yy, hint.c_str(), col::dim);
            } else {
                for (size_t i = 0; i < L.text.size(); i++) {
                    int px = cx + L.colx[i] * adv, py = yy + L.row[i] * lineH;
                    if (py < areaTop - lineH || py > areaBottom) continue;
                    if (L.sel[i]) fb.rect(px, py - 1, adv, lineH, col::cursor);
                    char ch[2] = {L.text[i], 0};
                    fb.text(px, py, ch, L.sel[i] ? 0xFFFFFF : L.color[i]);
                }
            }
        }
        y += h;
    }

    // bottom panel
    fb.rect(0, 380, kW, kH - 380, col::panel);
    fb.text(10, 383, ed.cursorInfo().c_str(), col::dim, FontId::Sans12);
    fb.text(10, 399, ed.message().c_str(), ed.messageIsError() ? col::err : col::text, FontId::Sans12);
    if (ed.backend() && !ed.backend()->log.empty()) {
        const auto& log = ed.backend()->log;
        size_t n = log.size() < 2 ? log.size() : 2;
        for (size_t i = 0; i < n; i++) {
            const std::string& l = log[log.size() - n + i];
            fb.text(10, 417 + (int)i * 15, l.c_str(), col::dim, FontId::Sans12, 120);
        }
    }

    // soft-key bar: labels for the 8 screen buttons below the display
    const int sy = kH - 26;
    fb.rect(0, sy - 2, kW, 1, col::dim);
    for (int i = 0; i < 8; i++) {
        Command k = controlCommand(ed, (Ctl)i, false);
        int x = i * 100;
        if (i) fb.rect(x, sy, 1, 26, col::dim);
        if (k.label.empty()) continue;
        fb.rect(x + 4, sy + 2, 92, 20, col::current);
        int w = fb.textWidth(k.label.c_str(), FontId::Sans12);
        fb.text(x + (100 - w) / 2, sy + 5, k.label.c_str(), col::text, FontId::Sans12);
    }

    // menu overlay
    if (ed.menuOpen()) {
        const auto& m = ed.menu();
        const int mx = 420, my = 40, mw = 370, rows = 15, rh = 22;
        fb.rect(mx, my, mw, rows * rh + 34, 0x101216);
        fb.frame(mx, my, mw, rows * rh + 34, col::accent);
        std::string title = ed.menuTitle() + "  (" + std::to_string(ed.menuSel() + 1) + "/" +
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
}

}  // namespace proto
