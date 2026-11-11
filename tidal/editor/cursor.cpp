// Cursor mode (UI v3 D, docs/tidal_ui_v3.md): the code as text, token by
// token. Rec switches between it and the tree mode; both edit the same tree
// with the same edits (EditSpec, undo, checks), only the cursor moves
// differently. The tokens come from the rendered block: each character's
// deepest node (the renderer's spans), runs of one node's characters = one
// token. An operator's / `$`'s symbol is a token of its own (onOperator),
// a group's brackets select the group, a string's quotes the string, the
// leading "dN" the block's top; expression parentheses and commas are
// skipped.
#include <string.h>

#include "editor.h"

namespace tc {

uint16_t stepOf(const Program& p, uint16_t n);  // editor.cpp

void Editor::setCursorMode(bool on) {
    cursorMode_ = on;
    selA_ = selB_ = selSeq_ = 0;
    tokAt_ = 0xFFFF;
    cursorSettle();
}

void Editor::tokOwners(uint16_t n, int len) {
    const Program& p = *prog_;
    const Span& sp = tokSpans_[n];
    if (sp.end > sp.start)
        for (int i = sp.start; i < sp.end && i < len; i++) tokOwner_[i] = n;
    for (uint16_t c = p.at(n).first; c; c = p.at(c).next) tokOwners(c, len);
}

static bool exprContainer(Kind k) {
    return k == Kind::App || k == Kind::Op || k == Kind::SectionL || k == Kind::SectionR || k == Kind::Compose ||
           k == Kind::Block || k == Kind::List;
}

int Editor::buildTokens() {
    tokN_ = 0;
    tokLines_ = 0;
    const Program& p = *prog_;
    if (cps_ || emptyRowIndex() >= 0) return 0;
    const uint16_t b = p.block[slot_];
    if (!b) return 0;
    memset(tokSpans_, 0, sizeof tokSpans_);
    Renderer r(tokText_, kTokText + 1, tokSpans_);
    r.setBreaks(true);
    int n = r.blockNode(p, b);
    if (n > kTokText) n = kTokText;
    for (int i = 0; i < n; i++) tokOwner_[i] = 0;
    tokOwners(b, n);
    const uint16_t e = p.at(b).first;
    const int eStart = e && tokSpans_[e].end > tokSpans_[e].start ? tokSpans_[e].start : n;
    int line = 0, col = 0, i = 0;
    auto add = [&](int start, int end, uint16_t node, uint8_t kind) {
        if (tokN_ >= kTokMax) return;
        Tok& t = toks_[tokN_++];
        t.start = (uint16_t)start;
        t.end = (uint16_t)end;
        t.node = node;
        t.kind = kind;
        t.line = (uint16_t)line;
        t.col = (uint16_t)col;
    };
    while (i < n) {
        const char ch = tokText_[i];
        if (ch == '\n') {
            line++;
            col = 0;
            i++;
            continue;
        }
        const uint16_t o = tokOwner_[i];
        const Kind k = o ? p.at(o).kind : Kind::None;
        if (ch == ' ' || !o || ch == ',' || ((ch == '(' || ch == ')') && exprContainer(k))) {
            col++;
            i++;
            continue;
        }
        if (k == Kind::Block) {  // "d1" / "jumpMod 3 4": the top, up to the expression
            int j = eStart;
            while (j > i && (tokText_[j - 1] == ' ' || tokText_[j - 1] == '$')) j--;
            if (j <= i) j = i + 1;
            add(i, j, o, kTokTop);
            col += eStart - i;
            i = eStart > i ? eStart : i + 1;
            continue;
        }
        int j = i + 1;
        if (k != Kind::Mini)  // a string's quotes are a token each
            while (j < n && tokOwner_[j] == o && tokText_[j] != ' ' && tokText_[j] != '\n' && tokText_[j] != ',' &&
                   !((tokText_[j] == '(' || tokText_[j] == ')') && exprContainer(k)))
                j++;
        const bool sym = k == Kind::Op || (k == Kind::App && (p.at(o).flags & kDollar));
        add(i, j, o, sym ? kTokSym : kTokNode);
        col += j - i;
        i = j;
    }
    tokLines_ = line + 1;
    return tokN_;
}

int Editor::tokenIndex() {
    if (!tokN_ || !cursor_) return -1;
    const bool sym = onOperator();
    int best = -1;
    for (int i = 0; i < tokN_; i++) {
        const Tok& t = toks_[i];
        if (t.node != cursor_ || (t.kind == kTokSym) != sym) continue;
        if (t.start == tokAt_) return i;
        if (best < 0) best = i;
    }
    return best;
}

void Editor::setToken(int i) {
    const Tok& t = toks_[i];
    cursor_ = t.node;
    onOp_ = t.kind == kTokSym;
    gapSeq_ = 0;
    tokAt_ = t.start;
}

int Editor::tokenNear(int line, int col) const {
    int best = -1, bestD = 0x7FFF;
    for (int i = 0; i < tokN_; i++) {
        const Tok& t = toks_[i];
        if (t.line != line) continue;
        const int a = t.col, z = t.col + (t.end - t.start);
        const int d = col < a ? a - col : col >= z ? col - z + 1 : 0;
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

// The character the gap cursor stands before (its bar).
static int gapChar(const Program& p, const Span* spans, uint16_t seq, int pos) {
    const int count = p.childCount(seq);
    if (pos < count) return spans[p.child(seq, pos)].start;
    return count ? spans[p.child(seq, count - 1)].end : spans[seq].start;
}

void Editor::cursorMove(int dir) {
    if (cps_ || (!cursor_ && !gapSeq_)) return navigate(dir < 0 ? Action::Left : Action::Right);
    if (!buildTokens()) return;
    int i = -1;
    if (gapSeq_) {
        const int g = gapChar(*prog_, tokSpans_, gapSeq_, gapPos_);
        if (dir > 0) {
            for (int k = 0; k < tokN_ && i < 0; k++)
                if (toks_[k].start >= g) i = k;
        } else {
            for (int k = tokN_ - 1; k >= 0 && i < 0; k--)
                if (toks_[k].end <= g) i = k;
        }
    } else {
        const int c = tokenIndex();
        i = c < 0 ? 0 : c + dir;
    }
    if (i >= 0 && i < tokN_) setToken(i);  // else the block's end: Left / Right stay in the block
    info(cursorInfo());
}

void Editor::cursorLine(int dir) {
    int col = 0;
    if (cps_) {
        if (dir < 0) return navigate(Action::Up);
        blockStep(1);
    } else if (!cursor_ && !gapSeq_) {
        blockStep(dir);
    } else {
        if (!buildTokens()) return;
        int line = 0;
        if (gapSeq_) {  // the gap's column
            const int g = gapChar(*prog_, tokSpans_, gapSeq_, gapPos_);
            for (int k = 0; k < g && k < kTokText; k++) {
                if (tokText_[k] == '\n') {
                    line++;
                    col = 0;
                } else {
                    col++;
                }
            }
        } else {
            const int c = tokenIndex();
            if (c >= 0) {
                line = toks_[c].line;
                col = toks_[c].col;
            }
        }
        const int to = line + dir;
        if (to >= 0 && to < tokLines_) {
            const int i = tokenNear(to, col);
            if (i >= 0) setToken(i);
            return info(cursorInfo());
        }
        const uint16_t was = prog_->block[slot_];
        const int slotWas = slot_;
        blockStep(dir);  // past the first / last line: the block above / below
        if (!cps_ && slot_ == slotWas && prog_->block[slot_] == was && emptyRowIndex() < 0 &&
            prog_->at(cursor_).kind != Kind::Block) {  // none: the line's start / end, as in a text editor
            buildTokens();
            if (tokN_) setToken(dir < 0 ? 0 : tokN_ - 1);
            return info(cursorInfo());
        }
    }
    // landed on a block's top: its last line (from below) or first line, same column
    if (!cps_ && cursor_ && prog_->at(cursor_).kind == Kind::Block && buildTokens()) {
        const int i = tokenNear(dir < 0 ? tokLines_ - 1 : 0, col);
        if (i >= 0) setToken(i);
    }
    info(cursorInfo());
}

void Editor::cursorSettle() {
    if (!cursorMode_ || menuOpen_ || kbdOpen_ || cps_ || gapSeq_ || selSeq_ || !cursor_) return;
    if (!buildTokens() || tokenIndex() >= 0) return;
    // on a node with no token of its own (an application, a whole operator
    // expression ...): its first token
    const Span sp = tokSpans_[cursor_];
    for (int i = 0; i < tokN_; i++)
        if (toks_[i].start >= sp.start && toks_[i].start < sp.end) return setToken(i);
}

uint16_t Editor::headOf(uint16_t n) const {
    if (!n || prog_->at(n).kind != Kind::Fn) return 0;
    const uint16_t par = prog_->parentOf(n);
    return par && prog_->at(par).kind == Kind::App && prog_->at(par).first == n ? par : 0;
}

void Editor::cursorDelete(bool back) {
    if (back && gapSeq_) return deleteAtGap();  // the bar: the step before it
    if (selSeq_) return doDelete();
    if (back) {
        if (!buildTokens()) return;
        const int c = tokenIndex();
        // never onto the block's top (Delete there clears the block)
        if (c <= 0 || toks_[c - 1].kind == kTokTop) return error("nothing to the left");
        setToken(c - 1);
    }
    if (!cursor_ || gapSeq_ || onOperator()) return doDelete();
    const Program& p = *prog_;
    const Kind k = p.at(cursor_).kind;
    if (const uint16_t app = headOf(cursor_)) {
        // a function's name: unwrap it (keep the pattern); a control behind an
        // operator (`# gain 1`, `|+ n 12`) goes with its operator
        const uint16_t par = p.parentOf(app);
        cursor_ = app;
        if (par && p.at(par).kind == Kind::Op && p.at(par).first != app) return deleteNode();
        if (tryUnwrap()) return;
        return deleteNode();
    }
    // a group's bracket: the brackets go, its steps stay where it was
    // (`[~ sd]` -> `~ sd`); a modifier's symbol (`*`, `?`, `(`): unwrap it
    if (k == Kind::MSub || k == Kind::MAlt || k == Kind::MPoly) {
        if (spliceGroup()) return;
    }
    if (k == Kind::MSub || k == Kind::MAlt || k == Kind::MPoly || k == Kind::MChoice ||
        (isMiniModifier(k) && k != Kind::MRange)) {
        if (tryUnwrap()) return;
    }
    doDelete();
}

// A one-layer group in a "..." sequence: its steps in its place.
bool Editor::spliceGroup() {
    const Program& p = *prog_;
    const uint16_t g = cursor_, par = p.parentOf(g);
    const Node& gn = p.at(g);
    const uint16_t layer = gn.first;
    if (!par || p.at(par).kind != Kind::MSeq || !layer || p.at(layer).next || p.at(layer).kind != Kind::MSeq ||
        !p.at(layer).first || (p.at(layer).flags & kDots) || (gn.kind == Kind::MPoly && (gn.flags & kHasSteps)))
        return false;
    if (!beginEdit()) return false;
    Program* q = prog_;
    const uint16_t first = q->at(layer).first;
    uint16_t last = first;
    while (q->at(last).next) last = q->at(last).next;
    const uint16_t prev = q->prevSibling(g);
    q->at(last).next = q->at(g).next;
    if (prev) q->at(prev).next = first;
    else q->at(par).first = first;
    q->at(layer).first = 0;
    q->at(g).next = 0;
    q->freeTree(g);
    if (!endEdit(first)) return true;  // refused: endEdit said why
    info("brackets removed");
    return true;
}

void Editor::cursorInsert() {
    if (gapSeq_ || !cursor_ || cps_) return doInsert();
    const Program& p = *prog_;
    const Kind k = p.at(cursor_).kind;
    if (k == Kind::Block) return doInsert();  // the top: a slot pad adds a block above
    if (isMiniKind(k) && k != Kind::Mini) {  // inside "...": a copy of the step after it
        cursor_ = stepOf(p, cursor_);
        const uint16_t par = p.parentOf(cursor_), gp = par ? p.parentOf(par) : 0;
        // a step of a sequence (not a modifier's argument, `hh*8`'s 8)
        if (par && p.at(par).kind == Kind::MSeq && !(gp && isMiniModifier(p.at(gp).kind))) return doInsert();
    }
    if (const uint16_t app = headOf(cursor_)) {  // a function's name: a function in front ($)
        cursor_ = app;
        return openWrap(WrapMode::Modifiers);
    }
    // anything else: a control behind the pattern (#)
    uint16_t t = controlTarget();
    if (t && p.at(t).kind == Kind::Block) t = p.at(t).first;
    if (t) cursor_ = t;
    onOp_ = false;
    openWrap(WrapMode::Fx);
}

}  // namespace tc
