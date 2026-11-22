// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Tree-mode navigation: arrows, leaf-to-leaf flow, slots and blocks, step
// selection inside "...".
#include "editor_impl.h"
#include "cold.h"

TC_COLD_BEGIN
namespace tc {

void Editor::gotoSlot(int s) {
    gotoSlotTree(s);
    if (!cursorMode_) return;
    const bool plain = message_ == cursorInfo();  // the message describes the cursor: follow it
    cursorSettle();
    if (plain) info(cursorInfo());
}

void Editor::gotoSlotTree(int s) {
    if (s < 0 || s >= kSlots) return;
    headGap_ = preSym_ = false;
    onOp_ = false;  // not on the new block's top operator (UI notes 2026-11-22)
    dropEmptyBlocks();
    cps_ = false;
    pending_ = 0;
    gapSeq_ = 0;
    if (newBlockArmed_) {
        newBlockArmed_ = false;
        return newBlockAbove(s);
    }
    const Program& p = *prog_;
    // the pad of the dN the cursor is in, again: a new dN block below this one
    // (the empty placeholder; Replace / Note / Instrument start it, Up / Down
    // leave it); on the placeholder, back to the block (UI notes, Editor14)
    const bool again = s == padAgain_;
    padAgain_ = s;
    if (again && s == slot_ && !cps_ && p.block[s]) {
        if (cursor_) {
            const int at = p.blockIndex(p.block[s]) + 1;
            cursor_ = 0;
            emptyPos_ = at;
            Label m;
            return info(m.printf("new d%d block: Note / Instrument / Screen 1-8 start it, Up / Down leave it",
                                 s + 1));
        }
        enterSlot(s);
        return info(cursorInfo());
    }
    // dN's blocks in file order
    FixVec<uint16_t, kMaxBlocks> mine;
    for (int i = 0; i < p.blockCount(); i++)
        if (p.blockSlot(p.blockAt(i)) == s) mine.push_back(p.blockAt(i));
    uint16_t target = 0;
    if (!mine.empty()) {
        target = mine[0];
        for (uint16_t b : mine)
            if (b == playing_[s]) target = b;  // the playing one first
    }
    if (target) prog_->block[s] = target;
    enterSlot(s);
    if (mine.size() > 1) {
        int k = 0;
        while (k < (int)mine.size() && mine[k] != prog_->block[s]) k++;
        Label m;
        info(m.printf("d%d block %d of %d%s: Shift+Up / Down: the others, the pad again: a new one", s + 1, k + 1,
                      mine.size(),
                      prog_->block[s] == playing_[s] ? " (playing)" : ""));
    } else {
        info(cursorInfo());
    }
}

// Shift+Up / Down (and Up / Down on the empty placeholder): the previous /
// next block's top; above the first block, the setcps line.
void Editor::blockStep(int dir) {
    const Program& p = *prog_;
    gapSeq_ = 0;
    onOp_ = false;
    pending_ = 0;
    if (cps_) {
        if (dir > 0 && p.blockCount()) selectBlock(p.blockAt(0));
        return info(cursorInfo());
    }
    int target;
    const int empty = emptyRowIndex();
    if (empty >= 0) target = dir < 0 ? empty - 1 : empty;
    else target = p.blockIndex(p.block[slot_]) + dir;
    if (target >= 0 && target < p.blockCount()) {
        selectBlock(p.blockAt(target));
    } else if (target < 0 && p.cpsExpr) {
        cps_ = true;
        cursor_ = p.cpsExpr;
    } else {
        return info(cursorInfo());
    }
    emptyPos_ = -1;
    info(cursorInfo());
}

void Editor::selectBlock(uint16_t b) {
    cps_ = false;
    const int s = prog_->blockSlot(b);
    prog_->block[s] = b;
    slot_ = s;
    pending_ = 0;
    gapSeq_ = 0;
    cursor_ = b;
}

// Insert, then slot pad N at a block's top: a new block for dN above the
// current block, as the empty placeholder.
void Editor::newBlockAbove(int slot) {
    // the empty placeholder above the current block (no `dN $ silence`: UI
    // notes 2026-11-22, like Delete leaves); Note / Instrument / Screen 1-8
    // / FX start the block there, Up / Down leave it
    const Program& p = *prog_;
    const uint16_t cur = p.block[slot_];
    int pos = cur ? p.blockIndex(cur) : -1;
    if (pos < 0) pos = p.blockCount();
    if (p.blockCount() >= kMaxBlocks) return error("too many blocks (max " + tnum(kMaxBlocks) + ")");
    slot_ = slot;
    cursor_ = 0;
    gapSeq_ = 0;
    emptyPos_ = pos;
    Label m;
    info(m.printf("new d%d block: Note / Instrument / Screen 1-8 start it, Up / Down leave it", slot + 1));
}

void Editor::selectMenu(int i) {
    if (menuOpen_ && i >= 0 && i < (int)menu_.size()) menuSel_ = i;
}

// numbers of the setcps expression, in order (Left / Right walk them)
static void cpsNumbers(const Program& p, uint16_t n, NodeList& out) {
    if (!n) return;
    if (p.at(n).kind == Kind::Num) out.push_back(n);
    for (uint16_t c = p.at(n).first; c; c = p.at(c).next) cpsNumbers(p, c, out);
}

void Editor::navigate(Action a) {
    const Program& p = *prog_;
    if (cps_) {
        // setcps: its top (the line) <-> its numbers; Down from the top = the
        // first block, Up from a number = the top
        const uint16_t root = p.cpsExpr;
        NodeList nums;
        cpsNumbers(p, root, nums);
        int at = -1;
        for (int i = 0; i < (int)nums.size(); i++)
            if (nums[i] == cursor_) at = i;
        if (cursor_ == root && !(nums.size() == 1 && nums[0] == root)) at = -1;
        const bool top = cursor_ == root;
        if (top && a == Action::Down) {
            if (p.blockCount()) selectBlock(p.blockAt(0));
        } else if (top && a == Action::Right) {
            if (!nums.empty()) cursor_ = nums[0];
        } else if (!top && a == Action::Up) {
            cursor_ = root;
        } else if (!top && (a == Action::Left || a == Action::Right)) {
            const int k = at + (a == Action::Right ? 1 : -1);
            if (at >= 0 && k >= 0 && k < (int)nums.size()) cursor_ = nums[k];
        }
        info(cursorInfo());
        return;
    }
    if (gapSeq_) {
        // on a gap: Right = the step after it, Left = the step before, Up = out
        const int count = p.childCount(gapSeq_);
        if (a == Action::Right && gapPos_ < count) {
            cursor_ = p.child(gapSeq_, gapPos_);
            gapSeq_ = 0;
        } else if (a == Action::Left && gapPos_ > 0) {
            cursor_ = p.child(gapSeq_, gapPos_ - 1);
            gapSeq_ = 0;
        } else if (a == Action::Left || a == Action::Right) {
            // the first / last gap: on across the code
            const uint16_t seq = gapSeq_;
            if (flow(a == Action::Right, seq)) return info(cursorInfo());
        } else if (a == Action::Up) {
            cursor_ = gapSeq_;
            gapSeq_ = 0;
            return navigate(Action::Up);
        }
        info(cursorInfo());
        return;
    }
    if (!cursor_) {  // the empty placeholder: Up / Down to the blocks around it (it goes)
        if (a == Action::Up || a == Action::Down) blockStep(a == Action::Up ? -1 : 1);
        return;
    }
    // a step of a "..." sequence: Left / Right go to the gap before / after it
    if (a == Action::Left || a == Action::Right) {
        const uint16_t par = p.parentOf(cursor_);
        if (par && (p.at(par).kind == Kind::MSeq || p.at(par).kind == Kind::List)) {
            int idx = 0;
            for (uint16_t c = p.at(par).first; c && c != cursor_; c = p.at(c).next) idx++;
            gapSeq_ = par;
            gapPos_ = a == Action::Left ? idx : idx + 1;
            cursor_ = par;
            info(cursorInfo());
            return;
        }
    }
    // at a block's top: Up / Down = the previous / next block, Right = into it
    if (p.at(cursor_).kind == Kind::Block) {
        if (a == Action::Up || a == Action::Down) {
            const int i = p.blockIndex(cursor_) + (a == Action::Up ? -1 : 1);
            if (i >= 0 && i < p.blockCount()) {
                selectBlock(p.blockAt(i));
            } else if (i < 0 && p.cpsExpr) {  // above the first block: the setcps line
                cps_ = true;
                gapSeq_ = 0;
                pending_ = 0;
                cursor_ = p.cpsExpr;
            }
        } else if (a == Action::Right && p.at(cursor_).first) {
            cursor_ = p.at(cursor_).first;
        }
        info(cursorInfo());
        return;
    }
    // on an operator's symbol: Left / Right into the operand on that side
    if (onOperator() && (a == Action::Left || a == Action::Right)) {
        uint16_t l, r;
        operatorSides(l, r);
        onOp_ = false;
        if (a == Action::Right && r) flowInto(true, r);
        else if (a == Action::Left && l) flowInto(false, l);
        return info(cursorInfo());
    }
    if (a == Action::Up && onOp_) {  // from the symbol: the whole operator expression
        onOp_ = false;
        return info(cursorInfo());
    }
    // a leaf, or a node with no sibling on that side: Left / Right go on to
    // the previous / next leaf of the text (into and out of subtrees and
    // "..." sequences, within the block)
    if ((a == Action::Left || a == Action::Right) &&
        (!p.at(cursor_).first || !(a == Action::Right ? p.at(cursor_).next : p.prevSibling(cursor_)))) {
        flow(a == Action::Right, cursor_);
        return info(cursorInfo());
    }
    uint16_t n = 0;
    switch (a) {
        case Action::Down:
            n = p.at(cursor_).first;
            while (isTransparent(p, n)) n = p.at(n).first;
            break;
        case Action::Up:
            n = p.parentOf(cursor_);
            while (isTransparent(p, n)) n = p.parentOf(n);
            break;
        case Action::Right:
            n = p.at(cursor_).next;
            while (isTransparent(p, n)) n = p.at(n).first;
            break;
        case Action::Left:
            n = p.prevSibling(cursor_);
            while (isTransparent(p, n)) n = p.at(n).first;
            break;
        default: break;
    }
    if (n) cursor_ = n;
    info(cursorInfo());
}

// From node `from` (a leaf, or a sequence whose first / last gap the cursor
// is on): climb to the first ancestor with a sibling on that side and go into
// it; inside a "..." sequence or a list, the gap next to the node is the stop.
// The children render in tree order (render.cpp), so this is the text order.
bool Editor::flow(bool right, uint16_t from) {
    const Program& p = *prog_;
    uint16_t c = from;
    for (;;) {
        const uint16_t par = p.parentOf(c);
        // the block's end: Left / Right stay in the block (Editor22 notes:
        // only Up / Down and Shift+Up / Down change blocks)
        if (!par || p.at(par).kind == Kind::Block) return false;
        const Kind pk = p.at(par).kind;
        if (pk == Kind::MSeq || pk == Kind::List) {
            int idx = 0;
            for (uint16_t k = p.at(par).first; k && k != c; k = p.at(k).next) idx++;
            gapSeq_ = par;
            gapPos_ = right ? idx + 1 : idx;
            cursor_ = par;
            return true;
        }
        const uint16_t sib = right ? p.at(c).next : p.prevSibling(c);
        // between an operator's operands, or before a `$`'s pattern: stop on the symbol
        const bool dollarPair = pk == Kind::App && (p.at(par).flags & kDollar) &&
                                (right ? (sib && !p.at(sib).next) : !p.at(c).next);
        if (sib && (pk == Kind::Op || dollarPair)) {
            gapSeq_ = 0;
            cursor_ = par;
            onOp_ = true;
            return true;
        }
        if (sib) {
            flowInto(right, sib);
            return true;
        }
        c = par;
    }
}

// Into node n from the left (right = true) or the right: its first / last
// leaf, or the first / last gap of a sequence or list on the way down.
void Editor::flowInto(bool right, uint16_t n) {
    const Program& p = *prog_;
    gapSeq_ = 0;
    for (;;) {
        const Node& x = p.at(n);
        if ((x.kind == Kind::MSeq || x.kind == Kind::List) && x.first) {
            gapSeq_ = n;
            gapPos_ = right ? 0 : p.childCount(n);
            cursor_ = n;
            return;
        }
        if (!x.first) {
            cursor_ = n;
            return;
        }
        uint16_t k = x.first;
        if (!right)
            while (p.at(k).next) k = p.at(k).next;
        n = k;
    }
}

// Empty slot: create its (empty) block and put the cursor on it.
void Editor::ensureBlock() {
    if (cursor_) return;
    Program* p = prog_;  // part of the edit that follows (or of nothing: an empty block)
    uint16_t b = p->alloc(Kind::Block);
    p->at(b).a = (uint32_t)slot_;
    // where the placeholder is (slot order unless it was put somewhere)
    const int pos = emptyRowIndex() >= 0 ? emptyRowIndex() : p->blockCount();
    emptyPos_ = -1;
    if (!b || !p->insertBlock(pos, b)) {
        if (b) p->freeTree(b);
        return error("too many blocks (max " + tnum(kMaxBlocks) + ")");
    }
    p->block[slot_] = b;
    cursor_ = b;
}

// ensureBlock() starts a block before the edit that fills it, so a cancelled
// menu or an undo of that edit leaves a block with no code ("d12" alone:
// not Tidal, and nothing to evaluate). Drop it: dN is empty again. No undo
// step holds the block (it was made outside every checkpoint), so it goes
// without the journal.
void Editor::dropEmptyBlocks() {
    if (menuOpen_ || kbdOpen_ || prog_->checkpointOpen()) return;
    Program* p = prog_;
    for (int i = p->blockCount() - 1; i >= 0; i--) {
        const uint16_t b = p->blockAt(i);
        if (p->at(b).first) continue;
        const int s = p->blockSlot(b);
        p->removeBlockAt(i);
        p->freeTree(b);
        if (cursor_ == b) {
            cursor_ = 0;
            emptyPos_ = i;  // the placeholder stays where the block was
        }
        if (playing_[s] == b) {  // e.g. undone back to nothing while it played
            playing_[s] = 0;
            if (out_) out_->silence(s);
            state_[s] = SlotState::Empty;
        }
        if (p->block[s] == b) {
            p->block[s] = 0;
            for (int k = 0; k < p->blockCount() && !p->block[s]; k++)  // another block of dN
                if (p->blockSlot(p->blockAt(k)) == s) p->block[s] = p->blockAt(k);
            if (s == slot_ && !cursor_ && emptyPos_ < 0) cursor_ = p->block[s];
            if (!p->block[s] && state_[s] != SlotState::Playing) state_[s] = SlotState::Empty;
        }
    }
}

// ---------------------------------------------------------------------------
// selecting a run of steps (Shift+Left / Right inside "...")

bool Editor::canSelect() const {
    if (gapSeq_) return prog_->at(gapSeq_).kind == Kind::MSeq && prog_->childCount(gapSeq_) > 0;
    if (!cursor_ || !isMiniKind(prog_->at(cursor_).kind)) return false;
    const uint16_t st = stepOf(*prog_, cursor_), par = prog_->parentOf(st);
    return par && prog_->at(par).kind == Kind::MSeq;
}

int Editor::selRange(int& pos) const {
    if (!selSeq_) return 0;
    int ia = -1, ib = -1, i = 0;
    for (uint16_t c = prog_->at(selSeq_).first; c; c = prog_->at(c).next, i++) {
        if (c == selA_) ia = i;
        if (c == selB_) ib = i;
    }
    if (ia < 0 || ib < 0) return 0;
    pos = ia < ib ? ia : ib;
    return (ia < ib ? ib - ia : ia - ib) + 1;
}

uint16_t Editor::selFirst() const {
    int pos;
    return selRange(pos) ? prog_->child(selSeq_, pos) : 0;
}

uint16_t Editor::selLast() const {
    int pos;
    const int n = selRange(pos);
    return n ? prog_->child(selSeq_, pos + n - 1) : 0;
}

// Steps pos .. pos+n-1 of a sequence, rendered and joined by `sep`.
Text Editor::joinSteps(uint16_t seq, int pos, int n, const char* sep) const {
    Text t;
    for (int k = 0; k < n; k++) {
        if (k) t += sep;
        t += renderNode(*prog_, prog_->child(seq, pos + k));
    }
    return t;
}

Text Editor::selText() const {
    int pos;
    const int n = selRange(pos);
    Text t;
    for (int k = 0; k < n; k++) {
        if (k) t += ' ';
        t += renderNode(*prog_, prog_->child(selSeq_, pos + k));
    }
    return t;
}

void Editor::selectMove(int dir) {
    if (!selSeq_) {
        if (!canSelect()) return error("Shift + arrows select steps inside \"...\"");
        if (gapSeq_) {  // from a gap: start at the step on that side
            const int count = prog_->childCount(gapSeq_);
            int k = dir > 0 ? gapPos_ : gapPos_ - 1;
            if (k < 0 || k >= count) return error("no step on that side of the gap");
            selSeq_ = gapSeq_;
            selA_ = selB_ = prog_->child(gapSeq_, k);
            gapSeq_ = 0;
        } else {
            const uint16_t st = stepOf(*prog_, cursor_);
            selSeq_ = prog_->parentOf(st);
            selA_ = selB_ = st;
            // the first press selects the step and the one next to it
            const uint16_t nb = dir > 0 ? prog_->at(st).next : prog_->prevSibling(st);
            if (nb) selB_ = nb;
        }
    } else {
        const uint16_t nb = dir > 0 ? prog_->at(selB_).next : prog_->prevSibling(selB_);
        if (nb) selB_ = nb;
    }
    cursor_ = selB_;
    int pos;
    const int n = selRange(pos);
    Label m;
    info(m.printf("%d step%s selected: Screen 3-7 wrap them, Delete removes, Copy copies", n, n == 1 ? "" : "s"));
}

}  // namespace tc
TC_COLD_END
