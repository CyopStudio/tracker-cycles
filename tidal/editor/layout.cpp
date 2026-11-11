// See layout.h. The rules are the prototype view's (tidal/proto/view.cpp
// before step 3), unchanged: blocks in file order with setcps first and an
// empty current slot's hint last; wrapping at spaces with continuation lines
// indented by 4; the current block scrolled into view by its pixel height.
#include "layout.h"

#include <stdio.h>
#include <string.h>

namespace tc {

namespace {

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
    memset(s.spans, 0, sizeof s.spans);
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
    if (lit)  // steps whose events sound now (only on the slot's playing block)
        for (const Editor::LitStep& l : ed.litSteps()) {
            if (l.slot != slot || !(l.on <= now && now < l.off) || !l.node || l.node >= kMaxNodes) continue;
            const Span& sp = s.spans[l.node];
            for (int i = sp.start; i < sp.end && i < n; i++) s.flags[i] |= kCellLit;
        }
    if (cur && !cps && ed.gapSeq()) {
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
        const Program& pr = ed.prog();
        const uint16_t l = pr.at(ed.cursor()).first, r = l ? pr.at(l).next : 0;
        if (l && r)
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

struct Row {
    uint16_t blk;  // 0 = setcps or the empty current slot
    int slot;
    int idx;  // list index (comments), -1 = the placeholder, -2 = setcps
};

}  // namespace

void layoutCode(const Editor& ed, double now, const CodeGeometry& g, CodeScreen& out, LayoutScratch& s) {
    const Program& p = ed.prog();
    out.n = 0;
    out.box.on = false;
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

    // heights (a layout pass per block), then the scroll
    int heights[kMaxBlocks + 2];
    int curY = 0, curH = 0, y = 0;
    for (int k = 0; k < nrows; k++) {
        const Row& rw = rows[k];
        int lines = 1;
        if (rw.idx == -2 || rw.blk) layoutText(ed, rw.blk, rw.idx == -2, false, false, rw.slot, now, g.cols, s, lines);
        const bool hasComment = rw.idx >= 0 && p.blockComment(rw.idx)[0];
        const int h = (rw.blk || rw.idx == -2) ? (hasComment ? g.lineH : 0) + lines * g.lineH + g.blockGap
                                               : g.lineH + g.blockGap;
        if (isCur(rw)) {
            curY = y;
            curH = h;
        }
        heights[k] = h;
        y += h;
    }
    const int scroll = curY + curH > g.areaH ? curY + curH - g.areaH : 0;

    // the lines of the blocks that reach the area
    y = -scroll;
    for (int k = 0; k < nrows; k++) {
        const Row& rw = rows[k];
        const int h = heights[k];
        const bool cur = isCur(rw);
        if (y + h >= 0 && y < g.areaH) {
            if (cur) out.box = CodeBox{true, (int16_t)y, (int16_t)h};
            int yy = y;
            if (rw.idx >= 0 && p.blockComment(rw.idx)[0]) {
                if (CodeLine* l = newLine(out, yy, LineKind::Comment)) setText(*l, p.blockComment(rw.idx));
                yy += g.lineH;
            }
            if (!rw.blk && rw.idx != -2) {
                if (CodeLine* l = newLine(out, yy, LineKind::Hint)) {
                    char hint[64];
                    snprintf(hint, sizeof hint, "d%d   (empty: Replace / Instrument / Note start a block)", rw.slot + 1);
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
