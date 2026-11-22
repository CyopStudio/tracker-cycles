// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Edits on the tree: line breaks, quick controls, copy / paste, gap insert /
// delete, delete and unwrap.
#include "editor_impl.h"
#include "cold.h"

TC_COLD_BEGIN
namespace tc {

static uint16_t firstValue(const Program& p, uint16_t n);

// Shift+Screen 7 (\n): a line break (on / off) before the argument at the cursor,
// or before the operator / $ it belongs to (UI notes, Editor14).
void Editor::toggleBreak() {
    if (cps_ || !cursor_ || prog_->at(cursor_).kind == Kind::Block)
        return error("a line break goes before an argument or an operator of a block");
    const Program& p = *prog_;
    const uint16_t n = cursor_;
    const Kind k = p.at(n).kind;
    if (isMiniKind(k)) return error("no line breaks inside \"...\" (on the quotes: before the whole string)");
    uint16_t target = 0, flag = 0;
    const uint16_t par = p.parentOf(n);
    const Kind pk = par ? p.at(par).kind : Kind::None;
    if (onOperator() || k == Kind::Op || k == Kind::Compose) {
        target = n;  // an operator expression: before its own symbol
        flag = kBreakOp;
    } else if (pk == Kind::Op || pk == Kind::Compose) {
        target = par;  // an operand: before the operator's symbol
        flag = kBreakOp;
    } else if (pk == Kind::App && p.at(par).first != n) {
        const bool lastDollar = (p.at(par).flags & kDollar) && !p.at(n).next;
        target = lastDollar ? par : n;  // after $: before the $
        flag = lastDollar ? kBreakOp : kBreak;
    } else if ((k == Kind::App && (p.at(n).flags & kDollar)) || k == Kind::Op || k == Kind::Compose) {
        target = n;
        flag = kBreakOp;
    } else {
        return error("a line break goes before an argument or an operator");
    }
    const bool wasOp = onOperator();
    if (!beginEdit()) return;
    prog_->at(target).flags ^= flag;
    const bool on = prog_->at(target).flags & flag;
    if (!endEdit(n, false)) return;
    onOp_ = wasOp;
    info(on ? "line break (Shift+Screen 7 again removes it)" : "line break removed");
}

bool Editor::operatorSides(uint16_t& l, uint16_t& r) const {
    l = r = 0;
    if (!onOperator()) return false;
    const Program& p = *prog_;
    const Node& n = p.at(cursor_);
    if (n.kind == Kind::Op) {
        l = n.first;
        r = l ? p.at(l).next : 0;
    } else {  // App with $: the pattern is the last child
        for (uint16_t c = n.first; c; c = p.at(c).next) {
            if (!p.at(c).next) r = c;
            else l = c;
        }
    }
    return l && r;
}

// Shift+Screen 1-3 (UI v3, user's choice): `# gain 0.9`, `# room 0.3`,
// `# delay 0.3` on the pattern around the cursor (out of a "..." string or
// an argument until it fits); the cursor lands on the new value for the jog.
uint16_t Editor::controlTarget() const {
    const Program& p = *prog_;
    uint16_t t = cursor_;
    if (p.at(t).kind == Kind::Block) t = p.at(t).first;
    while (t && (isMiniKind(p.at(t).kind) || p.at(t).kind == Kind::Mini)) t = p.parentOf(t);
    // out of a function's arguments (the 0.3 of `room 0.3`), then to the top
    // of the operator chain it is in: the control goes at the chain's end
    // (`s "bd" # gain 1 # room 0.3`, not `s "bd" # (room 0.3 ...)`)
    for (;;) {
        const uint16_t up = t ? p.parentOf(t) : 0;
        if (!up) break;
        const Node& un = p.at(up);
        const bool arg = un.kind == Kind::App && un.first != t && !((un.flags & kDollar) && !p.at(t).next);
        if (arg || un.kind == Kind::Op || un.kind == Kind::Tuple || un.kind == Kind::List) t = up;
        else break;
    }
    return t;
}

// Screen 2 / FX2 (UI notes 2026-11-22): from inside a control (its name, its
// value, its "...") the new `# ...` goes straight after that control: out of
// whatever isn't a control pattern yet (the 0.3 of `room 0.3`, the "0" of
// `n ("0" + "<0 3>")`), then past the operator it is the right side of
// (`a # room 0.3 # new`, not `a # (room 0.3 # new)`). At a block's end that
// appends. A function value (often's `rev`) stays: its menus replace it.
uint16_t Editor::controlAfter(uint16_t n) const {
    const Program& p = *prog_;
    if (!n || p.at(n).kind == Kind::Block || p.at(n).type == kTyFn) return n;
    const uint8_t ctl = kTyPatBase + (int)Base::Control;
    uint16_t t = n;
    while (p.at(t).type != ctl) {
        const uint16_t up = p.parentOf(t);
        if (!up || p.at(up).kind == Kind::Block) return n;  // no control around it: as before
        t = up;
    }
    // only out of a control (`room 0.3`, `n (...)`), not out of another
    // function's argument (the 2 of `hurry 2 $ ...` stays as it was)
    if (t != n && p.at(t).kind == Kind::App) {
        const uint16_t h = p.at(t).first;
        if (!h || p.at(h).kind != Kind::Fn || lib((int)p.at(h).a).cat != Cat::Control) return n;
    }
    const uint16_t up = p.parentOf(t);
    if (up && p.at(up).kind == Kind::Op && p.at(up).first != t) t = up;
    return t;
}

// The end of the current block's pattern: past every `$` (`every 2 (fast 2)
// $ s "bd" # gain 1` ends with `s "bd" # gain 1`).
uint16_t Editor::blockEnd() const {
    const Program& p = *prog_;
    const uint16_t b = prog_->block[slot_];
    uint16_t t = b ? p.at(b).first : 0;
    while (t && p.at(t).kind == Kind::App && (p.at(t).flags & kDollar)) {
        uint16_t last = p.at(t).first;
        while (last && p.at(last).next) last = p.at(last).next;
        if (!last) break;
        t = last;
    }
    return t;
}

void Editor::quickControl(const char* rhs) {
    if (cps_ || !cursor_) return error("go to a block's pattern first");
    const Program& p = *prog_;
    uint16_t t = controlTarget();
    for (; t && p.at(t).kind != Kind::Block; t = p.parentOf(t)) {
        EditSpec sp = spec(EditKind::WrapOp, t, rhs);
        sp.op = "#";
        sp.left = true;
        uint16_t nn;
        if (!trySpec(sp, nn)) continue;
        if (!doSpec(sp, nn)) return;
        if (const uint16_t v = addedValue(sp, nn)) cursor_ = v;
        gapSeq_ = 0;
        onOp_ = false;
        Text c = rhs;
        lastControl_ = c.c_str();
        return info("# " + c + ": jog changes it");
    }
    error(Text("# ") + rhs + " doesn't fit here");
}

// Copy at a block's top: a copy of the block below it (UI notes, Editor14).
void Editor::duplicateBlock() {
    const uint16_t b = prog_->block[slot_];
    if (!b || cursor_ != b) return;
    if (!beginEdit()) return;
    Program* p = prog_;
    const uint16_t c = p->copyTree(*p, b);
    if (!c || !p->insertBlock(p->blockIndex(b) + 1, c)) {
        abortEdit();
        return error("too many blocks (max " + tnum(kMaxBlocks) + ")");
    }
    p->block[slot_] = c;
    if (!endEdit(c, false)) return;
    info("block duplicated (the copy is selected; Up / Down: the blocks)");
}

void Editor::doCopy() {
    int selPos;
    if (selRange(selPos)) {  // the run, as one step
        clip_ = "[";
        clip_ += selText();
        clip_ += ']';
        clipMini_ = true;
        clipOp_.clear();
        return info("copied " + clip_);
    }
    if (!cursor_) return error("nothing to copy");
    if (prog_->at(cursor_).kind == Kind::Block) return duplicateBlock();
    uint16_t n = cursor_;
    if (!n) return error("nothing to copy");
    clipOp_.clear();
    if (keyOnOp_) {  // on `#` (UI notes 2026-11-22): the whole control, `# gain 0.9`
        const uint16_t l = prog_->at(n).first, r = l ? prog_->at(l).next : 0;
        if (r) {
            clip_ = renderNode(*prog_, r);
            clipMini_ = false;
            clipOp_ = lib((int)prog_->at(n).a).name;
            return info("copied " + clipOp_ + " " + clip_ + " (Paste: on a # replaces it, elsewhere adds it at the end)");
        }
    }
    clip_ = renderNode(*prog_, n);
    clipMini_ = isMiniKind(prog_->at(n).kind);
    info("copied " + clip_);
}

// A copied operator with its right side (`# gain 0.9`): on an operator (its
// symbol, the bar before it) it takes that one's place, symbol and right
// side; anywhere else it goes on the end of the block (UI notes 2026-11-22).
void Editor::pasteOperator() {
    const Text what = Text(clipOp_.c_str()) + " " + clip_;
    const int id = libFind(clipOp_.c_str(), (int)clipOp_.size());
    if (id < 0 || cps_) return error("can't paste " + what + " here");
    if (keyOnOp_) {
        const uint16_t o = cursor_;
        const uint16_t l = prog_->at(o).first, r = l ? prog_->at(l).next : 0;
        if (!r || !beginEdit()) return;
        Program& P = *prog_;
        Error e;
        const uint16_t n = parseExpr(P, clip_.c_str(), (int)clip_.size(), e);
        if (!n) {
            abortEdit();
            return error("can't paste " + what + " here");
        }
        P.replace(r, n);
        P.freeTree(r);
        P.at(o).a = (uint32_t)id;
        uint16_t v = firstValue(P, n);
        if (!endEdit(v ? v : n)) return;  // endEdit type-checks (and says why not)
        onOp_ = preSym_ = false;
        return info("replaced with " + what);
    }
    // the end of the block, else further out until it fits (as the quick controls)
    for (uint16_t t = blockEnd(); t && prog_->at(t).kind != Kind::Block; t = prog_->parentOf(t)) {
        EditSpec sp = spec(EditKind::WrapOp, t, clip_);
        sp.op = clipOp_.c_str();
        sp.left = true;
        uint16_t nn;
        if (!trySpec(sp, nn)) continue;
        if (!doSpec(sp, nn)) return;
        if (const uint16_t v = addedValue(sp, nn)) cursor_ = v;
        gapSeq_ = 0;
        onOp_ = false;
        return info("pasted " + what + " at the end of the block");
    }
    error("can't paste " + what + " into this block");
}

// Replace the selection with the clipboard; a copied step can also go where
// a pattern is expected (as "step"). Type-checked like every other edit.
void Editor::doPaste() {
    if (clip_.empty()) return error("clipboard is empty (Copy first)");
    if (listGap()) {
        uint16_t nn;
        EditSpec sp;
        if (!tryInsertListAt(clipMini_ ? "\"" + clip_ + "\"" : clip_, nn, &sp))
            return error("can't paste " + clip_ + " into this list");
        gapSeq_ = 0;
        if (doSpec(sp, nn)) info("pasted " + clip_ + " into the list");
        return;
    }
    if (!clipOp_.empty()) {
        if (!prog_->block[slot_]) return error("paste " + clipOp_ + " " + clip_ + " into a block");
        return pasteOperator();
    }
    uint16_t beforeCursor = cursor_;
    const bool hadBlock = prog_->block[slot_] != 0;
    ensureBlock();
    if (!cursor_) return;
    bool targetMini = isMiniKind(prog_->at(cursor_).kind);
    uint16_t nn;
    EditSpec sp;
    bool ok = false;
    if (targetMini == clipMini_) ok = tryReplace(clip_, clipMini_, nn, &sp);
    else if (clipMini_) ok = tryReplace("\"" + clip_ + "\"", false, nn, &sp);
    if (!ok) {
        if (!hadBlock) {  // ensureBlock added an empty block: take it out again
            const uint16_t b = prog_->block[slot_];
            prog_->removeBlockAt(prog_->blockIndex(b));
            prog_->freeTree(b);
            prog_->block[slot_] = 0;
        }
        cursor_ = beforeCursor;
        return error("can't paste " + clip_ + " here");
    }
    if (doSpec(sp, nn)) info("pasted " + clip_);
}

// ---------------------------------------------------------------------------
// context buttons

uint16_t ed::firstNumber(const Program& p, uint16_t n) {
    if (!n) return 0;
    Kind k = p.at(n).kind;
    if (k == Kind::Num || k == Kind::MNum) return n;
    for (uint16_t c = p.at(n).first; c; c = p.at(c).next)
        if (uint16_t f = firstNumber(p, c)) return f;
    return 0;
}

// The first value (number, note, step) under n in text order, skipping
// function names.
static uint16_t firstValue(const Program& p, uint16_t n) {
    if (!n) return 0;
    const Kind k = p.at(n).kind;
    if (k == Kind::Num || k == Kind::MNum || k == Kind::MNote || k == Kind::MWord || k == Kind::MBool ||
        k == Kind::MChord)
        return n;
    if (k == Kind::Fn) return 0;
    for (uint16_t c = p.at(n).first; c; c = p.at(c).next)
        if (uint16_t f = firstValue(p, c)) return f;
    return 0;
}

// After a wrap: the first value of the part that was added (the control's
// value of `# gain 1`, the 2 of `fast 2 $ x`), or 0.
uint16_t Editor::addedValue(const EditSpec& s, uint16_t nn) const {
    const Program& p = *prog_;
    if (!nn) return 0;
    if (s.kind == EditKind::WrapOp && p.at(nn).kind == Kind::Op) {
        const uint16_t l = p.at(nn).first, r = l ? p.at(l).next : 0;
        return firstValue(p, s.left ? r : l);
    }
    if (s.kind == EditKind::WrapApp && p.at(nn).kind == Kind::App) {
        // the arguments before the wrapped pattern (the last child)
        for (uint16_t c = p.at(p.at(nn).first).next; c && p.at(c).next; c = p.at(c).next)
            if (uint16_t v = firstValue(p, c)) return v;
    }
    return 0;
}

static uint16_t childAt(const Program& p, uint16_t n, int i) {
    uint16_t c = p.at(n).first;
    while (c && i-- > 0) c = p.at(c).next;
    return c;
}

static uint16_t lastChild(const Program& p, uint16_t n) {
    uint16_t c = p.at(n).first;
    while (c && p.at(c).next) c = p.at(c).next;
    return c;
}

Label Editor::quickLabel(int i) const {
    if (i < 0 || i > 3 || menuOpen_ || !cursor_) return "";
    Kind k = prog_->at(cursor_).kind;
    if (isMiniKind(k) || k == Kind::Mini) {  // a step, or the whole "..." sequence
        static const char* m[4] = {"[ ]", "< >", "{ }%4", "(3,8)"};
        return m[i];
    }
    if (k == Kind::Block) return "";
    if (i == 0) return "# " + lastControl_.substr(0, lastControl_.find(' '));
    static const char* o[4] = {"", "|+", "|*", "<~"};
    return o[i];
}

void Editor::quick(int i) {
    Text label = quickLabel(i);
    if (label.empty()) return error("select a pattern first");
    Kind k = prog_->at(cursor_).kind;
    uint16_t nn = 0, target = 0;
    EditSpec sp;
    bool ok = false;
    const uint16_t oldCursor = cursor_;
    if (k == Kind::Mini) {
        // on the quotes: wrap the whole sequence inside them
        cursor_ = prog_->at(cursor_).first;
        k = cursor_ ? prog_->at(cursor_).kind : Kind::None;
        if (!isMiniKind(k)) {
            cursor_ = oldCursor;
            return error(label + " doesn't fit here");
        }
    }
    if (isMiniKind(k)) {
        Text t = renderNode(*prog_, cursor_);
        bool multi = k == Kind::MSeq && prog_->at(cursor_).first && prog_->at(prog_->at(cursor_).first).next;
        Text step = multi ? "[" + t + "]" : t;  // modifiers bind to one step
        static const char* pre[4] = {"[", "<", "{", ""};
        static const char* post[4] = {"]", ">", "}%4", "(3,8)"};
        Text code = Text(pre[i]) + (i == 3 ? step : t) + post[i];
        sp = spec(EditKind::Replace, cursor_, code);
        sp.mini = true;
        ok = openSpec(sp, nn);
        if (ok) {
            const Program* r = prog_;  // the open edit
            uint16_t n = nn;
            while (r->at(n).kind == Kind::MSeq && r->at(n).first && !r->at(r->at(n).first).next) n = r->at(n).first;
            if (r->at(n).kind == Kind::MPoly) target = firstNumber(*r, lastChild(*r, n));
            else if (r->at(n).kind == Kind::MEuclid) target = firstNumber(*r, childAt(*r, n, 1));
            nn = n;  // the new bracket / euclid step, not a transparent sequence
        }
    } else {
        // operator wraps; the cursor lands on the new number so the jog edits it
        struct { const char* op; const char* rhs; const char* bare; bool left; } ops[4] = {
            {"#", lastControl_.c_str(), nullptr, true},
            {"|+", "n 12", "12", true},
            {"|*", "speed 2", "2", true},
            {"<~", "0.25", nullptr, false},
        };
        auto& o = ops[i];
        sp = spec(EditKind::WrapOp, cursor_, o.rhs);
        sp.op = o.op;
        sp.left = o.left;
        ok = openSpec(sp, nn);
        if (!ok && o.bare) {  // plain value patterns
            sp.code = o.bare;
            ok = openSpec(sp, nn);
        }
        if (ok) target = firstNumber(*prog_, o.left ? childAt(*prog_, nn, 1) : childAt(*prog_, nn, 0));
    }
    if (!ok) {
        cursor_ = oldCursor;
        return error(label + " doesn't fit here");
    }
    if (endEdit(target ? target : nn)) info(label + (target ? ": jog changes the number" : ""));
}

// ---------------------------------------------------------------------------
// gap cursor (v2 step 5)

void Editor::leaveGap() {
    if (!gapSeq_) return;
    cursor_ = gapSeq_;  // the whole sequence
    gapSeq_ = 0;
}

bool Editor::tryInsertAtGap(const Text& code, uint16_t& newStep, EditSpec* out) {
    EditSpec s = spec(EditKind::InsertGap, 0, code);
    s.seq = gapSeq_;
    s.pos = gapPos_;
    if (!trySpec(s, newStep)) return false;
    if (out) *out = s;
    return true;
}

bool Editor::listGap() const { return gapSeq_ && prog_->at(gapSeq_).kind == Kind::List; }

bool Editor::linkAtGap(Program& p, uint16_t node, uint16_t seq, int pos) const {
    if (!seq) return false;
    if (pos == 0) {
        p.at(node).next = p.at(seq).first;
        p.at(seq).first = node;
        return true;
    }
    const uint16_t prev = p.child(seq, pos - 1);
    if (!prev) return false;
    p.at(node).next = p.at(prev).next;
    p.at(prev).next = node;
    return true;
}

// A list gap: `code` (an expression) as a new element there, type-checked.
bool Editor::tryInsertListAt(const Text& code, uint16_t& newEl, EditSpec* out) {
    EditSpec s = spec(EditKind::InsertList, 0, code);
    s.seq = gapSeq_;
    s.pos = gapPos_;
    if (!trySpec(s, newEl)) return false;
    if (out) *out = s;
    return true;
}

// Insert at a list gap: a copy of the element before it (else the first),
// cursor on the copy.
void Editor::insertListCopy() {
    if (!listGap()) return;
    const uint16_t src = gapPos_ > 0 ? prog_->child(gapSeq_, gapPos_ - 1) : prog_->child(gapSeq_, 0);
    if (!src) return error("empty list");
    if (!beginEdit()) return;
    Program* p = prog_;
    const uint16_t c = p->copyTree(*p, src);
    if (!c) return abortEdit(), error("out of nodes");
    if (!linkAtGap(*p, c, gapSeq_, gapPos_)) return abortEdit(), error("can't insert here");
    gapSeq_ = 0;
    if (endEdit(c)) info("inserted a copy into the list");
}

// Pads at a gap: insert the step and stay in the gap after it, so several
// pads insert several steps.
void Editor::insertAtGap(const Text& code, const Label& label) {
    if (code.empty()) return error(label + " only goes inside \"...\"");
    const uint16_t seq = gapSeq_;
    const int pos = gapPos_;
    uint16_t nn;
    EditSpec sp;
    if (!tryInsertAtGap(code, nn, &sp)) return error(label + " doesn't fit here");
    if (!doSpec(sp, nn)) return;
    gapSeq_ = seq;
    gapPos_ = pos + 1;
    cursor_ = seq;
    info("+ " + label + "  (gap: more pads insert more)");
}

// Delete at a gap: remove the step before it (Backspace).
void Editor::deleteAtGap() {
    if (gapPos_ == 0) return error(listGap() ? "start of the list: nothing before the gap"
                                             : "start of the sequence: nothing before the gap");
    const uint16_t seq = gapSeq_;
    const int pos = gapPos_;
    const bool list = listGap();
    if (prog_->childCount(seq) < 2)
        return error(list ? "the last element: a list keeps one" : "the last step: Delete on it makes a rest");
    if (!beginEdit()) return;
    Program* p = prog_;
    const uint16_t target = p->child(seq, pos - 1);
    if (pos - 1 == 0) p->at(seq).first = p->at(target).next;
    else p->at(p->child(seq, pos - 2)).next = p->at(target).next;
    p->at(target).next = 0;
    p->freeTree(target);
    if (!endEdit(seq)) return;
    gapSeq_ = seq;
    gapPos_ = pos - 1;
    cursor_ = seq;
    info(list ? "removed the element before the gap" : "removed the step before the gap");
}

// Delete inside "...": a step becomes a rest (timing stays), a rest is
// removed, and the step pad entry just appended is removed (ends entry).
void Editor::doDelete() {
    if (onOperator()) {  // on `#`, `|+`, `$` ...: unwrap (UI v3 tree refinement)
        if (!tryUnwrap()) return error("can't unwrap this");
        onOp_ = false;
        return;
    }
    int selPos;
    const int selN = selRange(selPos);
    if (selN > 0) {  // remove the run (all of the steps: a rest stays)
        EditSpec sp;
        sp.kind = EditKind::ReplaceRun;
        sp.mini = true;
        sp.seq = selSeq_;
        sp.pos = selPos;
        sp.cycles = (uint32_t)selN;
        if (selN == prog_->childCount(selSeq_)) sp.code = "~";
        const uint16_t seq = selSeq_;
        selA_ = selB_ = selSeq_ = 0;
        uint16_t nn;
        if (!doSpec(sp, nn)) return error("can't remove these steps");
        if (sp.code.empty()) {  // to the gap where they were
            gapSeq_ = seq;
            gapPos_ = selPos;
            cursor_ = seq;
        }
        Label m;
        return info(m.printf("removed %d step%s", selN, selN == 1 ? "" : "s"));
    }
    if (gapSeq_) return deleteAtGap();
    if (!cursor_) return;
    Kind k = prog_->at(cursor_).kind;
    if (pending_ && pending_ == cursor_) {
        pending_ = 0;
        uint16_t step = stepOf(*prog_, cursor_);
        uint16_t prev = prog_->prevSibling(step);
        cursor_ = step;
        deleteNode();
        if (!messageError_) {
            if (prev) cursor_ = valueOf(*prog_, prev);
            info("entry done");
        }
        return;
    }
    if (isMiniKind(k) && k != Kind::MSeq) {
        if (k == Kind::MRest) {
            cursor_ = stepOf(*prog_, cursor_);  // ~*2 goes as a whole
            uint16_t par = prog_->parentOf(cursor_);
            if (par && prog_->at(par).kind == Kind::MSeq && prog_->childCount(par) > 1) return deleteNode();
            return error("already a rest");
        }
        uint16_t nn;
        EditSpec sp;
        if (tryReplace("~", true, nn, &sp)) {
            if (doSpec(sp, nn)) info("rest (Delete again removes the step)");
            return;
        }
    }
    deleteNode();
}

void Editor::deleteNode() {
    if (!cursor_) return;
    Program& cur = *prog_;
    uint16_t par = cur.parentOf(cursor_);
    if (!par) {
        // the block itself: clear the slot
        if (cur.at(cursor_).kind == Kind::Block || cursor_ == exprRoot()) {
            if (!beginEdit()) return;
            Program* p = prog_;
            uint16_t b = p->block[slot_];
            const int at = p->blockIndex(b);
            p->removeBlockAt(at);
            p->freeTree(b);
            p->block[slot_] = 0;
            for (int i = 0; i < p->blockCount() && !p->block[slot_]; i++)  // another block of dN
                if (p->blockSlot(p->blockAt(i)) == slot_) p->block[slot_] = p->blockAt(i);
            endEdit(0, false);  // no check: the other block is as it was
            emptyPos_ = at;     // the empty placeholder where the block was
            const bool wasPlaying = playing_[slot_] == b;
            if (wasPlaying) playing_[slot_] = 0;
            // the playing block went: dN stops, also when it was edited since
            // (1.0.0 device pass: an edited playing block kept playing)
            if (out_ && wasPlaying) out_->silence(slot_);
            if (!prog_->block[slot_]) state_[slot_] = SlotState::Empty;
            else if (wasPlaying) state_[slot_] = SlotState::Edited;
            info(prog_->block[slot_] ? "block deleted (Up / Down: the blocks around)" : "slot cleared");
        }
        return;
    }
    Kind pk = cur.at(par).kind;
    if (pk == Kind::Block) {  // all of the block's code: the block goes, not a ?? (UI notes, Editor14)
        cursor_ = par;
        return deleteNode();
    }
    // Never a ?? hole (user, UI v3: "very hard to fix"): where the delete
    // would leave one (a function's argument, a section's operand, a lone
    // list element ...), the nearest enclosing function / operator /
    // modifier that can be unwrapped goes instead
    const bool leavesHole = !((pk == Kind::MSeq || pk == Kind::List) && cur.childCount(par) > 1) &&
                            pk != Kind::MSeq && pk != Kind::Op;
    if (leavesHole) {
        // unwrap what it belongs to; if that can't be unwrapped, delete that
        // instead (an operand: the other side stays), never the whole block
        const uint16_t from = cursor_;
        cursor_ = par;
        if (tryUnwrap()) return info("deleted: unwrapped what it belonged to");
        const uint16_t gp = cur.parentOf(par);
        if (gp && cur.at(gp).kind != Kind::Block) return deleteNode();
        cursor_ = from;
        return error("can't delete this (it would leave a gap): change it with the jog or Replace");
    }
    if (!beginEdit()) return;
    Program* p = prog_;
    uint16_t newCursor = par;
    if ((pk == Kind::MSeq || pk == Kind::List) && p->childCount(par) > 1) {
        uint16_t next = p->at(cursor_).next;
        uint16_t prev = p->prevSibling(cursor_);
        if (prev) p->at(prev).next = next;
        else p->at(par).first = next;
        p->at(cursor_).next = 0;
        p->freeTree(cursor_);
        newCursor = next ? next : prev;
    } else if (pk == Kind::MSeq) {
        uint16_t rest = p->alloc(Kind::MRest);
        p->replace(cursor_, rest);
        p->freeTree(cursor_);
        newCursor = rest;
    } else if (pk == Kind::Op) {
        // keep the other operand
        uint16_t other = p->at(par).first == cursor_ ? p->at(cursor_).next : p->at(par).first;
        uint16_t first = p->at(par).first;
        p->at(first).next = 0;
        p->at(par).first = 0;
        uint16_t gone = cursor_;
        p->replace(par, other);
        p->freeTree(par);
        p->freeTree(gone);
        newCursor = other;
    }
    while (isTransparent(*p, newCursor)) newCursor = p->at(newCursor).first;
    if (endEdit(newCursor)) info("deleted");
}

void Editor::doUnwrap() {
    if (!tryUnwrap()) error("can't unwrap this");
}

// Unwrap the node at the cursor (keep the pattern it wraps) if the result
// type-checks; false (nothing changed, no message) if it can't.
bool Editor::tryUnwrap() {
    if (!cursor_) return false;
    const Node& n = prog_->at(cursor_);
    NodeList tries;
    switch (n.kind) {
        case Kind::App: {
            uint16_t last = n.first;
            while (prog_->at(last).next) last = prog_->at(last).next;
            tries.push_back(last);
            break;
        }
        case Kind::Op:
        case Kind::Compose:
            tries.push_back(n.first);
            tries.push_back(prog_->at(n.first).next);
            break;
        default:
            if (isMiniModifier(n.kind)) tries.push_back(n.first);
            else if (n.kind == Kind::MSub || n.kind == Kind::MAlt || n.kind == Kind::MChoice ||
                     n.kind == Kind::MPoly) {
                uint16_t layer = n.first;
                if (layer && prog_->at(layer).first) tries.push_back(prog_->at(layer).first);
            }
            break;
    }
    for (uint16_t keep : tries) {
        if (!keep) continue;
        if (!beginEdit()) return false;
        Program* p = prog_;
        // detach `keep` from its parent, then put it where the cursor is
        uint16_t kp = p->parentOf(keep);
        uint16_t prev = p->prevSibling(keep);
        if (prev) p->at(prev).next = p->at(keep).next;
        else p->at(kp).first = p->at(keep).next;
        p->at(keep).next = 0;
        p->replace(cursor_, keep);
        p->freeTree(cursor_);
        Text err;
        if (!recheck(*p, err)) {
            abortEdit();
            continue;
        }
        uint16_t c = keep;
        while (isTransparent(*p, c)) c = p->at(c).first;
        if (endEdit(c)) info("unwrapped");
        return true;
    }
    return false;
}

}  // namespace tc
TC_COLD_END
