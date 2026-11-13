// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
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
static uint16_t leafOf(const Program& p, uint16_t n) {
    while (n && p.at(n).first) n = p.at(n).first;
    return n;
}

void Editor::setCursorMode(bool on) {
    cursorMode_ = on;
    headGap_ = preSym_ = false;
    selA_ = selB_ = selSeq_ = 0;
    tokAt_ = 0xFFFF;
    cursorSettle();
}

void Editor::tokOwners(uint16_t n, int len) const {
    const Program& p = *prog_;
    const Span& sp = tokSpans_[n];
    if (sp.end > sp.start)
        for (int i = sp.start; i < sp.end && i < len; i++) tokOwner_[i] = n;
    for (uint16_t c = p.at(n).first; c; c = p.at(c).next) tokOwners(c, len);
}

// The gaps of the "..." sequences under n (steps, not a modifier's argument
// such as `bd*2`'s 2), in tree order.
void Editor::tokGaps(uint16_t n) const {
    const Program& p = *prog_;
    const Node& x = p.at(n);
    if (x.kind == Kind::MSeq && !(x.flags & kDots) && x.first) {
        const uint16_t par = p.parentOf(n);
        if (!par || !isMiniModifier(p.at(par).kind)) {
            const int count = p.childCount(n);
            for (int i = 0; i <= count && gapN_ < kGapMax; i++) {
                const uint16_t c = i < count ? p.child(n, i) : p.child(n, count - 1);
                const Span& sp = tokSpans_[c];
                if (sp.end <= sp.start) continue;
                gaps_[gapN_++] = Gap{i < count ? sp.start : sp.end, n, (uint16_t)i};
            }
        }
    }
    for (uint16_t c = x.first; c; c = p.at(c).next) tokGaps(c);
}

static bool exprContainer(Kind k) {
    return k == Kind::App || k == Kind::Op || k == Kind::SectionL || k == Kind::SectionR || k == Kind::Compose ||
           k == Kind::Block || k == Kind::List;
}

void Editor::tokRecord(uint16_t n) const {
    if (!n || tokUsedN_ >= kMaxNodes) return;
    tokUsed_[tokUsedN_++] = n;
    for (uint16_t c = prog_->at(n).first; c; c = prog_->at(c).next) tokRecord(c);
}

int Editor::buildTokens() const {
    const Program& p = *prog_;
    const int empty = emptyRowIndex();
    const uint16_t b = cps_ || empty >= 0 ? 0 : p.block[slot_];
    // the same code and block as last time: the tokens stand (no edit open)
    uint32_t key = 2166136261u;
    for (uint32_t v : {changes_, (uint32_t)slot_, (uint32_t)b, (uint32_t)cps_, (uint32_t)(empty + 1)})
        key = (key ^ v) * 16777619u;
    const bool open = p.checkpointOpen();
    if (tokValid_ && !open && key == tokKey_) return tokN_;
    tokValid_ = false;
    tokN_ = 0;
    tokLines_ = 0;
    for (int i = 0; i < tokUsedN_; i++) tokSpans_[tokUsed_[i]] = Span{0, 0};  // what the last render wrote
    tokUsedN_ = 0;
    if (!b) return 0;
    Renderer r(tokText_, kTokText + 1, tokSpans_);
    r.setBreaks(true);
    int n = r.blockNode(p, b);
    if (n > kTokText) n = kTokText;
    for (int i = 0; i < n; i++) tokOwner_[i] = 0;
    tokOwners(b, n);
    const uint16_t e = p.at(b).first;
    const int eStart = e && tokSpans_[e].end > tokSpans_[e].start ? tokSpans_[e].start : n;
    // the gaps between steps (user, RC11: the spaces in "..." are stops too),
    // by character; a gap goes before the token starting at its character
    gapN_ = 0;
    tokGaps(b);
    for (int a = 1; a < gapN_; a++)  // insertion sort: mostly in order already
        for (int k = a; k > 0 && gaps_[k - 1].at > gaps_[k].at; k--) {
            const Gap t = gaps_[k];
            gaps_[k] = gaps_[k - 1];
            gaps_[k - 1] = t;
        }
    int line = 0, col = 0, i = 0, gi = 0;
    auto add = [&](int start, int end, uint16_t node, uint8_t kind, uint16_t pos = 0) {
        if (tokN_ >= kTokMax) return;
        Tok& t = toks_[tokN_++];
        t.start = (uint16_t)start;
        t.end = (uint16_t)end;
        t.node = node;
        t.kind = kind;
        t.pos = pos;
        t.line = (uint16_t)line;
        t.col = (uint16_t)col;
    };
    while (i < n) {
        while (gi < gapN_ && gaps_[gi].at <= i) {
            add(gaps_[gi].at, gaps_[gi].at, gaps_[gi].seq, kTokGap, gaps_[gi].pos);
            gi++;
        }
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
            if (e && eStart < n) {  // the bar before the pattern (RC13 user: inserting a $ at the head)
                const int c0 = col;
                add(eStart, eStart, e, kTokHead);
                toks_[tokN_ - 1].col = (uint16_t)c0;
            }
            i = eStart > i ? eStart : i + 1;
            continue;
        }
        int j = i + 1;
        if (k != Kind::Mini)  // a string's quotes are a token each
            while (j < n && tokOwner_[j] == o && tokText_[j] != ' ' && tokText_[j] != '\n' && tokText_[j] != ',' &&
                   !((tokText_[j] == '(' || tokText_[j] == ')') && exprContainer(k)))
                j++;
        const bool sym = k == Kind::Op || (k == Kind::App && (p.at(o).flags & kDollar));
        if (sym) add(i, i, o, kTokPre);  // the bar before it: Insert puts a $ function / # control there
        add(i, j, o, sym ? kTokSym : kTokNode);
        col += j - i;
        i = j;
    }
    for (; gi < gapN_; gi++) add(gaps_[gi].at, gaps_[gi].at, gaps_[gi].seq, kTokGap, gaps_[gi].pos);
    tokLines_ = line + 1;
    tokRecord(b);  // the nodes whose spans this render wrote (cleared next time)
    tokValid_ = !open;
    tokKey_ = key;
    return tokN_;
}

int Editor::tokenIndex() const {
    if (headGap_) {
        for (int i = 0; i < tokN_; i++)
            if (toks_[i].kind == kTokHead) return i;
        return -1;
    }
    if (preSym_) {
        for (int i = 0; i < tokN_; i++)
            if (toks_[i].kind == kTokPre && toks_[i].node == cursor_) return i;
        return -1;
    }
    if (gapSeq_) {
        for (int i = 0; i < tokN_; i++)
            if (toks_[i].kind == kTokGap && toks_[i].node == gapSeq_ && toks_[i].pos == gapPos_) return i;
        return -1;
    }
    if (!tokN_ || !cursor_) return -1;
    const bool sym = onOperator();
    int best = -1;
    for (int i = 0; i < tokN_; i++) {
        const Tok& t = toks_[i];
        if (t.node != cursor_ || t.kind == kTokGap || t.kind == kTokHead || t.kind == kTokPre ||
            (t.kind == kTokSym) != sym)
            continue;
        if (t.start == tokAt_) return i;
        if (best < 0) best = i;
    }
    return best;
}

void Editor::setToken(int i) {
    const Tok& t = toks_[i];
    cursor_ = t.node;
    onOp_ = t.kind == kTokSym;
    gapSeq_ = t.kind == kTokGap ? t.node : 0;
    gapPos_ = t.pos;
    headGap_ = t.kind == kTokHead;
    preSym_ = t.kind == kTokPre;
    tokAt_ = t.start;
}

int Editor::tokenNear(int line, int col) const {
    int best = -1, bestD = 0x7FFF;
    for (int i = 0; i < tokN_; i++) {
        const Tok& t = toks_[i];
        if (t.line != line || t.kind == kTokGap || t.kind == kTokHead || t.kind == kTokPre) continue;  // tokens only
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
    cursorSettle();  // not on a token (an edit from outside a key): onto its first one first
    if (!buildTokens()) return;
    int i = -1;
    const int at = tokenIndex();
    if (at >= 0) {
        i = at + dir;
    } else if (gapSeq_) {  // a list gap (not a stop of its own): the tokens either side
        const int g = gapChar(*prog_, tokSpans_, gapSeq_, gapPos_);
        if (dir > 0) {
            for (int k = 0; k < tokN_ && i < 0; k++)
                if (toks_[k].start >= g) i = k;
        } else {
            for (int k = tokN_ - 1; k >= 0 && i < 0; k--)
                if (toks_[k].end <= g) i = k;
        }
    } else {
        i = 0;
    }
    if (i >= 0 && i < tokN_) setToken(i);  // else the block's end: Left / Right stay in the block
    info(cursorInfo());
}

void Editor::cursorLine(int dir) {
    int col = 0;
    cursorSettle();
    if (cps_) {
        if (dir < 0) return navigate(Action::Up);
        blockStep(1);
    } else if (!cursor_ && !gapSeq_) {
        blockStep(dir);
    } else {
        if (!buildTokens()) return;
        int line = 0;
        const int c0 = tokenIndex();
        if (c0 >= 0) {
            line = toks_[c0].line;
            col = toks_[c0].col;
        } else if (gapSeq_) {  // a list gap's column
            const int g = gapChar(*prog_, tokSpans_, gapSeq_, gapPos_);
            for (int k = 0; k < g && k < kTokText; k++) {
                if (tokText_[k] == '\n') {
                    line++;
                    col = 0;
                } else {
                    col++;
                }
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
    if (headGap_ || preSym_) return;
    if (!cursorMode_ || menuOpen_ || kbdOpen_ || cps_ || gapSeq_ || selSeq_ || !cursor_) return;
    if (!buildTokens() || tokenIndex() >= 0) return;
    // on a node with no token of its own (an application, a whole operator
    // expression ...): its first token
    const Span sp = tokSpans_[cursor_];
    for (int i = 0; i < tokN_; i++)
        if (toks_[i].start >= sp.start && toks_[i].start < sp.end && toks_[i].kind != kTokHead &&
            toks_[i].kind != kTokGap && toks_[i].kind != kTokPre)
            return setToken(i);
}

uint16_t Editor::headOf(uint16_t n) const {
    if (!n || prog_->at(n).kind != Kind::Fn) return 0;
    const uint16_t par = prog_->parentOf(n);
    return par && prog_->at(par).kind == Kind::App && prog_->at(par).first == n ? par : 0;
}

void Editor::cursorDelete(bool back) {
    if (headGap_) {  // the bar before the pattern: Delete acts on the token after it
        if (back) return error("nothing to the left");
        buildTokens();
        const int c = tokenIndex();
        if (c < 0 || c + 1 >= tokN_) return;
        setToken(c + 1);
    }
    if (preSym_) {  // the bar before `$` / `#`: Delete = the symbol (unwrap), Backspace = the token before
        buildTokens();
        const int c = tokenIndex();
        if (c < 0) return;
        if (back) {
            if (c == 0 || toks_[c - 1].kind == kTokTop || toks_[c - 1].kind == kTokHead)
                return error("nothing to the left");
            setToken(c - 1);
            if (gapSeq_) return deleteAtGap();
            back = false;  // now on the token before: delete it
        } else if (c + 1 < tokN_) {
            setToken(c + 1);
        }
    }
    if (back && gapSeq_) return deleteAtGap();  // the bar: the step before it
    if (!back && gapSeq_ && !listGap()) {     // the bar: the token after it (a step: ~)
        buildTokens();
        const int c = tokenIndex();
        if (c < 0 || c + 1 >= tokN_ || toks_[c + 1].kind == kTokGap || toks_[c + 1].start != toks_[c].start)
            return error("nothing to the right in this sequence (Shift+Delete: the step before)");
        setToken(c + 1);
    }
    if (selSeq_) return doDelete();
    if (back) {
        if (!buildTokens()) return;
        const int c = tokenIndex();
        // never onto the block's top (Delete there clears the block)
        if (c <= 0 || toks_[c - 1].kind == kTokTop || toks_[c - 1].kind == kTokHead) return error("nothing to the left");
        setToken(c - 1);
    }
    if (headGap_) return;  // nothing after the bar (an empty pattern can't be)
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
    if (headGap_) {  // before the pattern: a $ function in front of all of it
        headGap_ = false;
        return openWrap(WrapMode::Modifiers);
    }
    if (preSym_) {  // before `$`: a function on what follows it; before an operator: # on its left side
        preSym_ = false;
        const Program& q = *prog_;
        const Node& n = q.at(cursor_);
        if (n.kind == Kind::App) {
            uint16_t last = n.first;
            while (last && q.at(last).next) last = q.at(last).next;
            if (last) cursor_ = last;
            return openWrap(WrapMode::Modifiers);
        }
        if (n.first) cursor_ = n.first;
        return openWrap(WrapMode::Fx);
    }
    if (gapSeq_ && !listGap()) {  // a gap in "...": a copy of the step before it (the first: after it)
        const uint16_t seq = gapSeq_;
        const uint16_t src = prog_->child(seq, gapPos_ > 0 ? gapPos_ - 1 : 0);
        if (!src) return;
        if (!beginEdit()) return;
        Program* q = prog_;
        const uint16_t c = q->copyTree(*q, src);
        if (!c) return abortEdit(), error("out of nodes");
        if (!linkAtGap(*q, c, seq, gapPos_)) return abortEdit(), error("can't insert here");
        gapSeq_ = 0;
        if (endEdit(c)) info("inserted a copy (jog changes it)");
        return;
    }
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

// Step marks (RC14 / RC19 notes, Shift+Screen 1-4, 7): the step at the
// cursor (or before the gap) as x*2 / x/2 / x!2 / x@2 / x?0.5. Again on
// such a step: * / / / ! count one more; @ / ? go to their number. The
// cursor lands where the jog changes the number (both modes).
void Editor::stepSuffix(char op) {
    const Program& p = *prog_;
    const uint16_t step = keyStep();
    const uint16_t seq = step ? p.parentOf(step) : 0;
    Text mark = Text(&op, 1);
    if (!step) return error(mark + " goes on a step inside \"...\"");
    const Node& st = p.at(step);
    const Kind want = op == '!' ? Kind::MReplicate : op == '@' ? Kind::MWeight : op == '*' ? Kind::MFast
                    : op == '/' ? Kind::MSlow : Kind::MDegrade;
    const uint16_t arg = st.first ? p.at(st.first).next : 0;  // the mark's number (not for !)
    if (st.kind == want) {
        gapSeq_ = 0;
        if (op == '!' && !(st.flags & kBare)) {
            if ((int)st.a >= kMaxSeqSteps) return error("! counts go up to 64");
            if (!beginEdit()) return;
            prog_->at(step).a++;
            Label m;
            if (endEdit(step)) info(m.printf("!%u (jog changes it)", (unsigned)prog_->at(step).a));
            return;
        }
        const uint16_t num = arg ? leafOf(p, arg) : 0;
        if ((op == '*' || op == '/') && num && p.at(num).kind == Kind::MNum) {  // one more
            if (!beginEdit()) return;
            Node& nd = prog_->at(num);
            const Rational r = nodeRational(nd);
            if (r.den != 1) return abortEdit(), (void)(cursor_ = num), info(mark + ": jog changes it");
            setNodeRational(nd, makeRational((int64_t)r.num + 1, 1), 0);
            Label m;
            if (endEdit(num)) info(m.printf("%c%d (jog changes it)", op, (int)r.num + 1));
            return;
        }
        if (num && (op == '@' || op == '?' || op == '*' || op == '/')) {
            cursor_ = num;
            return info(mark + ": jog changes it");
        }
    }
    int idx = 0;
    for (uint16_t c = p.at(seq).first; c && c != step; c = p.at(c).next) idx++;
    Text code = joinSteps(seq, idx, 1, " ");
    code += op == '?' ? "?0.5" : mark + "2";
    if (code.overflowed()) return error("step too long");
    gapSeq_ = 0;
    const uint16_t was = cursor_;
    cursor_ = step;
    uint16_t nn;
    EditSpec sp;
    if (!tryReplace(code, true, nn, &sp) || !doSpec(sp, nn)) {
        cursor_ = was;
        return error(code + " doesn't fit here");
    }
    const Node& r = prog_->at(nn);
    if (op != '!' && r.first && prog_->at(r.first).next) cursor_ = leafOf(*prog_, prog_->at(r.first).next);
    else cursor_ = nn;
    info(code + ": jog changes the number");
}

bool Editor::cursorBarChar(int& at) const {
    if (!cursorMode_ || cps_ || !(headGap_ || preSym_) || !cursor_ || !buildTokens()) return false;
    const int c = tokenIndex();
    if (c < 0) return false;
    at = toks_[c].start;
    return true;
}

bool Editor::cursorTokenChars(int& a, int& b, int& pa, int& pb) const {
    if (!cursorMode_ || cps_ || gapSeq_ || headGap_ || preSym_ || selSeq_ || !cursor_ || onOperator()) return false;
    if (!buildTokens()) return false;
    const int c = tokenIndex();
    if (c < 0) return false;
    const Tok& t = toks_[c];
    if (t.kind != kTokTop && !(t.kind == kTokNode && prog_->at(t.node).first)) return false;  // a leaf: the node
    a = t.start;
    b = t.end;
    pa = pb = 0;
    int best = -1;  // the partner: the nearest other token of the same node
    for (int i = 0; i < tokN_; i++)
        if (i != c && toks_[i].node == t.node && toks_[i].kind == kTokNode &&
            (best < 0 || (i > c ? i - c : c - i) < (best > c ? best - c : c - best)))
            best = i;
    if (best >= 0) {
        pa = toks_[best].start;
        pb = toks_[best].end;
    }
    return true;
}

// The step the step keys act on: at the cursor (its whole step: `bd` of
// bd*2), or the one before the gap (at the start: after it); 0 if none.
uint16_t Editor::keyStep() const {
    const Program& p = *prog_;
    uint16_t step = 0;
    if (gapSeq_ && !listGap()) step = p.child(gapSeq_, gapPos_ > 0 ? gapPos_ - 1 : 0);
    else if (cursor_ && isMiniKind(p.at(cursor_).kind) && p.at(cursor_).kind != Kind::MSeq) step = stepOf(p, cursor_);
    uint16_t seq = step ? p.parentOf(step) : 0;
    // inside a mark's number (the 2 of bd*2): the marked step itself
    for (uint16_t mod = seq ? p.parentOf(seq) : 0; mod && isMiniModifier(p.at(mod).kind) && p.at(mod).first != seq;
         mod = seq ? p.parentOf(seq) : 0) {
        step = stepOf(p, mod);
        seq = p.parentOf(step);
    }
    return seq && p.at(seq).kind == Kind::MSeq ? step : 0;
}

// Shift+Screen 5 `,` (RC19 notes): another layer. In a group ([..] <..>
// {..}): a new layer holding a copy of the step (in {..}%n the %n goes: the
// first layer sets the steps, a polyrhythm); a plain step: a chord with a
// copy, [x, x]. The cursor on the copy (pads / jog change it).
void Editor::stepStack() {
    const uint16_t step = keyStep();
    if (!step) return error(", goes on a step inside \"...\"");
    const Program& p = *prog_;
    const uint16_t seq = p.parentOf(step), grp = p.parentOf(seq);
    const Kind gk = grp ? p.at(grp).kind : Kind::None;
    gapSeq_ = 0;
    if (gk == Kind::MSub || gk == Kind::MAlt || gk == Kind::MPoly) {
        if (!beginEdit()) return;
        Program* q = prog_;
        const uint16_t c = q->copyTree(*q, step), lay = c ? q->alloc(Kind::MSeq) : 0;
        if (!lay) return abortEdit(), error("out of nodes");
        q->appendChild(lay, c);
        bool poly = false;
        if (gk == Kind::MPoly && (q->at(grp).flags & kHasSteps)) {  // drop the %n
            uint16_t last = q->at(grp).first, prev = 0;
            while (q->at(last).next) prev = last, last = q->at(last).next;
            if (prev) q->at(prev).next = 0;
            else q->at(grp).first = 0;
            q->freeTree(last);
            q->at(grp).flags &= (uint16_t)~kHasSteps;
            poly = true;
        }
        q->appendChild(grp, lay);
        if (endEdit(c))
            info(poly ? ", a new layer (the %n went: the first layer sets the steps)" : ", a new layer: pads / jog change it");
        return;
    }
    int idx = 0;
    for (uint16_t c = p.at(seq).first; c && c != step; c = p.at(c).next) idx++;
    const Text t = joinSteps(seq, idx, 1, " ");
    const Text code = "[" + t + ", " + t + "]";
    if (code.overflowed()) return error("step too long");
    cursor_ = step;
    uint16_t nn;
    EditSpec sp;
    if (!tryReplace(code, true, nn, &sp) || !doSpec(sp, nn)) return error(code + " doesn't fit here");
    // the second layer's step
    uint16_t lay2 = prog_->at(nn).first ? prog_->at(prog_->at(nn).first).next : 0;
    cursor_ = lay2 ? leafOf(*prog_, lay2) : nn;
    info(code + ": a chord, pads / jog change the copy");
}

// Shift+Screen 6 `""` (RC19 notes): a plain number (`n 0`, `fast 2`) as a
// pattern, `n "0"`, so steps can be added; the cursor on its step.
void Editor::toPattern() {
    if (!cursor_ || cps_) return error("\"\": on a number");
    const Kind k = prog_->at(cursor_).kind;
    if (isMiniKind(k) || k == Kind::Mini || gapSeq_) return error("\"\": already a pattern");
    if (k != Kind::Num) return error("\"\": on a number (it becomes \"...\")");
    Text t = renderNode(*prog_, cursor_);
    if (t.size() > 2 && t[0] == '(' && t.back() == ')') t = t.substr(1, t.size() - 2);  // (-1)
    if (t.find('/') != Text::npos) return error("a fraction can't go inside \"...\"; jog it first");
    uint16_t nn;
    EditSpec sp;
    if (!tryReplace("\"" + t + "\"", false, nn, &sp) || !doSpec(sp, nn)) return error("\"" + t + "\" doesn't fit here");
    cursor_ = leafOf(*prog_, nn);
    info("\"" + t + "\": pads add steps after it");
}

// Shift+Screen 7 (RC19 notes): on a step `?` (maybe), at a gap between two
// numbers `..` (a range), elsewhere \n (a line break).
Label Editor::shift7Label() const {
    if (gapSeq_ && !listGap()) return "..";
    if (!gapSeq_ && cursor_ && keyStep()) return "?";
    return "\\n";  // a line break (user: \\n, like the code it adds)
}

void Editor::shift7() {
    if (gapSeq_ && !listGap()) {
        const Program& p = *prog_;
        const int count = p.childCount(gapSeq_);
        const uint16_t a = gapPos_ > 0 ? p.child(gapSeq_, gapPos_ - 1) : 0, b = gapPos_ < count ? p.child(gapSeq_, gapPos_) : 0;
        if (!a || !b || p.at(a).kind != Kind::MNum || p.at(b).kind != Kind::MNum)
            return error("..: at a gap between two numbers (0 7 -> 0 .. 7)");
        EditSpec sp;
        sp.kind = EditKind::ReplaceRun;
        sp.mini = true;
        sp.seq = gapSeq_;
        sp.pos = gapPos_ - 1;
        sp.cycles = 2;
        sp.code = joinSteps(gapSeq_, gapPos_ - 1, 2, " .. ");
        uint16_t nn;
        if (!doSpec(sp, nn)) return error(sp.code + " doesn't fit here");
        cursor_ = nn;
        return info(sp.code);
    }
    if (cursor_ && keyStep()) return stepSuffix('?');
    toggleBreak();
}

}  // namespace tc
