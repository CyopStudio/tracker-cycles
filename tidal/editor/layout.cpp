// See layout.h. The rules are the prototype view's (tidal/proto/view.cpp
// before step 3), unchanged: blocks in file order with setcps first and an
// empty current slot's hint last; wrapping at spaces with continuation lines
// indented by 4; the current block scrolled into view by its pixel height.
#include "layout.h"

#include <stdio.h>
#include <string.h>

namespace tc {

namespace {

constexpr uint32_t kSpansClean = 0x5C1EA9u;

// A fingerprint of n's subtree: what its rendering (so its height) depends on.
uint32_t nodeHash(const Program& p, uint16_t n, uint32_t h) {
    if (!n) return h;
    const Node& x = p.at(n);
    auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
    mix((uint32_t)x.kind | (uint32_t)x.flags << 8);
    mix(x.a);
    mix(x.b);
    if (x.kind == Kind::MWord || x.kind == Kind::MChord)  // string ids: their text
        for (const char* c = p.str((int)x.a); c && *c; c++) mix((uint8_t)*c);
    for (uint16_t c = x.first; c; c = p.at(c).next) h = nodeHash(p, c, h);
    mix(0xFFu);  // end of children
    return h;
}

// Zero the spans of n's subtree (what the renderer wrote for one block).
void clearSpans(const Program& p, uint16_t n, LayoutScratch& s) {
    if (!n) return;
    s.spans[n] = Span{0, 0};
    for (uint16_t c = p.at(n).first; c; c = p.at(c).next) clearSpans(p, c, s);
}

bool styleOf(Kind k, CodeStyle& st) {
    switch (k) {
        case Kind::Fn: st = CodeStyle::Fn; return true;
        case Kind::Num:
        case Kind::MNum: st = CodeStyle::Num; return true;
        case Kind::MWord:
        case Kind::MNote:
        case Kind::MChord:
        case Kind::MBool: st = CodeStyle::Word; return true;
        case Kind::Mini: st = CodeStyle::Quote; return true;
        case Kind::Hole: st = CodeStyle::Hole; return true;
        default: return false;
    }
}

void paint(const Program& p, uint16_t idx, LayoutScratch& s, int n) {
    const Node& nd = p.at(idx);
    CodeStyle st;
    const Span& sp = s.spans[idx];
    if (styleOf(nd.kind, st) && sp.end > sp.start) {
        if (nd.kind == Kind::Mini) {  // only the quote characters
            if (sp.start < n) s.style[sp.start] = st;
            if (sp.end >= 1 && sp.end - 1 < n) s.style[sp.end - 1] = st;
        } else {
            for (int i = sp.start; i < sp.end && i < n; i++) s.style[i] = st;
        }
    }
    for (uint16_t ch = nd.first; ch; ch = p.at(ch).next) paint(p, ch, s, n);
}

// One block (or the setcps line) into s: text, styles, flags, rows /
// columns. Returns the character count; rows = wrapped line count.
int layoutText(const Editor& ed, uint16_t blk, bool cps, bool cur, bool lit, int slot, double now, int cols,
               LayoutScratch& s, int& rows) {
    const Program& p = ed.prog();
    if (s.clean != kSpansClean) {  // the first use (raw storage on the device)
        memset(s.spans, 0, sizeof s.spans);
        s.clean = kSpansClean;
        s.heightKey = 0;
        s.nKnown = 0;
        s.geomKey = 0;
    }
    Renderer r(s.text, kLayoutText + 1, s.spans);
    r.setBreaks(true);
    s.text[0] = 0;
    int n = cps ? r.cps(p) : r.blockNode(p, blk);
    if (n > kLayoutText) n = kLayoutText;
    for (int i = 0; i < n; i++) {
        s.style[i] = CodeStyle::Text;
        s.flags[i] = 0;
    }
    paint(p, cps ? p.cpsExpr : blk, s, n);
    if (!cps && blk) {  // the block's "dN" (its first word) by the slot's state
        // dN's playing block (or its selected one when nothing plays) shows
        // the slot's state; dN's other blocks stay plain
        CodeStyle st = CodeStyle::Text;
        const uint16_t playing = ed.playingBlock(slot);
        if (blk == playing || (!playing && blk == p.block[slot])) {
            switch (ed.slotState(slot)) {
                case SlotState::Playing: st = ed.muted(slot) ? CodeStyle::SlotMuted : CodeStyle::SlotPlay; break;
                case SlotState::Edited: st = CodeStyle::SlotEdit; break;
                case SlotState::Error: st = CodeStyle::SlotErr; break;
                default: break;
            }
            if (playing && ed.muted(slot)) st = CodeStyle::SlotMuted;
        }
        for (int i = 0; i < n && s.text[i] != ' '; i++) s.style[i] = st;
    }
    if (lit)  // steps whose events sound now (only on the slot's playing block)
        for (const Editor::LitStep& l : ed.litSteps()) {
            if (l.slot != slot || !(l.on <= now && now < l.off) || !l.node || l.node >= kMaxNodes) continue;
            const Span& sp = s.spans[l.node];
            for (int i = sp.start; i < sp.end && i < n; i++) s.flags[i] |= kCellLit;
        }
    int ta, tb, pa, pb;
    int barAt;
    if (cur && !cps && ed.cursorBarChar(barAt)) {  // a bar: before the pattern / an operator / `$`
        if (barAt < n) s.flags[barAt] |= kCellGap;
    } else if (cur && !cps && ed.cursorTokenChars(ta, tb, pa, pb)) {  // just the bracket / quote / dN
        for (int i = ta; i < tb && i < n; i++) s.flags[i] |= kCellSel;
        for (int i = pa; i < pb && i < n; i++) s.flags[i] |= kCellPair;
    } else if (cur && !cps && ed.gapSeq()) {
        const uint16_t seq = ed.gapSeq();
        const int count = p.childCount(seq), pos = ed.gapPos();
        int gapAt = -1;
        if (pos < count) gapAt = s.spans[p.child(seq, pos)].start;
        else if (count) gapAt = s.spans[p.child(seq, count - 1)].end;
        if (gapAt >= 0 && gapAt < n) s.flags[gapAt] |= kCellGap;
    } else if (cur && !cps && ed.selFirst()) {  // a selected run of steps
        const int a = s.spans[ed.selFirst()].start, b = s.spans[ed.selLast()].end;
        for (int i = a; i < b && i < n; i++) s.flags[i] |= kCellSel;
    } else if (cur && !cps && ed.onOperator()) {  // just the operator's symbol, between its operands
        uint16_t l, r;
        if (ed.operatorSides(l, r))
            for (int i = s.spans[l].end; i < s.spans[r].start && i < n; i++)
                if (s.text[i] != ' ' && s.text[i] != '(' && s.text[i] != ')') s.flags[i] |= kCellSel;
    } else if (cur && ed.cursor()) {
        const Span& sp = s.spans[ed.cursor()];
        for (int i = sp.start; i < sp.end && i < n; i++) s.flags[i] |= kCellSel;
    }
    // wrap at spaces, continuation lines indented by 4; a line break (kBreak,
    // the renderer's "\n    ") starts a new row
    int row = 0, c = 0, i = 0;
    while (i < n) {
        int j = i;
        while (j < n && s.text[j] != ' ' && s.text[j] != '\n') j++;
        const int wl = j - i;
        if (c > 4 && c + wl > cols) {
            row++;
            c = 4;
        }
        for (int k = i; k < j; k++) {
            if (c >= cols) {
                row++;
                c = 4;
            }
            s.row[k] = (uint16_t)row;
            s.col[k] = (uint8_t)(c < 255 ? c : 255);
            c++;
        }
        if (j < n && s.text[j] == '\n') {  // a line break: a new row (its cell drawn as a space)
            s.row[j] = (uint16_t)row;
            s.col[j] = (uint8_t)(c < 255 ? c : 255);
            row++;
            c = 0;
        } else if (j < n) {  // the space
            s.row[j] = (uint16_t)row;
            s.col[j] = (uint8_t)(c < 255 ? c : 255);
            c++;
        }
        i = j + 1;
    }
    rows = row + 1;
    clearSpans(p, cps ? p.cpsExpr : blk, s);  // all zero again for the next block
    return n;
}

CodeLine* newLine(CodeScreen& out, int y, LineKind kind) {
    if (out.n >= kCodeLines) return nullptr;
    CodeLine& l = out.line[out.n++];
    l.y = (int16_t)y;
    l.n = 0;
    l.kind = kind;
    l.bar = LineBar::None;
    l.dim = false;
    return &l;
}

void setText(CodeLine& l, const char* t) {
    int n = 0;
    for (; t[n] && n < kCodeCols; n++) {
        l.cell[n].ch = t[n];
        l.cell[n].style = CodeStyle::Text;
        l.cell[n].flags = 0;
    }
    l.n = (uint8_t)(n < 255 ? n : 255);
}

struct BlockHeightCache {
    LayoutScratch::BlockHeight e[kMaxBlocks + 2];
    int n;
};

struct Row {
    uint16_t blk;  // 0 = setcps or the empty current slot
    int slot;
    int idx;  // list index (comments), -1 = the placeholder, -2 = setcps
};

}  // namespace

void layoutCode(const Editor& ed, double now, const CodeGeometry& g, CodeScreen& out, LayoutScratch& s) {
    const Program& p = ed.prog();
    out.n = 0;
    out.tokenMode = ed.cursorMode();
    out.box.on = false;
    out.flash.on = false;
    Row rows[kMaxBlocks + 2];
    int nrows = 0;
    // the empty placeholder (a slot with no block, a new dN block not started
    // yet, a deleted block's place) sits where the editor says
    const int empty = ed.emptyRowIndex();
    if (p.cpsExpr) rows[nrows++] = Row{0, -1, -2};
    for (int i = 0; i < p.blockCount(); i++) {
        if (i == empty) rows[nrows++] = Row{0, ed.slot(), -1};
        rows[nrows++] = Row{p.blockAt(i), p.blockSlot(p.blockAt(i)), i};
    }
    if (empty >= p.blockCount()) rows[nrows++] = Row{0, ed.slot(), -1};
    const uint16_t curBlk = ed.onCps() || empty >= 0 ? 0xFFFF : p.block[ed.slot()];
    auto isCur = [&](const Row& rw) { return rw.idx == -2 ? ed.onCps() : rw.blk ? rw.blk == curBlk : empty >= 0; };
    auto litFor = [&](const Row& rw) { return rw.blk && rw.blk == ed.playingBlock(rw.slot); };

    // heights (a layout pass per block), then the scroll; the heights only
    // change with the code, the rows or the geometry (not with highlights,
    // the flash or the cursor), so they're kept while those stay
    uint32_t key = 2166136261u;
    auto mix = [&](uint32_t v) { key = (key ^ v) * 16777619u; };
    mix(ed.changes());
    mix((uint32_t)(empty + 1));
    mix((uint32_t)ed.slot());
    mix((uint32_t)nrows);
    mix((uint32_t)g.cols);
    mix((uint32_t)g.lineH);
    mix((uint32_t)g.blockGap);
    for (int k = 0; k < nrows; k++) mix(rows[k].blk);
    if (key == 0) key = 1;
    const bool haveHeights = s.clean == kSpansClean && s.heightKey == key && s.nHeights == nrows;
    // the code changed: blocks whose nodes / comment / dN are as before keep
    // their height (an edit touches one block); others are measured again
    const uint32_t geom = (uint32_t)g.cols | (uint32_t)g.lineH << 10 | (uint32_t)g.blockGap << 20;
    if (s.clean == kSpansClean && s.geomKey != geom) s.nKnown = 0;
    s.geomKey = geom;
    BlockHeightCache fresh;
    fresh.n = 0;
    int heights[kMaxBlocks + 2];
    int curY = 0, curH = 0, y = 0;
    for (int k = 0; k < nrows; k++) {
        const Row& rw = rows[k];
        int h;
        if (haveHeights) {
            h = s.heights[k];
        } else if (!rw.blk && rw.idx != -2) {
            h = g.lineH + g.blockGap;  // the empty placeholder
        } else {
            const uint16_t root = rw.idx == -2 ? p.cpsExpr : rw.blk;
            uint32_t hh = nodeHash(p, root, 2166136261u ^ (uint32_t)(rw.idx == -2));
            if (rw.idx >= 0)
                for (const char* c = p.blockComment(rw.idx); *c; c++) hh = (hh ^ (uint8_t)*c) * 16777619u;
            h = -1;
            for (int i = 0; i < s.nKnown && h < 0; i++)
                if (s.known[i].blk == root && s.known[i].hash == hh) h = s.known[i].h;
            if (h < 0) {
                int lines = 1;
                layoutText(ed, rw.blk, rw.idx == -2, false, false, rw.slot, now, g.cols, s, lines);
                const bool hasComment = rw.idx >= 0 && p.blockComment(rw.idx)[0];
                h = (hasComment ? g.lineH : 0) + lines * g.lineH + g.blockGap;
            }
            if (fresh.n < kMaxBlocks + 2) fresh.e[fresh.n++] = LayoutScratch::BlockHeight{root, hh, (int16_t)h};
        }
        if (!haveHeights) s.heights[k] = (int16_t)h;
        if (isCur(rw)) {
            curY = y;
            curH = h;
        }
        heights[k] = h;
        y += h;
    }
    if (!haveHeights) {
        s.heightKey = key;
        s.nHeights = nrows;
        for (int i = 0; i < fresh.n; i++) s.known[i] = fresh.e[i];
        s.nKnown = fresh.n;
    }
    const int scroll = curY + curH > g.areaH ? curY + curH - g.areaH : 0;
    out.scroll = (int16_t)scroll;
    out.contentH = (int16_t)(y < 32767 ? y : 32767);

    // the lines of the blocks that reach the area
    y = -scroll;
    for (int k = 0; k < nrows; k++) {
        const Row& rw = rows[k];
        const int h = heights[k];
        const bool cur = isCur(rw);
        if (y + h >= 0 && y < g.areaH) {
            if (cur) out.box = CodeBox{true, (int16_t)y, (int16_t)h};
            if (rw.blk && rw.blk == ed.flashBlock() && ed.flashOn(now))
                out.flash = CodeFlash{true, ed.flashOk(), (int16_t)y, (int16_t)h};
            int yy = y;
            if (rw.idx >= 0 && p.blockComment(rw.idx)[0]) {
                if (CodeLine* l = newLine(out, yy, LineKind::Comment)) setText(*l, p.blockComment(rw.idx));
                yy += g.lineH;
            }
            if (!rw.blk && rw.idx != -2) {
                if (CodeLine* l = newLine(out, yy, LineKind::Hint)) {
                    char hint[64];
                    snprintf(hint, sizeof hint, "d%d   (empty: Instrument / Note / Screen 1-8 start a block)", rw.slot + 1);
                    setText(*l, hint);
                    if (cur)  // the cursor is on this empty slot: "dN" selected
                        for (int i = 0; i < l->n && l->cell[i].ch != ' '; i++) l->cell[i].flags |= kCellSel;
                }
            } else {
                const bool cps = rw.idx == -2;
                int lines = 1;
                const int n = layoutText(ed, rw.blk, cps, cur, !cps && litFor(rw), rw.slot, now, g.cols, s, lines);
                const bool selected = cps || p.block[rw.slot] == rw.blk;  // dN's selected alternative
                const LineBar bar = cps ? (ed.cpsSent() ? LineBar::CpsSent : LineBar::CpsEdited)
                                        : (rw.blk == ed.playingBlock(rw.slot) ? LineBar::Playing : LineBar::None);
                int i = 0;
                for (int r = 0; r < lines; r++) {
                    CodeLine* l = newLine(out, yy + r * g.lineH, cps ? LineKind::Cps : LineKind::Code);
                    if (!l) break;
                    l->bar = bar;
                    l->dim = !selected;
                    while (i < n && s.row[i] == r) {
                        if (l->n < kCodeCols) {
                            CodeCell& c = l->cell[l->n];
                            c.ch = s.text[i] == '\n' ? ' ' : s.text[i];
                            c.style = s.style[i];
                            c.flags = cps ? (uint8_t)(s.flags[i] & kCellSel) : s.flags[i];
                            l->col[l->n] = s.col[i];
                            l->n++;
                        }
                        i++;
                    }
                }
            }
        }
        y += h;
    }
}

}  // namespace tc
