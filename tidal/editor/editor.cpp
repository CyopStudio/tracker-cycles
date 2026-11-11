#include "editor.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "../core/check.h"
#include "keyboard.h"
#include "../core/library.h"
#include "../core/mapping.h"
#include "../core/parse.h"
#include "../core/render.h"

namespace tc {

static const struct { const char* name; Action a; } kActions[] = {
    {"left", Action::Left},       {"right", Action::Right},       {"up", Action::Up},
    {"down", Action::Down},       {"wheel+", Action::WheelUp},    {"wheel-", Action::WheelDown},
    {"wheel++", Action::WheelUpFast}, {"wheel--", Action::WheelDownFast},
    {"replace", Action::Replace}, {"wrap", Action::Wrap},         {"insert", Action::Insert},
    {"delete", Action::Delete},   {"unwrap", Action::Unwrap},     {"undo", Action::Undo},
    {"eval", Action::Evaluate},   {"hush", Action::Hush},         {"next", Action::NextSlot},
    {"prev", Action::PrevSlot},   {"ok", Action::Confirm},        {"cancel", Action::Cancel},
    {"save", Action::Save},       {"tracker", Action::ToggleTrackerMode},
    {"copy", Action::Copy},       {"paste", Action::Paste},       {"home", Action::Home},
    {"notes", Action::MenuNotes}, {"samples", Action::MenuSamples}, {"chords", Action::MenuChords},
    {"silence", Action::SilenceSlot}, {"folders", Action::MenuFolders},
    {"blockup", Action::BlockUp}, {"blockdown", Action::BlockDown},
    {"functions", Action::MenuFunctions}, {"controls", Action::MenuControls},
    {"quick1", Action::Quick1},   {"quick2", Action::Quick2},     {"quick3", Action::Quick3},
    {"quick4", Action::Quick4},   {"launch", Action::MenuLaunch},
    {"struct1", Action::Struct1}, {"struct2", Action::Struct2},   {"struct3", Action::Struct3},
    {"struct4", Action::Struct4}, {"struct5", Action::Struct5},   {"struct6", Action::Struct6},
    {"struct7", Action::Struct7}, {"struct8", Action::Struct8},
    {"modifiers", Action::MenuModifiers}, {"fx", Action::MenuFx},
    {"jumpn", Action::JumpNotes}, {"jumps", Action::JumpSamples},
    {"playcancel", Action::PlayCancel},
    {"cps", Action::JumpCps},     {"selleft", Action::SelLeft}, {"selright", Action::SelRight},     {"comment", Action::EditComment},
    {"kbdtype", Action::KbdType}, {"kbdok", Action::KbdOk},       {"kbdcancel", Action::KbdCancel},
    {"kbdclear", Action::KbdClear},
};

const char* actionName(Action a) {
    for (auto& x : kActions)
        if (x.a == a) return x.name;
    return "?";
}
bool actionFromName(const char* s, Action& out) {
    for (auto& x : kActions)
        if (!strcmp(s, x.name)) {
            out = x.a;
            return true;
        }
    return false;
}

// Default argument for a control when it is inserted.
static const char* controlDefault(const char* name) {
    static const struct { const char* n; const char* d; } t[] = {
        {"s", "\"bd sd\""},     {"sound", "\"bd sd\""}, {"n", "\"0 .. 3\""}, {"note", "\"c e g\""},
        {"up", "12"},           {"gain", "1"},          {"pan", "sine"},     {"cutoff", "1000"},
        {"resonance", "0.2"},   {"hpf", "200"},         {"bandf", "1000"},   {"delay", "0.3"},
        {"room", "0.3"},        {"crush", "4"},         {"shape", "0.3"},    {"begin", "0.25"},
        {"speed", "2"},         {"legato", "1"},        {"sustain", "0.5"},  {"nudge", "0.1"},
        {"cut", "1"},           {"orbit", "1"},         {"end", "0.75"},     {"amp", "0.4"},
        {"velocity", "0.8"},    {"attack", "0.01"},     {"release", "0.2"},  {"hresonance", "0.2"},
        {"distort", "0.3"},     {"midichan", "1"},      {"ccn", "74"},       {"ccv", "\"0 64 127\""},
        {"midibend", "0"},      {"lpf", "1000"},        {"lpq", "0.2"},      {"hpq", "0.2"},
        {"bpf", "1000"},        {"att", "0.01"},        {"rel", "0.2"},     {"voices", "3"},
    };
    for (auto& x : t)
        if (strcmp(x.n, name) == 0) return x.d;
    return "1";
}

static const char* const kDefaultInstruments[kInstrumentSlots] = {
    "bd",    "sd",    "hh",    "cp",    "ho",     "cr",     "lt",    "mt",    "ht",    "drum",
    "sn",    "tink",  "808bd", "808sd", "808hc",  "808oh",  "808cy", "808lt", "808mt", "808ht",
    "808lc", "808mc", "808",   "hh27",  "arpy",   "bass",   "bass1", "bass2", "bass3", "jvbass",
    "pluck", "casio", "blip",  "bend",  "bev",    "bottle", "break", "jazz",  "feel",  "future",
    "gab",   "hand",  "east",  "kurt",  "rave",   "space",  "tabla", "wind"};

Editor::Editor(Program& prog, Checkpoint& cp, uint8_t* undoArena, int undoBytes)
    : prog_(&prog), cp_(&cp), undoArena_(undoArena), undoCap_(undoBytes) {
    for (auto& s : state_) s = SlotState::Empty;
    // 48 instrument slots, one per pad (the Tracker has 48 sample
    // instruments). Default: Dirt-Samples folders, so SuperDirt plays them.
    for (int i = 0; i < kInstrumentSlots; i++) setInstrument(i, kDefaultInstruments[i]);
    info("ready");
}

void Editor::setInstrument(int i, const char* name) {
    if (i < 0 || i >= kInstrumentSlots) return;
    snprintf(samples_[i], sizeof samples_[i], "%s", name ? name : "");
}

// ---------------------------------------------------------------------------
// files

bool Editor::loadText(const char* text, int len, Label& err) {
    abortEdit();
    Error e;
    if (!parseFile(*prog_, text, len, e)) {
        int line, col;
        errorLineCol(text, e.pos, line, col);
        err.printf("%d:%d: %s", line, col, e.msg);
        prog_->clear();
        return false;
    }
    Error ce;
    if (!checkProgram(*prog_, ce)) {
        err.printf("type error: %s", ce.msg);
        prog_->clear();
        return false;
    }
    undoHead_ = undoBytes_ = undoCount_ = 0;
    changes_++;
    for (int s = 0; s < kSlots; s++) state_[s] = prog_->block[s] ? SlotState::Edited : SlotState::Empty;
    slot_ = 0;
    for (int s = 0; s < kSlots; s++)
        if (prog_->block[s]) {
            slot_ = s;
            break;
        }
    enterSlot(slot_);
    info("loaded");
    return true;
}

int Editor::fileText(char* buf, int cap) const {
    Renderer r(buf, cap);
    if (cap > 0) buf[0] = 0;
    return r.file(*prog_);
}

// ---------------------------------------------------------------------------
// undo ring: steps of [4-byte length][tag][a][b], newest at the end; the
// oldest steps are dropped when a new one doesn't fit

uint8_t* Editor::reserveUndo(uint8_t tag, int payload) {
    const int len = 1 + payload, need = 4 + len;
    if (need > undoCap_) return nullptr;  // can't keep this one
    auto dropOldest = [&]() {
        int first;
        memcpy(&first, undoArena_, 4);
        memmove(undoArena_, undoArena_ + 4 + first, (size_t)(undoBytes_ - 4 - first));
        undoBytes_ -= 4 + first;
        undoCount_--;
    };
    while (undoBytes_ + need > undoCap_ && undoCount_ > 0) dropOldest();
    if (undoCount_ >= 64) dropOldest();  // the prototype's depth
    uint8_t* d = undoArena_ + undoBytes_;
    memcpy(d, &len, 4);
    d[4] = tag;
    undoBytes_ += need;
    undoCount_++;
    return d + 5;
}

bool Editor::popUndo(uint8_t& tag, const uint8_t*& data, int& len) {
    if (!undoCount_) return false;
    // walk to the newest step
    int at = 0, l = 0;
    for (int i = 0; i < undoCount_; i++) {
        memcpy(&l, undoArena_ + at, 4);
        if (i + 1 < undoCount_) at += 4 + l;
    }
    tag = undoArena_[at + 4];
    data = undoArena_ + at + 5;
    len = l - 1;
    undoBytes_ = at;  // the data stays readable until the next push
    undoCount_--;
    return true;
}

// ---------------------------------------------------------------------------
// helpers

Text Editor::renderNode(const Program& p, uint16_t n) const {
    char buf[Text::capacity() + 2];
    Renderer r(buf, sizeof buf);
    buf[0] = 0;
    const int len = r.expr(p, n);
    Text t;
    t.assign(buf, len > Text::capacity() || r.overflow() ? Text::capacity() + 1 : len);  // marks overflow
    return t;
}

bool Editor::isTransparent(const Program& p, uint16_t n) const {
    if (!n) return false;
    const Node& x = p.at(n);
    if (x.kind != Kind::MSeq) return false;
    uint16_t par = p.parentOf(n);
    if (par && p.at(par).kind == Kind::Mini) return true;
    return x.first && !p.at(x.first).next && !(x.flags & kDots);
}

uint16_t Editor::exprRoot() const {
    uint16_t b = prog_->block[slot_];
    return b ? prog_->at(b).first : 0;
}

int Editor::emptyRowIndex() const {
    if (cps_ || cursor_ || kbdOpen_) return -1;
    const Program& p = *prog_;
    if (emptyPos_ >= 0) return emptyPos_ < p.blockCount() ? emptyPos_ : p.blockCount();
    int pos = 0;  // slot order: after the last block of a lower or equal dN
    for (int i = 0; i < p.blockCount(); i++)
        if (p.blockSlot(p.blockAt(i)) <= slot_) pos = i + 1;
    return pos;
}

void Editor::enterSlot(int s) {
    emptyPos_ = -1;
    slot_ = s;
    cps_ = false;
    uint16_t b = prog_->block[s];
    cursor_ = b ? (prog_->at(b).first ? prog_->at(b).first : b) : 0;
    menuOpen_ = false;
}

bool Editor::recheck(Program& p, Text& err) const {
    Error e;
    if (cps_) {  // editing setcps: only its expression is checked
        if (!checkCps(p, e)) {
            err = e.msg;
            return false;
        }
        return true;
    }
    if (!checkBlock(p, slot_, e)) {
        err = e.msg;
        return false;
    }
    // Tracker mode: refuse edits (and so menu items) that bring in something
    // the device can't play; a block that already has such a thing (loaded
    // from a file) stays editable, evaluate() reports it.
    if (trackerMode_ && trackerBefore_) {
        Error now;
        if (!trackerCheck(p, slot_, now)) {
            err = now.msg;
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// in-place edits

bool Editor::beginEdit() {
    if (prog_->checkpointOpen()) prog_->rollback(*cp_);  // never nest
    Error e;
    trackerBefore_ = !trackerMode_ || cps_ || trackerCheck(*prog_, slot_, e);
    const uint16_t roots[2] = {prog_->block[slot_], prog_->cpsExpr};
    if (!prog_->beginCheckpoint(*cp_, roots, 2)) {
        error("block too large to edit");
        return false;
    }
    return true;
}

void Editor::abortEdit() {
    if (prog_->checkpointOpen()) prog_->rollback(*cp_);
}

bool Editor::endEdit(uint16_t cursor, bool check) {
    gapSeq_ = 0;
    Text err;
    if (check && !recheck(*prog_, err)) {
        abortEdit();
        error(err);
        return false;
    }
    prog_->commitCheckpoint();
    if (uint8_t* d = reserveUndo('C', checkpointPackedSize(*cp_))) packCheckpoint(*cp_, d);
    changes_++;
    cursor_ = cursor;
    messageError_ = false;  // callers test it after an edit; an earlier error must not stick
    if (cps_) return true;  // setcps: no slot state
    if (state_[slot_] != SlotState::Empty || prog_->block[slot_]) state_[slot_] = SlotState::Edited;
    return true;
}

uint16_t Editor::applySpec(const EditSpec& s) {
    Program& P = *prog_;
    Error e;
    const uint16_t t = s.target;
    switch (s.kind) {
        case EditKind::Replace: {
            uint16_t n = s.mini ? parseMini(P, s.code.c_str(), (int)s.code.size(), e)
                                : parseExpr(P, s.code.c_str(), (int)s.code.size(), e);
            if (!n) return 0;
            if (s.mini && P.at(t).kind != Kind::MSeq) {
                // replacing a single step: unwrap the parsed one-step sequence
                Node& sq = P.at(n);
                if (!sq.first || P.at(sq.first).next || (sq.flags & kDots)) return 0;
                uint16_t step = sq.first;
                sq.first = 0;
                P.freeTree(n);
                n = step;
            }
            Node& cur = P.at(t);
            if (cur.kind == Kind::Block) {
                if (cur.first) {
                    uint16_t old = cur.first;
                    P.replace(old, n);
                    P.freeTree(old);
                } else {
                    P.appendChild(t, n);
                }
            } else {
                P.replace(t, n);
                P.freeTree(t);
            }
            return n;
        }
        case EditKind::WrapList: {
            if (!t || P.at(t).kind == Kind::Block) return 0;
            const Text code = s.code + " [silence, silence]";
            const uint16_t app = parseExpr(P, code.c_str(), (int)code.size(), e);
            if (!app) return 0;
            uint16_t list = 0;
            for (uint16_t c = P.at(app).first; c; c = P.at(c).next)
                if (P.at(c).kind == Kind::List) list = c;
            if (!list) return 0;
            const uint16_t s1 = P.at(list).first, s2 = P.at(s1).next;
            const uint16_t copy = P.copyTree(P, t);
            if (!copy) return 0;
            P.replace(t, app);  // fn [..] takes x's place
            P.replace(s1, t);   // x is the first element
            P.freeTree(s1);
            P.replace(s2, copy);  // its copy the second
            P.freeTree(s2);
            return copy;
        }
        case EditKind::WrapApp: {
            uint16_t h = parseExpr(P, s.code.c_str(), (int)s.code.size(), e);
            if (!h) return 0;
            uint16_t app;
            if (P.at(h).kind == Kind::App && !(P.at(h).flags & kDollar)) {
                app = h;
            } else {
                app = P.alloc(Kind::App);
                if (!app) return 0;
                P.appendChild(app, h);
            }
            if (exprPrec(P, t) < 11) P.at(app).flags |= kDollar;
            P.replace(t, app);
            P.appendChild(app, t);
            return app;
        }
        case EditKind::WrapOp: {
            const int id = libFind(s.op.c_str(), (int)s.op.size());
            if (id < 0) return 0;
            uint16_t r = parseExpr(P, s.code.c_str(), (int)s.code.size(), e);
            if (!r) return 0;
            uint16_t o = P.alloc(Kind::Op);
            if (!o) return 0;
            P.at(o).a = (uint32_t)id;
            P.replace(t, o);
            if (s.left) {
                P.appendChild(o, t);
                P.appendChild(o, r);
            } else {
                P.appendChild(o, r);
                P.appendChild(o, t);
            }
            return o;
        }
        case EditKind::InsertGap: {
            uint16_t sq = parseMini(P, s.code.c_str(), (int)s.code.size(), e);
            if (!sq) return 0;
            Node& seq = P.at(sq);
            if (!seq.first || P.at(seq.first).next || (seq.flags & kDots)) return 0;  // one step only
            const uint16_t step = seq.first;
            seq.first = 0;
            P.freeTree(sq);
            return linkAtGap(P, step, s.seq, s.pos) ? step : 0;
        }
        case EditKind::InsertList: {
            uint16_t x = parseExpr(P, s.code.c_str(), (int)s.code.size(), e);
            return x && linkAtGap(P, x, s.seq, s.pos) ? x : 0;
        }
        case EditKind::Launch:
            setBlockLaunch(P.at(t), (Launch)s.launch, s.cycles);
            return t;
        case EditKind::SetOp:
            if (!t || P.at(t).kind != Kind::Op) return 0;
            P.at(t).a = (uint32_t)s.launch;
            return t;
        case EditKind::ReplaceRun: {
            const uint16_t seq = s.seq;
            const int count = s.cycles, total = P.childCount(seq);
            if (count < 1 || s.pos < 0 || s.pos + count > total) return 0;
            uint16_t step = 0;
            if (!s.code.empty()) {
                const uint16_t sq = parseMini(P, s.code.c_str(), s.code.size(), e);
                if (!sq) return 0;
                Node& one = P.at(sq);
                if (!one.first || P.at(one.first).next || (one.flags & kDots)) return 0;
                step = one.first;
                one.first = 0;
                P.freeTree(sq);
            } else if (count == total) {
                return 0;  // a sequence keeps one step
            }
            // unlink the run, put the new step in its place, free the run
            const uint16_t prev = s.pos ? P.child(seq, s.pos - 1) : 0;
            uint16_t first = prev ? P.at(prev).next : P.at(seq).first, last = first;
            for (int k = 1; k < count; k++) last = P.at(last).next;
            const uint16_t after = P.at(last).next;
            P.at(last).next = 0;
            uint16_t link = after;
            if (step) {
                P.at(step).next = after;
                link = step;
            }
            if (prev) P.at(prev).next = link;
            else P.at(seq).first = link;
            for (uint16_t c = first; c;) {
                const uint16_t nx = P.at(c).next;
                P.freeTree(c);
                c = nx;
            }
            return step ? step : seq;
        }
    }
    return 0;
}

bool Editor::trySpec(const EditSpec& s, uint16_t& nn) {
    if (buildOnly_) {  // building a menu: collect now, check in checkMenu()
        nn = 0;
        return true;
    }
    if (!beginEdit()) return false;
    const uint16_t n = applySpec(s);
    Text err;
    const bool ok = n && recheck(*prog_, err);
    abortEdit();
    if (ok) nn = n;
    return ok;
}

bool Editor::openSpec(const EditSpec& s, uint16_t& nn) {
    if (!beginEdit()) return false;
    const uint16_t n = applySpec(s);
    Text err;
    if (!n || !recheck(*prog_, err)) {
        abortEdit();
        return false;
    }
    nn = n;
    return true;
}

bool Editor::doSpec(const EditSpec& s, uint16_t& nn) {
    if (!openSpec(s, nn)) return false;
    return endEdit(nn);
}

static EditSpec spec(EditKind k, uint16_t target, const Text& code) {
    EditSpec s;
    s.kind = k;
    s.target = target;
    s.code = code;
    return s;
}

bool Editor::tryReplace(const Text& code, bool mini, uint16_t& newNode, EditSpec* out) {
    EditSpec s = spec(EditKind::Replace, cursor_, code);
    s.mini = mini;
    if (!trySpec(s, newNode)) return false;
    if (out) *out = s;
    return true;
}

bool Editor::tryWrapList(const char* fn, uint16_t& newNode, EditSpec* out) {
    EditSpec s = spec(EditKind::WrapList, cursor_, fn);
    if (!trySpec(s, newNode)) return false;
    if (out) *out = s;
    return true;
}

bool Editor::tryWrapApp(const Text& head, uint16_t& newNode, EditSpec* out) {
    EditSpec s = spec(EditKind::WrapApp, cursor_, head);
    if (!trySpec(s, newNode)) return false;
    if (out) *out = s;
    return true;
}

bool Editor::tryWrapOp(const char* op, const Text& rhs, bool xOnLeft, uint16_t& newNode, EditSpec* out) {
    EditSpec s = spec(EditKind::WrapOp, cursor_, rhs);
    s.op = op;
    s.left = xOnLeft;
    if (!trySpec(s, newNode)) return false;
    if (out) *out = s;
    return true;
}

MenuList& Editor::menuBuild() {
    menuOpen_ = false;
    menuVoices_ = false;
    menuFolders_ = false;
    menu_.clear();
    menuNext_ = 0;
    buildOnly_ = true;
    return menu_;
}

int Editor::checkMenu(int n) {
    if (menuFolders_) {  // names, not edits: nothing to check
        for (MenuItem& it : menu_) it.checked = true;
        menuNext_ = menu_.size();
    }
    while (n-- > 0 && menuNext_ < menu_.size()) {
        MenuItem& it = menu_[menuNext_];
        uint16_t nn = 0;
        if (trySpec(it.spec, nn)) {
            it.cursor = nn;
            it.checked = true;
            menuNext_++;
        } else {
            menu_.erase_at(menuNext_);
            if (menuSel_ > menuNext_) menuSel_--;
        }
    }
    if (menuSel_ >= menu_.size()) menuSel_ = menu_.size() ? menu_.size() - 1 : 0;
    if (menuOpen_ && menu_.empty()) {
        menuOpen_ = false;
        error("nothing fits here");
    }
    if (menuOpen_ && !menuTouched_ && menu_.size() == 1 && menuNext_ == 1) {
        applyMenuItem(0, onOperator());  // one item left: as if it had been the only one
        return 0;
    }
    return menu_.size() - menuNext_;
}

// A menu with one item that fits applies it at once (user, after
// Editor14): no menu to confirm. Lazily checked menus (the device) do the
// same when checking leaves one item before the user touched the menu.
bool Editor::openMenu(const Label& title) {
    buildOnly_ = false;
    menuNext_ = 0;
    if (!lazyMenus_ || menu_.size() == 1) checkMenu(menu_.size());
    if (menu_.empty()) {
        error("nothing fits here");
        return false;
    }
    menuTitle_ = title;
    menuSel_ = 0;
    menuOpen_ = true;
    menuTouched_ = false;
    if (menu_.size() == 1 && menuNext_ == 1) {
        applyMenuItem(0, onOperator());
        return true;
    }
    info(title + ": wheel to choose, ok to apply, cancel to close");
    return true;
}

void Editor::applyMenuItem(int i, bool wasOnOp) {
    MenuItem& it = menu_[i];
    if (menuFolders_) {
        menuOpen_ = menuFolders_ = false;
        Label msg;
        if (!out_) return error("no output");
        if (out_->loadFolder(it.label.c_str(), msg)) info(msg);
        else error(msg);
        return;
    }
    if (it.label.startsWith("# ")) lastControl_ = it.label.substr(2);
    menuOpen_ = false;
    uint16_t nn = 0;
    if (!doSpec(it.spec, nn)) return error("can't apply " + it.label);
    cursor_ = it.checked ? it.cursor : nn;
    gapSeq_ = 0;
    onOp_ = wasOnOp && it.spec.kind == EditKind::SetOp;
    state_[slot_] = SlotState::Edited;
    if (menuVoices_ && !it.label.startsWith("single")) ensureVoices();
    info(menuTitle_ + ": " + it.label);
}

bool Editor::padNumeric() const {
    if (menuOpen_ || !cursor_) return false;
    const Node& n = prog_->at(cursor_);
    // a number argument, the quotes of a numeric pattern, or anything inside
    // them (a step, or a group like <1 2> / 0 .. 7, which gets replaced)
    if (!(n.kind == Kind::Num || n.kind == Kind::Mini || isMiniKind(n.kind))) return false;
    return n.type == kTyPatBase + (int)Base::Time || n.type == kTyPatBase + (int)Base::Double ||
           n.type == kTyPatBase + (int)Base::Int || n.type == kTyPatBase + (int)Base::Note;
}

bool Editor::cursorInt(int& v) const {
    if (!cursor_) return false;
    const Node& n = prog_->at(cursor_);
    if (n.kind == Kind::MNote) {
        v = (int32_t)n.a;
        return true;
    }
    if ((n.kind == Kind::Num || n.kind == Kind::MNum) && n.b == 1) {
        v = (int32_t)n.a;
        return true;
    }
    return false;
}

bool Editor::cursorRest() const { return cursor_ && prog_->at(cursor_).kind == Kind::MRest; }

// The step a mini node belongs to: climb out of modifier targets (bd in
// bd*2). A range's ends (lo .. hi) are values of their own, so stop there.
static uint16_t stepOf(const Program& p, uint16_t n) {
    for (;;) {
        uint16_t par = p.parentOf(n);
        if (!par || !isMiniModifier(p.at(par).kind) || p.at(par).kind == Kind::MRange || p.at(par).first != n)
            return n;
        n = par;
    }
}

// The value to select in a step: descend into modifier targets.
static uint16_t valueOf(const Program& p, uint16_t step) {
    while (step && isMiniModifier(p.at(step).kind) && p.at(step).first) step = p.at(step).first;
    return step;
}

void Editor::enterNumber(int v, bool rest) {
    padAgain_ = -1;
    if (!padNumeric()) return error("pads enter numbers inside a numeric pattern");
    Text n = tnum(v);
    if (rest) return enterStep("~", "", "~");
    if (heldPads_ && chordHere()) return chordAdd(n, n);
    const uint16_t s0 = gapSeq_;
    const int p0 = gapPos_;
    enterStep(n, v < 0 ? "(" + n + ")" : n, n);
    if (entryDone(s0, p0)) chord_.push_back(n.c_str());
}

// Instrument mode: pads are the 48 instrument slots (samples_).
bool Editor::padInstruments() const {
    if (menuOpen_ || !cursor_) return false;
    const Node& n = prog_->at(cursor_);
    if (n.type != kTyPatBase + (int)Base::String) return false;
    if (!(n.kind == Kind::Mini || isMiniKind(n.kind))) return false;
    // only the pattern of s / sound (not arp "up", scale "major", ...)
    uint16_t m = cursor_;
    while (m && prog_->at(m).kind != Kind::Mini) m = prog_->parentOf(m);
    uint16_t app = m ? prog_->parentOf(m) : 0;
    if (!app || prog_->at(app).kind != Kind::App) return false;
    const Node& f = prog_->at(prog_->at(app).first);
    if (f.kind != Kind::Fn) return false;
    const char* name = lib((int)f.a).name;
    return strcmp(name, "s") == 0 || strcmp(name, "sound") == 0;
}

int Editor::cursorInstrument() const {
    if (!cursor_ || prog_->at(cursor_).kind != Kind::MWord) return -1;
    const char* w = prog_->str((int)prog_->at(cursor_).a);
    for (int i = 0; i < kInstrumentSlots; i++)
        if (!strcmp(samples_[i], w)) return i;
    return -1;
}

void Editor::enterInstrument(int i) {
    padAgain_ = -1;
    if (!padInstruments()) return error("pads pick instruments inside an s pattern");
    if (i < 0 || i >= kInstrumentSlots || !samples_[i][0])
        return error("instrument slot " + tnum(i + 1) + " is empty");
    if (heldPads_ && chordHere()) return chordAdd(samples_[i], samples_[i]);
    const uint16_t s0 = gapSeq_;
    const int p0 = gapPos_;
    enterStep(samples_[i], "", samples_[i]);
    if (entryDone(s0, p0)) chord_.push_back(samples_[i]);
}

// ---- chords (held pads, Shift+Note) ----------------------------------------

void Editor::entryPadDown(int pad) {
    if (pad >= 0 && pad < 64) heldPads_ |= (uint64_t)1 << pad;
}

void Editor::entryPadUp(int pad) {
    if (pad >= 0 && pad < 64) heldPads_ &= ~((uint64_t)1 << pad);
    if (!heldPads_) chord_.clear();  // all up: the chord is done
}

// The cursor is still in the gap after the step the held pads entered.
bool Editor::chordHere() const {
    return !chord_.empty() && gapSeq_ && gapSeq_ == chordSeq_ && gapPos_ == chordPos_ && gapPos_ > 0 &&
           !menuOpen_;
}

// A pad entry ended in a gap it moved (a step entered): the start of a chord
// if more pads come while this one is held.
bool Editor::entryDone(uint16_t seq0, int pos0) {
    chord_.clear();
    if (!gapSeq_ || (gapSeq_ == seq0 && gapPos_ == pos0) || listGap() || gapPos_ < 1) return false;
    // only a plain value (not the range or group the pads just ended)
    const Kind k = prog_->at(prog_->child(gapSeq_, gapPos_ - 1)).kind;
    if (k != Kind::MNum && k != Kind::MNote && k != Kind::MWord) return false;
    chordSeq_ = gapSeq_;
    chordPos_ = gapPos_;
    return true;
}

// A pad while another is held: the step before the gap becomes the stack of
// every held pad's value, "[0,4,7]"; the cursor stays in the gap.
void Editor::chordAdd(const Text& code, const Label& label) {
    if (chord_.size() >= chord_.capacity()) return error("a chord takes at most 8 notes");
    for (const auto& c : chord_)
        if (code == c.c_str()) return info("already in the chord: " + label);
    chord_.push_back(code.c_str());
    Text st = "[";
    for (int i = 0; i < chord_.size(); i++) st = st + (i ? "," : "") + chord_[i].c_str();
    st = st + "]";
    const uint16_t seq = gapSeq_;
    const int pos = gapPos_;
    EditSpec sp = spec(EditKind::Replace, prog_->child(seq, pos - 1), st);
    sp.mini = true;
    uint16_t nn;
    if (!doSpec(sp, nn)) {
        chord_.pop_back();
        gapSeq_ = seq;
        gapPos_ = pos;
        cursor_ = seq;
        return error("chord " + st + " doesn't fit here");
    }
    gapSeq_ = seq;
    gapPos_ = pos;
    cursor_ = seq;
    if (chord_.size() == 2) ensureVoices();
    info("= chord " + st + "  (hold pads to add)");
}

static uint16_t findControlApp(const Program& p, uint16_t n, const char* name) {
    if (!n) return 0;
    const Node& x = p.at(n);
    if (x.kind == Kind::App && x.first && p.at(x.first).kind == Kind::Fn && !strcmp(lib((int)p.at(x.first).a).name, name))
        return n;
    for (uint16_t c = x.first; c; c = p.at(c).next)
        if (uint16_t f = findControlApp(p, c, name)) return f;
    return 0;
}

// Chords ring only with more than one voice: a sample block without a
// `voices` control gets `# voices 3` on its whole pattern (MIDI blocks
// play chords anyway; an existing voices is left as the user set it).
void Editor::ensureVoices() {
    if (slot_ >= kSampleSlots) return;
    const uint16_t b = prog_->block[slot_];
    if (!b || !exprRoot() || findControlApp(*prog_, b, "voices")) return;
    const uint16_t old = cursor_, seq = gapSeq_;
    const int pos = gapPos_;
    cursor_ = exprRoot();
    gapSeq_ = 0;
    uint16_t nn;
    EditSpec sp;
    const bool ok = tryWrapOp("#", "voices 3", true, nn, &sp) && doSpec(sp, nn);
    cursor_ = old;
    gapSeq_ = seq;
    gapPos_ = pos;
    if (!ok) error("chord: can't add # voices 3 (it would cut its own notes)");
}

// Shift+Note: chord names on the note at the cursor (in a gap: the step
// before it, e.g. right after pad entry). The root is written as a note name
// (Tidal: c5 = 0), since the chord syntax takes one: 4 -> e'maj, 12 -> c6'maj.
void Editor::openChords() {
    if (gapSeq_) {
        if (gapPos_ == 0 || listGap()) return error("chords: on a note, or in the gap after one");
        cursor_ = prog_->child(gapSeq_, gapPos_ - 1);
        gapSeq_ = 0;
    }
    if (!cursor_) return error("chords: on a note in an n / note pattern");
    const uint16_t v = valueOf(*prog_, cursor_);
    const Node& x = prog_->at(v);
    if (x.type != kTyPatBase + (int)Base::Note) return error("chords: on a note in an n / note pattern");
    int root = 0;
    bool chord = false;
    if (x.kind == Kind::MNote || ((x.kind == Kind::MNum) && x.b == 1)) {
        root = (int32_t)x.a;
    } else if (x.kind == Kind::MChord) {
        const char* w = prog_->str((int)x.a);
        const char* q = strchr(w, '\'');
        if (!q || !parseNoteName(w, (int)(q - w), root)) return error("chords: can't read this chord's root");
        chord = true;
    } else {
        return error("chords: on a whole-number note (0, e, c6 ...)");
    }
    if (const uint16_t par = prog_->parentOf(v))
        if (prog_->at(par).kind == Kind::MRange) return error("chords: not on a range's end (0 .. 7)");
    cursor_ = v;
    static const char* const kPc[12] = {"c", "cs", "d", "ef", "e", "f", "fs", "g", "af", "a", "bf", "b"};
    const int pc = ((root % 12) + 12) % 12, oct = 5 + (root - pc) / 12;
    Text name = kPc[pc];
    if (oct != 5) name = name + tnum(oct);
    MenuList& items = menuBuild();
    for (const char* c : {"maj", "min", "maj7", "dom7", "min7", "dim", "dim7", "aug", "sus2", "sus4", "six", "five"}) {
        const Text code = name + "'" + c;
        addReplaceItems(items, code, code, true);
    }
    if (chord) addReplaceItems(items, "single note " + tnum(root), tnum(root), true);
    menuVoices_ = true;
    openMenu("chord");
}

// Pad entry: replace the value at the cursor (mini code inside "...",
// exprCode for a number argument; empty = not allowed there), then select
// the next step's value or append a copy to type over (Delete removes it).
void Editor::enterStep(const Text& miniCode, const Text& exprCode, const Label& label) {
    if (gapSeq_) return insertAtGap(miniCode, label);
    const uint16_t oldCursor = cursor_;
    if (prog_->at(cursor_).kind == Kind::Mini) cursor_ = prog_->at(cursor_).first;  // whole "..."
    bool mini = isMiniKind(prog_->at(cursor_).kind);
    Text code = mini ? miniCode : exprCode;
    if (code.empty()) {
        cursor_ = oldCursor;
        return error(label + " only goes inside \"...\"");
    }
    uint16_t nn;
    EditSpec sp = spec(EditKind::Replace, cursor_, code);
    sp.mini = mini;
    if (!openSpec(sp, nn)) {
        cursor_ = oldCursor;
        return error(label + " doesn't fit here");
    }
    Program* r = prog_;  // the edit is open: the follow-up below is part of it
    uint16_t cur = nn, appended = 0;
    if (mini) {
        // replacing a whole sequence leaves a one-step sequence: use its step
        while (r->at(cur).kind == Kind::MSeq && r->at(cur).first && !r->at(r->at(cur).first).next)
            cur = r->at(cur).first;
        uint16_t step = stepOf(*r, cur), par = r->parentOf(step);
        if (par && r->at(par).kind == Kind::MRange) {
            if (r->at(step).next) {
                par = 0;  // lo: go on to hi
                cur = r->at(step).next;
            } else {
                step = stepOf(*r, par);  // hi: continue after the range
                par = r->parentOf(step);
            }
        }
        if (par && r->at(par).kind == Kind::MSeq) {
            // then the gap after the step: the next pads insert there (Delete
            // there removes the step before, the arrows move on)
            int idx = 0;
            for (uint16_t c = r->at(par).first; c && c != step; c = r->at(c).next) idx++;
            if (!endEdit(par)) {
                cursor_ = oldCursor;
                return;
            }
            gapSeq_ = par;
            gapPos_ = idx + 1;
            cursor_ = par;
            return info("= " + label + "  (gap: more pads insert more)");
        }
    }
    if (!endEdit(cur)) {
        cursor_ = oldCursor;
        return;
    }
    pending_ = appended;
    info("= " + label);
}

void Editor::gotoSlot(int s) {
    if (s < 0 || s >= kSlots) return;
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
            return info(m.printf("new d%d block: Replace / Note / Instrument / Screen 1-8 start it, Up / Down leave it",
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

void Editor::selectBlock(uint16_t b) {
    cps_ = false;
    const int s = prog_->blockSlot(b);
    prog_->block[s] = b;
    slot_ = s;
    pending_ = 0;
    gapSeq_ = 0;
    cursor_ = b;
}

// Insert, then slot pad N at a block's top: a new block for dN (silence)
// above the current block; the cursor on `silence`, ready for Replace.
void Editor::newBlockAbove(int slot) {
    if (!beginEdit()) return;
    Program* p = prog_;
    const uint16_t cur = prog_->block[slot_];
    int pos = cur ? p->blockIndex(cur) : -1;
    if (pos < 0) pos = p->blockCount();
    uint16_t b = p->alloc(Kind::Block);
    Error e;
    uint16_t x = b ? parseExpr(*p, "silence", 7, e) : 0;
    if (!b || !x) return abortEdit(), error("program full");
    p->at(b).a = (uint32_t)slot;
    p->appendChild(b, x);
    if (!p->insertBlock(pos, b)) return abortEdit(), error("too many blocks (max " + tnum(kMaxBlocks) + ")");
    p->block[slot] = b;
    slot_ = slot;  // the check runs on the new block
    if (!endEdit(x)) return;
    if (state_[slot] == SlotState::Empty) state_[slot] = SlotState::Edited;
    info("new block for d" + tnum(slot + 1) + ": Replace the silence to start");
}

void Editor::selectMenu(int i) {
    if (menuOpen_ && i >= 0 && i < (int)menu_.size()) menuSel_ = i;
}

Label Editor::cursorInfo() const {
    if (gapSeq_ && listGap()) {
        const int count = prog_->childCount(gapSeq_);
        Label m;
        return m.printf("list gap %d of %d: Insert copies an element in, Paste / Replace insert, Delete removes "
                        "the one before",
                        gapPos_, count),
               m;
    }
    if (gapSeq_) {
        const int count = prog_->childCount(gapSeq_);
        Label m;
        return m.printf("gap %d of %d: pads / Screen 3-7 insert, Delete removes the step before", gapPos_, count), m;
    }
    if (!cursor_)
        return "empty: Replace, Instrument, Note, Screen 1-8 or FX start a block; Up / Down leave";
    if (cps_ && cursor_ == prog_->cpsExpr && prog_->at(cursor_).kind != Kind::Num)
        return "setcps (Right: its numbers, Down: the blocks, jog: the BPM, Pattern sends it)";
    const Node& n = prog_->at(cursor_);
    if (n.kind == Kind::Block) {  // the block's top: Up / Down = other blocks, Right = into it
        int k = 0, count = 0;
        for (int i = 0; i < prog_->blockCount(); i++)
            if (prog_->blockSlot(prog_->blockAt(i)) == slot_) {
                count++;
                if (prog_->blockAt(i) == cursor_) k = count;
            }
        Label s, of;
        if (count > 1) of.printf(" %d of %d", k, count);
        return s.printf("d%d block%s%s (Up / Down: blocks, Right: into it, Insert + pad: new block above)", slot_ + 1,
                        of.c_str(), cursor_ == playing_[slot_] ? " (playing)" : ""),
               s;
    }
    Label s = kindName(n.kind);
    if (n.kind == Kind::Fn || n.kind == Kind::Op || n.kind == Kind::SectionL || n.kind == Kind::SectionR) {
        s += " ";
        s += lib((int)n.a).name;
    }
    if (n.type) {
        s += " : ";
        s += tyName(n.type);
    }
    return s;
}

// ---------------------------------------------------------------------------
// actions

void Editor::act(Action a) {
    padAgain_ = -1;  // a slot pad twice in a row only
    dropEmptyBlocks();
    actOne(a);
    dropEmptyBlocks();
}

void Editor::actOne(Action a) {
    if (kbdOpen_) return kbdAction(a);
    if (selSeq_ && !menuOpen_) {  // a selection lives until another kind of key
        switch (a) {
            case Action::SelLeft: case Action::SelRight: case Action::Struct3: case Action::Struct4:
            case Action::Struct5: case Action::Struct6: case Action::Struct7: case Action::Delete:
            case Action::Copy: case Action::Evaluate: case Action::PlayCancel:
                break;
            default: selA_ = selB_ = selSeq_ = 0; break;
        }
    }
    if (a == Action::SelLeft || a == Action::SelRight) return selectMove(a == Action::SelLeft ? -1 : 1);
    const bool wasOnOp = onOperator();
    switch (a) {  // these keep the cursor on the operator's symbol; the rest leave it
        case Action::WheelUp: case Action::WheelDown: case Action::WheelUpFast: case Action::WheelDownFast:
        case Action::Replace: case Action::Left: case Action::Right: case Action::Up:
        case Action::Evaluate: case Action::PlayCancel: case Action::Confirm: case Action::Cancel:
            break;
        default: onOp_ = false; break;
    }
    if (a != Action::Insert) newBlockArmed_ = false;  // only the very next slot pad
    if (cps_ && !menuOpen_) {
        switch (a) {
            case Action::Left: case Action::Right: case Action::Up: case Action::Down:
            case Action::WheelUp: case Action::WheelDown: case Action::WheelUpFast: case Action::WheelDownFast:
            case Action::Replace: case Action::Undo: case Action::Evaluate: case Action::Hush:
            case Action::Save: case Action::Copy: case Action::Paste: case Action::JumpCps:
            case Action::ToggleTrackerMode: case Action::PlayCancel: case Action::Confirm: case Action::Cancel:
            case Action::NextSlot: case Action::PrevSlot: case Action::Home: case Action::BlockUp:
            case Action::BlockDown:
                break;
            default: return error("on setcps: jog / Replace change it, Pattern sends it; Shift+Song or a slot pad goes back");
        }
    }
    // an empty slot: the keys that would wrap or add something start a block
    // (the starter patterns); Note / Instrument make one themselves
    if (!menuOpen_ && !cps_ && emptyRowIndex() >= 0) {
        switch (a) {
            case Action::Struct1: case Action::Struct2: case Action::Struct3: case Action::Struct4:
            case Action::Struct5: case Action::Struct6: case Action::Struct7: case Action::Struct8:
            case Action::MenuModifiers: case Action::MenuFx: case Action::Wrap: case Action::Insert:
            case Action::Quick1: case Action::Quick2: case Action::Quick3: case Action::Quick4:
            case Action::MenuFunctions: case Action::MenuControls:
                return openReplace();
            default: break;
        }
    }
    if (gapSeq_ && !menuOpen_ && listGap()) {
        switch (a) {
            case Action::Left: case Action::Right: case Action::Up: case Action::Down:
            case Action::Delete: case Action::Insert: case Action::Paste: case Action::Replace:
            case Action::PlayCancel: case Action::Evaluate: case Action::Hush: case Action::SilenceSlot:
                break;
            case Action::Wrap: case Action::Struct1: case Action::Struct2: case Action::Struct3:
            case Action::Struct4: case Action::Struct5: case Action::Struct6: case Action::Struct7:
            case Action::Struct8: case Action::MenuModifiers: case Action::MenuFx:
                return error("at a list gap: Insert / Paste / Replace add an element, Delete removes the one before");
            default: leaveGap(); break;  // act on the whole list
        }
    } else if (gapSeq_ && !menuOpen_) {
        switch (a) {
            case Action::Left: case Action::Right: case Action::Up: case Action::Down:
            case Action::Delete: case Action::Struct1: case Action::Struct2: case Action::Struct3:
            case Action::Struct4: case Action::Struct5: case Action::Struct6: case Action::Struct7:
            case Action::Struct8: case Action::MenuModifiers: case Action::MenuFx:
            case Action::PlayCancel: case Action::Evaluate: case Action::Hush: case Action::MenuChords:
            case Action::SilenceSlot:
                break;
            default: leaveGap(); break;  // act on the whole sequence
        }
    }
    uint16_t pending = pending_;
    pending_ = 0;  // only the very next Delete may remove an appended step
    if (a == Action::Delete && pending && pending == cursor_) pending_ = pending;
    if (menuOpen_) {
        menuTouched_ = true;  // a lazily checked menu down to one item no longer applies itself
        switch (a) {
            case Action::WheelUp:
            case Action::Down:
            case Action::Right: menuSel_ = (menuSel_ + 1) % (int)menu_.size(); return;
            case Action::WheelDown:
            case Action::Up:
            case Action::Left: menuSel_ = (menuSel_ + (int)menu_.size() - 1) % (int)menu_.size(); return;
            case Action::WheelUpFast: menuSel_ = std::min((int)menu_.size() - 1, menuSel_ + 8); return;
            case Action::WheelDownFast: menuSel_ = std::max(0, menuSel_ - 8); return;
            case Action::Confirm:
            case Action::Evaluate: return applyMenuItem(menuSel_, wasOnOp);
            case Action::Cancel: menuOpen_ = false; info("cancelled"); return;
            default: return;
        }
    }
    switch (a) {
        case Action::Left:
        case Action::Right:
        case Action::Up:
        case Action::Down: navigate(a); break;
        case Action::WheelUp: wheel(+1, false); break;
        case Action::WheelDown: wheel(-1, false); break;
        case Action::WheelUpFast: wheel(+1, true); break;
        case Action::WheelDownFast: wheel(-1, true); break;
        case Action::Replace: openReplace(); break;
        case Action::Wrap: openWrap(WrapMode::All); break;
        case Action::Copy: doCopy(); break;
        case Action::Paste: doPaste(); break;
        case Action::Home:  // the block's top (dN $ / jumpMod N k $); on setcps its line
            if (cps_) cursor_ = prog_->cpsExpr;
            else if (prog_->block[slot_]) cursor_ = prog_->block[slot_];
            info(cursorInfo());
            break;
        case Action::MenuNotes: openNotes(); break;
        case Action::MenuSamples: openSamples(); break;
        case Action::MenuChords: openChords(); break;
        case Action::MenuFunctions: openWrap(WrapMode::Functions); break;
        case Action::MenuControls: openWrap(WrapMode::Controls); break;
        case Action::MenuLaunch: openLaunch(); break;
        case Action::MenuFolders: openFolders(); break;
        case Action::BlockUp: blockStep(-1); break;
        case Action::BlockDown: blockStep(1); break;
        case Action::Struct1:
        case Action::Struct2:
        case Action::Struct3:
        case Action::Struct4:
        case Action::Struct5:
        case Action::Struct6:
        case Action::Struct7:
        case Action::Struct8: openStruct((int)a - (int)Action::Struct1); break;
        case Action::MenuModifiers: openWrap(WrapMode::Modifiers); break;
        case Action::PlayCancel:
            playUsed_ = true;  // releasing Play does nothing now
            info("cancelled");
            break;
        case Action::JumpNotes: jumpControl(true); break;
        case Action::JumpSamples: jumpControl(false); break;
        case Action::MenuFx: openWrap(WrapMode::Fx); break;
        case Action::Quick1: quick(0); break;
        case Action::Quick2: quick(1); break;
        case Action::Quick3: quick(2); break;
        case Action::Quick4: quick(3); break;
        case Action::Insert: {
            if (gapSeq_) {
                insertListCopy();
                break;
            }
            if (!cursor_) break;
            if (prog_->at(cursor_).kind == Kind::Block) {
                newBlockArmed_ = true;
                info("slot pad: a new block for that dN above this one");
                break;
            }
            uint16_t par = prog_->parentOf(cursor_);
            Kind pk = par ? prog_->at(par).kind : Kind::None;
            if (pk == Kind::MSeq || pk == Kind::List) {
                // duplicate this step / element after itself
                if (!beginEdit()) break;
                Program* p = prog_;
                uint16_t c = p->copyTree(*p, cursor_);
                if (!c) {
                    abortEdit();
                    error("out of nodes");
                    break;
                }
                p->at(c).next = p->at(cursor_).next;
                p->at(cursor_).next = c;
                if (endEdit(c)) info("inserted a copy");
            } else {
                openWrap(WrapMode::Controls);
            }
            break;
        }
        case Action::Delete: doDelete(); break;
        case Action::Unwrap: doUnwrap(); break;
        case Action::Undo:
            uint8_t tag;
            const uint8_t* st;
            int stLen;
            abortEdit();
            if (!popUndo(tag, st, stLen)) {
                error("nothing to undo");
            } else {
                if (tag == 'M') {  // a comment
                    int idx;
                    memcpy(&idx, st, sizeof idx);
                    if (idx >= 0 && idx < prog_->blockCount())
                        snprintf(prog_->blockComment(idx), kMaxCommentLen + 1, "%s", (const char*)st + 4);
                } else {
                    unpackCheckpoint(st, *cp_);
                    prog_->rollback(*cp_);
                }
                changes_++;
                if (cps_ && prog_->cpsExpr) {  // stay on setcps
                    cursor_ = prog_->cpsExpr;
                    info("undone");
                    break;
                }
                enterSlot(slot_);
                state_[slot_] = prog_->block[slot_] ? SlotState::Edited : SlotState::Empty;
                info("undone");
            }
            break;
        case Action::Evaluate: evaluate(); break;
        case Action::JumpCps: jumpCps(); break;
        case Action::EditComment: openCommentKeyboard(); break;
        case Action::SilenceSlot:
            if (playing_[slot_] || state_[slot_] == SlotState::Playing) {
                if (out_) out_->silence(slot_);
                playing_[slot_] = 0;
                if (state_[slot_] == SlotState::Playing) state_[slot_] = SlotState::Edited;
                info("d" + tnum(slot_ + 1) + " silenced (Pattern plays it again)");
            } else {
                info("d" + tnum(slot_ + 1) + " isn't playing");
            }
            break;
        case Action::Hush:
            if (out_) out_->hush();
            for (int s = 0; s < kSlots; s++)
                if (state_[s] == SlotState::Playing) state_[s] = SlotState::Edited;
            info("hush");
            break;
        case Action::NextSlot: enterSlot((slot_ + 1) % kSlots); info(cursorInfo()); break;
        case Action::PrevSlot: enterSlot((slot_ + kSlots - 1) % kSlots); info(cursorInfo()); break;
        case Action::Save: {
            Label msg;
            bool holes = false;
            for (int i = 1; i < prog_->nodeEnd() && !holes; i++) holes = prog_->at((uint16_t)i).kind == Kind::Hole;
            if (!out_) error("nowhere to save");
            else if (!out_->save(*prog_, msg)) error(msg);
            else if (holes) error(msg + ", but it has ?? holes: Tidal can't run those blocks");
            else info(msg);
            break;
        }
        case Action::ToggleTrackerMode:
            trackerMode_ = !trackerMode_;
            info(trackerMode_ ? "tracker mode on" : "tracker mode off");
            break;
        default: break;
    }
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
    if (onOp_ && p.at(cursor_).kind == Kind::Op && (a == Action::Left || a == Action::Right)) {
        const uint16_t l = p.at(cursor_).first, r = l ? p.at(l).next : 0;
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
        if (!par) return false;
        if (p.at(par).kind == Kind::Block) {  // the block's end: on into the next / previous block
            const int i = p.blockIndex(par) + (right ? 1 : -1);
            if (i < 0 || i >= p.blockCount()) return false;
            const uint16_t nb = p.blockAt(i);
            selectBlock(nb);
            if (p.at(nb).first) flowInto(right, p.at(nb).first);
            return true;
        }
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
        if (sib && pk == Kind::Op) {  // between an operator's operands: stop on its symbol
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

static uint16_t firstNumber(const Program& p, uint16_t n);

void Editor::wheel(int dir, bool fast) {
    if (!cursor_) return;
    if (cps_ && cursor_ == prog_->cpsExpr && prog_->at(cursor_).kind != Kind::Num) {
        const uint16_t top = cursor_, num = firstNumber(*prog_, top);
        if (!num) return;
        cursor_ = num;
        wheel(dir, fast);
        cursor_ = prog_->cpsExpr;  // stays on the line
        (void)top;
        return info(cursorInfo());
    }
    if (prog_->at(cursor_).kind == Kind::Fn) return wheelFn(dir);
    if (prog_->at(cursor_).kind == Kind::Op) return wheelOp(dir);
    if (!beginEdit()) return;
    Program* p = prog_;
    Node& n = prog_->at(cursor_);
    Node& m = n;  // in place
    switch (n.kind) {
        case Kind::Num:
        case Kind::MNum: {
            int dec = nodeDecimals(n);
            int64_t scale = 1;
            for (int k = 0; k < dec; k++) scale *= 10;
            Rational r = nodeRational(n);
            int64_t step = fast ? 10 : 1;
            // value + dir*step/scale
            int64_t num = (int64_t)r.num * scale + dir * step * (int64_t)r.den;
            Rational nr = makeRational(num, (int64_t)r.den * scale);
            setNodeRational(m, nr, dec);
            if ((int32_t)m.a < 0) m.flags |= kNegParen;
            else m.flags &= ~kNegParen;
            break;
        }
        case Kind::MNote:
            m.a = (uint32_t)((int32_t)m.a + dir * (fast ? 12 : 1));
            m.b = 0;
            break;
        case Kind::MBool: m.a = !m.a; break;
        case Kind::MWord: {
            if (fast) {
                int idx = (int)m.b - 1 + dir;  // -1 = no index
                m.b = idx < 0 ? 0 : (uint32_t)idx + 1;
            } else {
                const char* cur = p->str((int)m.a);
                int i = 0;
                for (; i < kInstrumentSlots; i++)
                    if (!strcmp(samples_[i], cur)) break;
                const int ns = kInstrumentSlots;
                int k = i < ns ? i : 0;
                for (int t = 0; t < ns; t++) {  // next non-empty slot
                    k = ((k + dir) % ns + ns) % ns;
                    if (samples_[k][0]) break;
                }
                if (!samples_[k][0]) return abortEdit(), error("no instruments");
                int id = p->intern(samples_[k], (int)strlen(samples_[k]));
                if (id < 0) return abortEdit(), error("too many words");
                m.a = (uint32_t)id;
            }
            break;
        }
        default: abortEdit(); error("the wheel changes numbers, notes, samples and functions"); return;
    }
    if (!endEdit(cursor_)) return;
    state_[slot_] = SlotState::Edited;
    info(renderNode(*prog_, cursor_));
}

// Wheel on a function name: step through the functions that type-check here.
void Editor::wheelFn(int dir) {
    FixVec<int, 256> ok;
    int cur = (int)prog_->at(cursor_).a;
    for (int id = 0; id < libCount(); id++) {
        if (libIsOp(id) || libIsAlias(id)) continue;
        if (id == cur) {
            ok.push_back(id);
            continue;
        }
        if (!beginEdit()) return;
        prog_->at(cursor_).a = (uint32_t)id;
        Text err;
        if (recheck(*prog_, err)) ok.push_back(id);
        abortEdit();
    }
    int at = 0;
    for (int i = 0; i < (int)ok.size(); i++)
        if (ok[i] == cur) at = i;
    int next = ok[(at + dir + (int)ok.size()) % (int)ok.size()];
    if (next == cur) return info("no other function fits here");
    if (!beginEdit()) return;
    prog_->at(cursor_).a = (uint32_t)next;
    if (endEdit(cursor_)) info(lib(next).name);
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
        if (playing_[s] == b) playing_[s] = 0;
        if (p->block[s] == b) {
            p->block[s] = 0;
            for (int k = 0; k < p->blockCount() && !p->block[s]; k++)  // another block of dN
                if (p->blockSlot(p->blockAt(k)) == s) p->block[s] = p->blockAt(k);
            if (s == slot_ && !cursor_ && emptyPos_ < 0) cursor_ = p->block[s];
            if (!p->block[s] && state_[s] != SlotState::Playing) state_[s] = SlotState::Empty;
        }
    }
}

// Sample Loader: the Tidal folders on the card; OK loads one (the output
// builds a sliced instrument in the background).
void Editor::openFolders() {
    MenuList& items = menuBuild();
    if (!out_) return error("no output");
    const int n = out_->folders();
    if (n <= 0) return error("no sample folders (put them in /Tidal/<name>/ on the card)");
    for (int i = 0; i < n && items.push_back(MenuItem{}); i++) items.back().label = out_->folderName(i);
    menuFolders_ = true;
    openMenu("load a Tidal folder");
}

void Editor::addReplaceItems(MenuList& items, const Label& label, const Text& code,
                             bool mini) {
    uint16_t nn;
    EditSpec sp;
    const bool ok = listGap() ? tryInsertListAt(mini ? "\"" + code + "\"" : code, nn, &sp)
                              : tryReplace(code, mini, nn, &sp);
    if (ok) items.push_back(MenuItem{label, sp, nn});
}

// Note button: pitches. Inside "..." single steps, otherwise note patterns.
void Editor::openNotes() {
    ensureBlock();
    if (!cursor_) return;
    MenuList& items = menuBuild();
    auto add = [&](const Text& l, const Text& code, bool mini) { addReplaceItems(items, l, code, mini); };
    if (isMiniKind(prog_->at(cursor_).kind)) {
        for (const char* v : {"c", "d", "e", "f", "g", "a", "b", "cs", "ef", "fs", "af", "bf", "c4", "c6",
                              "c'maj", "c'min", "c'maj7", "c'min7", "c'dom7", "c'sus4", "e'min", "g'maj"})
            add(v, v, true);
        for (int i = 0; i <= 12; i++) add(tnum(i), tnum(i), true);
        add("rest ~", "~", true);
    } else {
        for (const char* v : {"note \"c e g\"", "n \"0 .. 7\"", "n \"c'maj e'min\"",
                              "n (scale \"major\" \"0 .. 7\")", "n (arp \"up\" \"<c'maj e'min>\")",
                              "\"c e g\"", "\"0 .. 7\"", "\"<c'maj e'min>\"", "(scale \"major\" \"0 .. 7\")",
                              "(arp \"up\" \"<c'maj e'min>\")"})
            add(v, v, false);
    }
    openMenu("notes");
}

// Instrument button: samples. Inside "..." names (and name:n), otherwise s "name".
void Editor::openSamples() {
    ensureBlock();
    if (!cursor_) return;
    MenuList& items = menuBuild();
    bool mini = isMiniKind(prog_->at(cursor_).kind);
    for (int si = 0; si < kInstrumentSlots; si++) {
        const Text s = samples_[si];
        if (s.empty()) continue;
        if (mini) {
            addReplaceItems(items, s, s, true);
            for (int i = 1; i <= 3; i++) addReplaceItems(items, s + ":" + tnum(i), s + ":" + tnum(i), true);
        } else {
            addReplaceItems(items, "s \"" + s + "\"", "s \"" + s + "\"", false);
        }
    }
    openMenu("samples");
}

void Editor::doCopy() {
    int selPos;
    if (selRange(selPos)) {  // the run, as one step
        clip_ = "[";
        clip_ += selText();
        clip_ += ']';
        clipMini_ = true;
        return info("copied " + clip_);
    }
    if (!cursor_) return error("nothing to copy");
    if (prog_->at(cursor_).kind == Kind::Block) return duplicateBlock();
    uint16_t n = cursor_;
    if (!n) return error("nothing to copy");
    clip_ = renderNode(*prog_, n);
    clipMini_ = isMiniKind(prog_->at(n).kind);
    info("copied " + clip_);
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

static uint16_t firstNumber(const Program& p, uint16_t n) {
    if (!n) return 0;
    Kind k = p.at(n).kind;
    if (k == Kind::Num || k == Kind::MNum) return n;
    for (uint16_t c = p.at(n).first; c; c = p.at(c).next)
        if (uint16_t f = firstNumber(p, c)) return f;
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
// mute / solo (Play as a modifier)

void Editor::setMuted(int s, bool on) {
    if (muted_[s] == on) return;
    muted_[s] = on;
    if (out_) out_->mute(s, on);
}

void Editor::playDown(bool shift) {
    playHeld_ = true;
    playShift_ = shift;
    playUsed_ = false;
    info(shift ? "Shift+Play: pad = solo, release = hush, Delete = cancel"
               : "Play: pad = mute / unmute, release = unmute all, Delete = cancel");
}

void Editor::playUp() {
    if (!playHeld_) return;
    playHeld_ = false;
    if (playUsed_) return;
    if (playShift_) {
        act(Action::Hush);
        return;
    }
    bool any = false;
    for (int s = 0; s < kSlots; s++) any |= muted_[s];
    for (int s = 0; s < kSlots; s++) muted_[s] = false;
    if (out_) out_->unmuteAll();
    info(any ? "unmuted all" : "nothing was muted");
}

void Editor::mutePad(int slot) {
    padAgain_ = -1;
    if (!playHeld_ || slot < 0 || slot >= kSlots) return;
    playUsed_ = true;
    const Text d = "d" + tnum(slot + 1);
    if (!playShift_) {
        setMuted(slot, !muted_[slot]);
        info(d + (muted_[slot] ? " muted" : " unmuted"));
        return;
    }
    // solo: everything else muted; soloing the soloed slot again unmutes all
    bool soloed = !muted_[slot];
    for (int s = 0; s < kSlots && soloed; s++)
        if (s != slot && !muted_[s]) soloed = false;
    if (soloed) {
        for (int s = 0; s < kSlots; s++) muted_[s] = false;
        if (out_) out_->unmuteAll();
        info(d + " unsoloed: all unmuted");
        return;
    }
    for (int s = 0; s < kSlots; s++) setMuted(s, s != slot);
    info(d + " solo");
}

// The quoted pattern of the first `name "..."` (or `alt "..."`) in the tree
// under `n`, preorder; 0 if none.
// The "..." of a control's argument; through operators (`"0,7,10" +| "0 1 2"`)
// it follows the side the structure comes from: +| the right operand, |+
// the left, + / |+| the left.
static uint16_t structMini(const Program& p, uint16_t n) {
    for (int depth = 0; n && depth < 16; depth++) {
        const Node& x = p.at(n);
        if (x.kind == Kind::Mini) return n;
        if (x.kind != Kind::Op || !x.first) return 0;
        const char* op = lib((int)x.a).name;
        const int len = (int)strlen(op);
        const bool right = len >= 2 && op[len - 1] == '|' && op[0] != '|';
        n = right ? p.at(x.first).next : x.first;
    }
    return 0;
}

static uint16_t findControlMini(const Program& p, uint16_t n, const char* name, const char* alt) {
    if (!n) return 0;
    const Node& x = p.at(n);
    if (x.kind == Kind::App && x.first && p.at(x.first).kind == Kind::Fn) {
        const char* fn = lib((int)p.at(x.first).a).name;
        uint16_t arg = p.at(x.first).next;
        if ((!strcmp(fn, name) || !strcmp(fn, alt)) && arg)
            if (const uint16_t m = structMini(p, arg)) return m;
    }
    for (uint16_t c = x.first; c; c = p.at(c).next)
        if (uint16_t f = findControlMini(p, c, name, alt)) return f;
    return 0;
}

// Note / Instrument (v2): the cursor goes to the first step of the block's
// n / note (or s / sound) pattern; a block without one gets `# n "0"` /
// `# s "bd"` on its whole pattern first.
void Editor::jumpControl(bool notes) {
    const char* what = notes ? "n" : "s";
    if (!notes && slot_ >= kSampleSlots) return error("MIDI blocks (d9-d16) have no s pattern");
    if (emptyRowIndex() >= 0) {
        // the empty placeholder: start the block with the pattern this key jumps to
        const char* firstIns = "bd";
        for (int i = 0; i < kInstrumentSlots; i++)
            if (samples_[i][0]) {
                firstIns = samples_[i];
                break;
            }
        Text code;
        if (!notes) code.printf("s \"%s\"", firstIns);
        else if (slot_ >= kSampleSlots) code = "n \"0\"";
        else code.printf("n \"0\" # s \"%s\"", firstIns);
        ensureBlock();
        uint16_t nn;
        EditSpec sp = spec(EditKind::Replace, cursor_, code);
        if (!cursor_ || !doSpec(sp, nn)) return error(Label("can't start a block with ") + code.c_str());
    }
    uint16_t b = prog_->block[slot_];
    if (!b || !exprRoot()) return error("empty slot: replace to start a pattern");
    uint16_t mini = notes ? findControlMini(*prog_, b, "n", "note") : findControlMini(*prog_, b, "s", "sound");
    if (!mini) {
        const uint16_t old = cursor_;
        cursor_ = exprRoot();
        uint16_t nn;
        EditSpec sp;
        if (!tryWrapOp("#", notes ? "n \"0\"" : "s \"bd\"", true, nn, &sp)) {
            cursor_ = old;
            return error(Text("can't add # ") + what + " to this block");
        }
        if (!doSpec(sp, nn)) return;
        mini = notes ? findControlMini(*prog_, prog_->block[slot_], "n", "note")
                     : findControlMini(*prog_, prog_->block[slot_], "s", "sound");
        if (!mini) return;
    }
    uint16_t step = prog_->at(mini).first;
    while (step && prog_->at(step).kind == Kind::MSeq && prog_->at(step).first) step = prog_->at(step).first;
    cursor_ = step ? valueOf(*prog_, step) : mini;
    info(Text(what) + " pattern: " + (notes ? "pads enter notes" : "pads pick instruments"));
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

// Screen 1-8 (v2, docs/tidal_ui_v2.md): structure menus at the cursor.
void Editor::openStruct(int i) {
    switch (i) {
        case 0: return openWrap(WrapMode::Dollar);
        case 1: return openWrap(WrapMode::Controls);
        case 7: return openWrap(WrapMode::Ops);
        default: return openMini(i - 2);
    }
}

// Code from a pattern: '$' = t, '@' = step (t as one step); in quotes if
// `quote`. False if it doesn't fit.
static bool fillPattern(Text& out, const char* pat, const Text& t, const Text& step, bool quote) {
    out.clear();
    if (quote) out += '"';
    for (const char* c = pat; *c; c++) {
        if (*c == '$') out += t;
        else if (*c == '@') out += step;
        else out += *c;
    }
    if (quote) out += '"';
    return !out.overflowed();
}

// [] <> {} () | menus: the patterns per key (Screen 3-7), for a step inside
// "..." (with the old one-touch wrap first), at a gap, and on a number
static const char* const kMiniPats[5][5] = {
    {"[$]", "[$ $]", "[$ $ $]", "[$, $]", nullptr},
    {"<$>", "<$ ~>", "<$ $>", nullptr, nullptr},
    {"{$}%4", "{$ $ $}%4", "{$, $ $ $}", nullptr, nullptr},
    {"@(3,8)", "@(5,8)", "@(3,8,2)", "@(<3 5>,8)", nullptr},
    {"[$ | ~]", "[$ | $]", "[$ | $ | ~]", nullptr, nullptr},
};
static const char* const kGapPats[5][4] = {
    {"[$ $]", "[$ $ $]", "[$, $]", nullptr},
    {"<$ ~>", "<$ $>", nullptr, nullptr},
    {"{$ $ $}%4", "{$, $ $ $}", nullptr, nullptr},
    {"$(3,8)", "$(5,8)", "$(3,8,2)", nullptr},
    {"[$ | ~]", "[$ | $]", nullptr, nullptr},
};
static const char* const kNumPats[5][4] = {
    {"[$ $]", "[$ $ $]", "[$, $]", nullptr},
    {"<$ $>", "<$ ~>", nullptr, nullptr},
    {"{$ $ $}%4", "{$, $ $ $}", nullptr, nullptr},
    {"$(3,8)", "$(5,8)", "$(3,8,2)", nullptr},
    {"[$ | ~]", "[$ | $]", nullptr, nullptr},
};
static const char* const kMiniTitle[5] = {"[ ]", "< >", "{ }", "( , )", "|"};

// [] <> {} () | around the step(s) at the cursor (inside "..."); the first
// item of each is what the old one-touch buttons did.
void Editor::openMini(int k) {
    if (k < 0 || k > 4) k = 4;
    int selPos;
    const int selN = selRange(selPos);
    if (selN > 1 && k == 4) {  // | on a selection: [a | b | c] at once (Editor13 note)
        EditSpec sp;
        sp.kind = EditKind::ReplaceRun;
        sp.mini = true;
        sp.seq = selSeq_;
        sp.pos = selPos;
        sp.cycles = (uint32_t)selN;
        sp.code = "[" + joinSteps(selSeq_, selPos, selN, " | ") + "]";
        uint16_t nn;
        selA_ = selB_ = selSeq_ = 0;
        if (sp.code.overflowed()) return error("selection too long");
        if (doSpec(sp, nn)) info(sp.code);
        return;
    }
    if (selN > 0) {  // the selected run, wrapped as one step
        static const char* const kRunPats[5][4] = {
            {"[$]", nullptr, nullptr, nullptr},
            {"<$>", nullptr, nullptr, nullptr},
            {"{$}%4", "{$}%8", nullptr, nullptr},
            {"@(3,8)", "@(5,8)", "@(3,8,2)", nullptr},
            {"[$ | ~]", nullptr, nullptr, nullptr},
        };
        const Text t = selText();
        Text step = "[";
        step += t;
        step += ']';
        MenuList& items = menuBuild();
        for (const char* pat : kRunPats[k]) {
            if (!pat || !items.push_back(MenuItem{})) continue;
            MenuItem& it = items.back();
            it.spec.kind = EditKind::ReplaceRun;
            it.spec.mini = true;
            it.spec.seq = selSeq_;
            it.spec.pos = selPos;
            it.spec.cycles = (uint32_t)selN;
            if (!fillPattern(it.spec.code, pat, t, step, false)) {
                items.pop_back();
                continue;
            }
            it.label = it.spec.code;
        }
        Label title = kMiniTitle[k];
        title += " (selection)";
        return (void)openMenu(title);
    }
    if (gapSeq_ && k == 4 && !listGap()) {
        // | at a gap: the steps either side as [a | b], or | between all the
        // sequence's steps (Editor13 note)
        const Program& p = *prog_;
        const int count = p.childCount(gapSeq_);
        const uint16_t par = p.parentOf(gapSeq_);
        const bool group = par && p.at(par).kind == Kind::MSub && p.childCount(par) == 1;  // [a b c]
        MenuList& items = menuBuild();
        auto run = [&](int pos, int n, const Label& label) {
            if (!items.push_back(MenuItem{})) return;
            MenuItem& it = items.back();
            it.spec.kind = EditKind::ReplaceRun;
            it.spec.mini = true;
            it.spec.seq = gapSeq_;
            it.spec.pos = pos;
            it.spec.cycles = (uint32_t)n;
            it.spec.code = "[" + joinSteps(gapSeq_, pos, n, " | ") + "]";
            it.label = label + it.spec.code;
            if (it.spec.code.overflowed()) items.pop_back();
        };
        const bool both = gapPos_ > 0 && gapPos_ < count;
        if (both && !(count == 2 && group)) run(gapPos_ - 1, 2, "either side: ");
        if (count >= 2) {
            if (group) {  // the group itself becomes the choice: no [[...]]
                if (items.push_back(MenuItem{})) {
                    MenuItem& it = items.back();
                    it.spec = spec(EditKind::Replace, par, "[" + joinSteps(gapSeq_, 0, count, " | ") + "]");
                    it.spec.mini = true;
                    it.label = "all: " + it.spec.code;
                    if (it.spec.code.overflowed()) items.pop_back();
                }
            } else if (!(both && count == 2)) {
                run(0, count, "all: ");
            }
        }
        if (!items.empty()) return (void)openMenu("|");
    }
    if (gapSeq_) {
        // at a gap: insert, built from the neighbouring step
        const Program& p = *prog_;
        const uint16_t nb = gapPos_ > 0 ? p.child(gapSeq_, gapPos_ - 1) : p.child(gapSeq_, 0);
        const Text t = nb ? renderNode(p, nb) : Text("~");
        MenuList& items = menuBuild();
        for (const char* pat : kGapPats[k]) {
            if (!pat || !items.push_back(MenuItem{})) continue;
            MenuItem& it = items.back();
            it.spec.kind = EditKind::InsertGap;
            it.spec.seq = gapSeq_;
            it.spec.pos = gapPos_;
            if (!fillPattern(it.spec.code, pat, t, t, false)) {
                items.pop_back();
                continue;
            }
            it.label = "insert ";
            it.label += it.spec.code;
        }
        Label title = "insert ";
        title += kMiniTitle[k];
        return (void)openMenu(title);
    }
    if (!cursor_) return error("select a step inside \"...\" first");
    Kind kind = prog_->at(cursor_).kind;
    const uint16_t oldCursor = cursor_;
    if (kind == Kind::Mini) {  // on the quotes: the whole sequence inside them
        cursor_ = prog_->at(cursor_).first;
        kind = cursor_ ? prog_->at(cursor_).kind : Kind::None;
    }
    const bool num = kind == Kind::Num;
    if (!num && !isMiniKind(kind)) {
        cursor_ = oldCursor;
        return error("inside \"...\" only");
    }
    Text t = renderNode(*prog_, cursor_);
    if (num) {
        // a plain number argument (`fast 2`): make it a pattern, "[2 2]" ...
        if (t.size() > 2 && t[0] == '(' && t.back() == ')') t = t.substr(1, t.size() - 2);  // (-1)
        if (t.find('/') != Text::npos) return error("a fraction can't go inside \"...\"; jog it first");
    }
    const bool multi = kind == Kind::MSeq && prog_->at(cursor_).first && prog_->at(prog_->at(cursor_).first).next;
    Text step;  // modifiers bind to one step
    if (multi) {
        step = "[";
        step += t;
        step += ']';
    } else {
        step = t;
    }
    MenuList& items = menuBuild();
    const char* const* pats = num ? kNumPats[k] : kMiniPats[k];
    const int npats = num ? 4 : 5;
    for (int i = 0; i < npats; i++) {
        if (!pats[i] || !items.push_back(MenuItem{})) continue;
        MenuItem& it = items.back();
        it.spec.kind = EditKind::Replace;
        it.spec.mini = !num;
        it.spec.target = cursor_;
        if (!fillPattern(it.spec.code, pats[i], t, step, num)) {
            items.pop_back();
            continue;
        }
        it.label = it.spec.code;
    }
    Label title = kMiniTitle[k];
    if (num) title += " (number -> pattern)";
    if (!openMenu(title)) cursor_ = oldCursor;
}

void Editor::wheelOp(int dir) {
    int ok[48];
    int n = 0;
    const int cur = (int)prog_->at(cursor_).a;
    for (int id = 0; id < libCount() && n < 48; id++) {
        if (!libIsOp(id)) continue;
        if (id == cur) {
            ok[n++] = id;
            continue;
        }
        EditSpec s;
        s.kind = EditKind::SetOp;
        s.target = cursor_;
        s.launch = id;
        uint16_t nn;
        if (trySpec(s, nn)) ok[n++] = id;
    }
    int at = 0;
    for (int i = 0; i < n; i++)
        if (ok[i] == cur) at = i;
    if (n < 2) return info("no other operator fits here");
    const int next = ok[(at + dir + n) % n];
    EditSpec s;
    s.kind = EditKind::SetOp;
    s.target = cursor_;
    s.launch = next;
    uint16_t nn;
    if (doSpec(s, nn)) {
        onOp_ = true;
        info(lib(next).name);
    }
}

void Editor::openOps() {
    MenuList& items = menuBuild();
    const int cur = (int)prog_->at(cursor_).a;
    for (int id = 0; id < libCount(); id++) {
        if (!libIsOp(id) || id == cur || !items.push_back(MenuItem{})) continue;
        MenuItem& it = items.back();
        it.spec.kind = EditKind::SetOp;
        it.spec.target = cursor_;
        it.spec.launch = id;
        it.label = lib(id).name;
    }
    (void)openMenu("operator");
}

void Editor::openReplace() {
    if (cursor_ && prog_->at(cursor_).kind == Kind::Op && onOp_) return openOps();
    MenuList& items = menuBuild();
    auto add = [&](const Text& label, const Text& code, bool mini) {
        addReplaceItems(items, label, code, mini);
    };
    if (cps_) {  // setcps: numbers only (silence would type-check but set no tempo)
        if (cursor_ == prog_->cpsExpr && prog_->at(cursor_).kind != Kind::Num) {  // the line: whole tempos
            for (const char* v : {"90", "100", "110", "120", "128", "130", "138", "140", "150", "160", "170", "174"})
                add(Text(v) + " BPM", Text(v) + "/60/4", false);
            add("0.5 cycles / s", "0.5", false);
            return (void)openMenu("tempo");
        }
        for (const char* v : {"90", "100", "110", "120", "128", "130", "138", "140", "150", "160", "170", "174",
                              "60", "4", "3", "0.5", "1"})
            add(v, v, false);
        return (void)openMenu("tempo");
    }
    ensureBlock();
    if (!cursor_) return;
    const Node& n = prog_->at(cursor_);
    if (n.kind == Kind::Block || cursor_ == exprRoot()) {
        bool midi = slot_ >= kSampleSlots;
        if (midi) {
            add("note pattern", "n \"c e g\"", false);
            add("arpeggio", "n (arp \"up\" \"<c'maj e'min>\")", false);
            add("scale run", "n (scale \"major\" \"0 .. 7\")", false);
        } else {
            add("drum pattern", "s \"bd*2 [~ sd] bd sd\"", false);
            add("hi-hats", "s \"hh*8\" # gain 0.8", false);
            add("euclid", "s \"bd(3,8) sd(<3 5>,8,2)\"", false);
            add("melody", "n \"0 .. 7\" # s \"arpy\"", false);
            add("notes", "note \"c e g\" # s \"superpiano\"", false);
        }
        add("silence", "silence", false);
    }
    if (isMiniKind(n.kind)) {
        Text t = renderNode(*prog_, cursor_);
        add("rest ~", "~", true);
        for (int si = 0; si < kInstrumentSlots; si++)
            if (samples_[si][0]) add(samples_[si], samples_[si], true);
        for (const char* v : {"0", "1", "2", "3", "0.5", "c", "e", "g", "t", "f"}) add(v, v, true);
        add("group [x x]", "[" + t + " " + t + "]", true);
        add("alternate <x x>", "<" + t + " " + t + ">", true);
        add("stack [x, x]", "[" + t + ", " + t + "]", true);
    } else if (n.kind != Kind::Block) {
        for (const char* v : {"silence", "1", "0.5", "2", "(1/8)", "\"1 2\"", "\"<0 2>\"", "\"bd sd\"",
                              "\"c e g\"", "\"t f t t\"", "sine", "saw", "rand", "rev", "(fast 2)",
                              "(# speed 2)", "(0.25 ~>)"})
            add(v, v, false);
        for (int id = 0; id < libCount(); id++) {
            const LibEntry& e = lib(id);
            if (libIsOp(id)) continue;
            Text code = e.name;
            if (e.cat == Cat::Control) code += Text(" ") + controlDefault(e.name);
            else if (e.defaults) code += Text(" ") + e.defaults;
            add(code, code, false);
        }
    }
    openMenu(listGap() ? "insert" : "replace");
}

void Editor::openWrap(WrapMode mode) {
    if (!cursor_ || prog_->at(cursor_).kind == Kind::Block) return error("select a pattern first");
    bool controlsOnly = mode == WrapMode::Controls || mode == WrapMode::Fx;
    const bool patternLevel = controlsOnly || mode == WrapMode::Dollar || mode == WrapMode::Modifiers ||
                              mode == WrapMode::Ops;
    // the "..." the cursor is in (or on), for numeric operators (Screen 8)
    uint16_t numMini = 0;
    if (mode == WrapMode::Ops) {
        uint16_t m = cursor_;
        while (m && isMiniKind(prog_->at(m).kind)) m = prog_->parentOf(m);
        if (m && prog_->at(m).kind == Kind::Mini) {
            const uint8_t ty = prog_->at(m).type;
            if (ty == kTyPatBase + (int)Base::Time || ty == kTyPatBase + (int)Base::Double ||
                ty == kTyPatBase + (int)Base::Int || ty == kTyPatBase + (int)Base::Note)
                numMini = m;
        }
    }
    if (patternLevel) {
        // from inside "..." a control goes on the enclosing pattern
        uint16_t up = cursor_;
        while (up && (isMiniKind(prog_->at(up).kind) || prog_->at(up).kind == Kind::Mini)) up = prog_->parentOf(up);
        if (up && up != cursor_) {
            if (prog_->at(up).kind == Kind::Block) return error("select a pattern first");
            uint16_t par = prog_->parentOf(up);
            // `s "bd"` is an App: step out of its argument to the whole App
            if (par && prog_->at(par).kind == Kind::App && prog_->at(up).kind != Kind::App) up = par;
            cursor_ = up;
        }
    }
    MenuList& items = menuBuild();
    const Node& n = prog_->at(cursor_);
    if (isMiniKind(n.kind)) {
        Text t = renderNode(*prog_, cursor_);
        auto add = [&](const Text& label, const Text& code) {
            uint16_t nn;
            EditSpec sp;
            if (tryReplace(code, true, nn, &sp)) items.push_back(MenuItem{label, sp, nn});
        };
        for (const char* suf : {"*2", "/2", "!2", "@2", "?", "(3,8)", "*<2 3>", " _"})
            add(t + suf, t + suf);
        add("[" + t + " " + t + "]", "[" + t + " " + t + "]");
        add("<" + t + " ~>", "<" + t + " ~>");
        add("[" + t + ", " + t + "]", "[" + t + ", " + t + "]");
        return (void)openMenu("wrap");
    }
    auto addOp = [&](const char* op, const Text& rhs, bool left, const Text& label) {
        uint16_t nn;
        EditSpec sp;
        if (tryWrapOp(op, rhs, left, nn, &sp)) items.push_back(MenuItem{label, sp, nn});
    };
    auto addControls = [&]() {
        for (int id = 0; id < libCount(); id++) {
            const LibEntry& e = lib(id);
            if (e.cat != Cat::Control) continue;
            Text rhs = Text(e.name) + " " + controlDefault(e.name);
            addOp("#", rhs, true, "# " + rhs);
        }
    };
    // sound FX controls (FX2): filters, reverb / delay sends, drive, crush,
    // pan, level, envelope
    static const char* const kFx[] = {"cutoff", "resonance", "hcutoff", "hresonance", "bandf", "lpf", "lpq",
                                      "hpf",    "hpq",       "bpf",     "room",       "delay", "crush",
                                      "shape",  "distort",   "pan",     "gain",       "amp",   "attack",
                                      "release", "att",      "rel"};
    auto isFx = [&](const char* name) {
        for (const char* f : kFx)
            if (!strcmp(f, name)) return true;
        return false;
    };
    // A function value (often's `rev`, every's `(fast 2)`, a section): the
    // menus that would wrap it offer what can replace it instead.
    if (prog_->at(cursor_).type == kTyFn && mode != WrapMode::All) {
        auto add = [&](const Text& code) { addReplaceItems(items, code, code, false); };
        if (mode == WrapMode::Dollar || mode == WrapMode::Modifiers || mode == WrapMode::Functions) {
            for (int id = 0; id < libCount(); id++) {
                const LibEntry& e = lib(id);
                if (libIsOp(id) || libIsAlias(id) || e.cat == Cat::Control) continue;
                if (mode == WrapMode::Modifiers && !(e.cat == Cat::Time || e.cat == Cat::Structure ||
                                                     e.cat == Cat::Random || e.cat == Cat::Layer ||
                                                     e.cat == Cat::Sample))
                    continue;
                add(e.defaults ? Text(e.name) + " " + e.defaults : Text(e.name));
            }
            for (const char* s : {"(0.25 <~)", "(0.25 ~>)", "(|+ n 12)", "(|* speed 2)", "(# speed 2)"}) add(s);
        } else if (mode == WrapMode::Ops) {
            for (const char* op : {"|+", "|-", "|*", "|/", "|<", "|>"})
                for (const char* rhs : {"n 12", "note 7", "speed 2", "gain 0.8", "pan 0.5", "cutoff 1000"})
                    add(Text("(") + op + " " + rhs + ")");
        } else {  // # controls (all, or FX2's sound FX)
            for (int id = 0; id < libCount(); id++) {
                const LibEntry& e = lib(id);
                if (e.cat != Cat::Control || (mode == WrapMode::Fx && !isFx(e.name))) continue;
                add(Text("(# ") + e.name + " " + controlDefault(e.name) + ")");
            }
        }
        return (void)openMenu("replace function");
    }
    if (mode == WrapMode::Fx) {
        for (int id = 0; id < libCount(); id++) {
            const LibEntry& e = lib(id);
            if (e.cat != Cat::Control || !isFx(e.name)) continue;
            Text rhs = Text(e.name) + " " + controlDefault(e.name);
            addOp("#", rhs, true, "# " + rhs);
        }
        return (void)openMenu("FX");
    }
    if (mode == WrapMode::Ops) {  // |+ |- |* |/ |< |> with a control
        if (numMini) {
            // on a number pattern: operators in front (`"0,7,10" +| x`) and behind
            const uint16_t keep = cursor_;
            cursor_ = numMini;
            static const struct {
                const char* op;
                const char* other;
                bool xLeft;
            } kNum[] = {
                {"+|", "\"0,7,10\"", false}, {"+|", "\"0,4,7\"", false}, {"+", "\"<0 3 5>\"", false},
                {"|+", "\"<0 12>\"", false}, {"|+", "12", true},             {"+", "\"<0 12>\"", true},
                {"*", "2", true},
            };
            for (const auto& o : kNum) {
                Text label;
                if (o.xLeft) label.printf("x %s %s", o.op, o.other);
                else label.printf("%s %s x", o.other, o.op);
                addOp(o.op, o.other, o.xLeft, label);
            }
            cursor_ = keep;
        }
        for (const char* op : {"|+", "|-", "|*", "|/", "|<", "|>"})
            for (const char* rhs : {"n 12", "note 7", "speed 2", "gain 0.8", "pan 0.5", "cutoff 1000"})
                addOp(op, rhs, true, Text(op) + " " + rhs);
        // structure from the right (op|) or both (|op|): a patterned right side
        for (const char* op : {">|", "<|", "+|", "|>|", "|+|"})
            for (const char* rhs : {"n \"0 3 5 7\"", "s \"bd*4\"", "gain \"1 0.8\""})
                if (!(op[0] == '+' || op[1] == '+') || rhs[0] != 's')  // no sums of names
                    addOp(op, rhs, true, Text(op) + " " + rhs);
        return (void)openMenu("operators");
    }
    // Haskell lists: x -> stack [x, x] ... (the copy is selected to edit)
    auto addLists = [&]() {
        for (const char* fn : {"stack", "cat", "fastcat"}) {
            uint16_t nn;
            EditSpec sp;
            if (tryWrapList(fn, nn, &sp)) items.push_back(MenuItem{Text(fn) + " [x, x]", sp, nn});
        }
    };
    if (mode == WrapMode::Dollar || mode == WrapMode::Modifiers) {
        // functions applied with $ (Screen 1: all of them; FX1: pattern
        // modifiers = time, structure, randomness, layering, sample cutting)
        for (int id = 0; id < libCount(); id++) {
            const LibEntry& e = lib(id);
            if (libIsOp(id) || libIsAlias(id) || e.cat == Cat::Control) continue;
            if (mode == WrapMode::Modifiers && !(e.cat == Cat::Time || e.cat == Cat::Structure ||
                                                 e.cat == Cat::Random || e.cat == Cat::Layer ||
                                                 e.cat == Cat::Sample))
                continue;
            Text head = e.name;
            if (e.defaults) head += Text(" ") + e.defaults;
            uint16_t nn;
            EditSpec sp;
            if (tryWrapApp(head, nn, &sp)) items.push_back(MenuItem{head + " $ ...", sp, nn});
        }
        addLists();  // last, so the menu's other items keep their places
        return (void)openMenu(mode == WrapMode::Dollar ? "$ functions" : "pattern modifiers");
    }
    if (controlsOnly) addControls();
    if (!controlsOnly) {
        addOp("|+", "n 12", true, "|+ n 12");
        addOp("|*", "speed 2", true, "|* speed 2");
        addOp("+", "\"<0 3 5>\"", true, "+ \"<0 3 5>\"");
        addOp("<~", "0.25", false, "0.25 <~");
        addOp("~>", "\"<0 0.25>\"", false, "\"<0 0.25>\" ~>");
        for (int id = 0; id < libCount(); id++) {
            const LibEntry& e = lib(id);
            if (libIsOp(id) || libIsAlias(id) || e.cat == Cat::Control) continue;
            Text head = e.name;
            if (e.defaults) head += Text(" ") + e.defaults;
            uint16_t nn;
            EditSpec sp;
            if (tryWrapApp(head, nn, &sp)) items.push_back(MenuItem{head + " $ ...", sp, nn});
        }
        if (mode == WrapMode::All) addControls();
        if (mode == WrapMode::All) addLists();
    }
    openMenu(controlsOnly ? "add control" : mode == WrapMode::Functions ? "functions" : "wrap");
}

// Delete inside "...": a step becomes a rest (timing stays), a rest is
// removed, and the step pad entry just appended is removed (ends entry).
void Editor::doDelete() {
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
            if (out_ && wasPlaying && state_[slot_] == SlotState::Playing) out_->silence(slot_);
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
    } else if (pk == Kind::Block) {
        uint16_t h = p->alloc(Kind::Hole);
        p->replace(cursor_, h);
        p->freeTree(cursor_);
        newCursor = h;
    } else {
        uint16_t h = p->alloc(Kind::Hole);
        p->replace(cursor_, h);
        p->freeTree(cursor_);
        newCursor = h;
    }
    while (isTransparent(*p, newCursor)) newCursor = p->at(newCursor).first;
    if (endEdit(newCursor)) info("deleted");
}

void Editor::doUnwrap() {
    if (!cursor_) return;
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
        if (!beginEdit()) return;
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
        return;
    }
    error("can't unwrap this");
}

// "d3", "jumpMod 3 4" or "jumpIn 3 2" for the slot's launch mode.

// Pattern button: when this block's next evaluation takes effect.
void Editor::openLaunch() {
    uint16_t b = prog_->block[slot_];
    if (!b || !prog_->at(b).first) return error("empty slot: write a pattern first");
    static const struct {
        Launch l;
        uint32_t k;
        const char* fmt;  // %d = the slot
    } opts[] = {
        {Launch::Now, 0, "d%d $  (now)"},
        {Launch::JumpMod, 2, "jumpMod %d 2  (every 2 cycles)"},
        {Launch::JumpMod, 4, "jumpMod %d 4  (every 4 cycles)"},
        {Launch::JumpMod, 8, "jumpMod %d 8  (every 8 cycles)"},
        {Launch::JumpMod, 16, "jumpMod %d 16  (every 16 cycles)"},
        {Launch::JumpIn, 1, "jumpIn %d 1  (after 1 cycle)"},
        {Launch::JumpIn, 2, "jumpIn %d 2  (after 2 cycles)"},
        {Launch::JumpIn, 4, "jumpIn %d 4  (after 4 cycles)"},
    };
    MenuList& items = menuBuild();
    int sel = 0;
    const Node& cur = prog_->at(b);
    for (auto& o : opts) {
        if (!items.push_back(MenuItem{})) break;
        MenuItem& it = items.back();
        it.spec.kind = EditKind::Launch;
        it.spec.target = b;
        it.spec.launch = (int)o.l;
        it.spec.cycles = o.k;
        it.cursor = cursor_;
        it.label.printf(o.fmt, slot_ + 1);
        if (o.l == blockLaunch(cur) && (o.l == Launch::Now || o.k == cur.b)) sel = items.size() - 1;
    }
    openMenu("launch");
    menuSel_ = sel;
}

// the rendered setcps expression ("" if none)
static Text cpsText(const Program& p) {
    char buf[256];
    Renderer r(buf, sizeof buf);
    buf[0] = 0;
    if (p.cpsExpr) r.expr(p, p.cpsExpr);
    return Text(buf);
}

void Editor::evaluate() {
    if (cps_) {
        if (!prog_->cpsExpr) return error("no setcps");
        Error e;
        if (!checkCps(*prog_, e)) return error(e.msg);
        lastCps_ = cpsText(*prog_);
        if (out_) out_->setCps(*prog_);
        return info("tempo sent: setcps (" + lastCps_ + ")");
    }
    uint16_t b = prog_->block[slot_];
    if (!b || !prog_->at(b).first || emptyRowIndex() >= 0) return error("nothing to evaluate");
    Error e;
    int holes = 0;
    if (!checkBlock(*prog_, slot_, e, &holes)) {
        state_[slot_] = SlotState::Error;
        return error(e.msg);
    }
    if (holes) return error("fill in the ?? first");
    if (trackerMode_ && !trackerCheck(*prog_, slot_, e)) return error(e.msg);
    if (prog_->cpsExpr && out_) {  // a changed setcps goes first
        const Text cps = cpsText(*prog_);
        if (cps != lastCps_) {
            out_->setCps(*prog_);
            lastCps_ = cps;
        }
    }
    Label msg;
    if (out_ && !out_->evaluate(*prog_, slot_, msg)) return error(msg);
    state_[slot_] = SlotState::Playing;
    playing_[slot_] = b;
    info("d" + tnum(slot_ + 1) + " evaluated");
}

bool Editor::pollHighlights(double now) {
    bool changed = false;
    if (out_) {
        EditorOutput::Lit got[64];
        int n;
        while ((n = out_->litSteps(got, 64)) > 0) {
            for (int i = 0; i < n; i++) {
                const EditorOutput::Lit& g = got[i];
                // short events still show: at least 80 ms
                LitStep l{g.slot, g.node, g.on, g.off > g.on + 0.08 ? g.off : g.on + 0.08};
                // a new onset ends the slot's earlier steps (a note held by
                // legato / a long sample doesn't keep its step lit over the
                // next one); steps with the same onset (chords, stacks) stay
                for (LitStep& o : lit_)
                    if (o.slot == l.slot && o.on < l.on - 0.001 && o.off > l.on) o.off = l.on;
                if (lit_.full()) lit_.erase_at(0);
                lit_.push_back(l);
                if (litSeen_.full()) litSeen_.erase_at(0);
                litSeen_.push_back(l);
                // the record arrives after its onset (often after the poll
                // that would have seen it turn on): a new step that is still
                // lit always redraws (Editor9: d1 / d2 missed highlights)
                if (l.off > now) changed = true;
            }
        }
    }
    // a redraw only when a step turns on or off since the last poll
    for (int i = lit_.size() - 1; i >= 0; i--) {
        const LitStep& l = lit_[i];
        if ((l.on > litPolled_ && l.on <= now) || l.off <= now) changed = true;
        if (l.off <= now) lit_.erase_at(i);
    }
    litPolled_ = now;
    return changed;
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

// ---------------------------------------------------------------------------
// setcps (Shift+Song)

bool Editor::cpsSent() const {
    if (!prog_->cpsExpr) return true;
    char buf[256];
    Renderer r(buf, sizeof buf);
    buf[0] = 0;
    r.expr(*prog_, prog_->cpsExpr);
    return lastCps_ == Text(buf);
}

void Editor::jumpCps() {
    if (cps_) {  // back to the slot
        enterSlot(slot_);
        return info(cursorInfo());
    }
    menuOpen_ = false;
    gapSeq_ = 0;
    pending_ = 0;
    if (!prog_->cpsExpr) {
        // Tidal 1.10.3's default tempo is 0.575 cycles / s = 138 BPM at 4
        // beats per cycle, so adding it changes nothing until edited
        if (!beginEdit()) return;
        Error e;
        const char* code = "138/60/4";
        uint16_t x = parseExpr(*prog_, code, (int)strlen(code), e);
        if (!x) return abortEdit(), error("program full");
        prog_->cpsExpr = x;
        cps_ = true;  // the check is the setcps one
        if (!endEdit(x)) return void(cps_ = false);
        char buf[256];
        Renderer r(buf, sizeof buf);
        buf[0] = 0;
        r.expr(*prog_, prog_->cpsExpr);
        if (lastCps_.empty()) lastCps_ = Text(buf);  // the backend's default anyway
    }
    cps_ = true;
    const uint16_t n = firstNumber(*prog_, prog_->cpsExpr);
    cursor_ = n ? n : prog_->cpsExpr;
    info("setcps: jog changes the value (BPM / 60 / beats per cycle), Pattern sends it, Shift+Song goes back");
}

// ---------------------------------------------------------------------------
// keyboard screen (Params): the block's comment

void Editor::openCommentKeyboard() {
    const uint16_t b = prog_->block[slot_];
    if (!b) return error("no block to comment (start one first)");
    const int idx = prog_->blockIndex(b);
    if (idx < 0) return error("block not in the file");
    const Text all = prog_->blockComment(idx);
    // edit the last line (the one right above the block); earlier lines stay
    const int nl = all.rfind('\n');
    const Text head = nl == Text::npos ? "" : all.substr(0, nl + 1);
    Text last = nl == Text::npos ? all : all.substr(nl + 1);
    if (last.startsWith("--")) last = last.substr(2);
    if (!last.empty() && last[0] == ' ') last = last.substr(1);
    kbdMax_ = std::min(48, kMaxCommentLen - (int)head.size() - 3);
    if (kbdMax_ <= 0) return error("comment too long already");
    if ((int)last.size() > kbdMax_) last.resize(kbdMax_);
    kbdOpen_ = true;
    kbdBlock_ = idx;
    kbdText_ = last;
    kbdPos_ = (int)last.size();
    kbdSel_ = 0;
    kbdCaps_ = false;
    menuOpen_ = false;
    kbdTitle_ = "comment above d" + tnum(slot_ + 1);
    info("keyboard: pads type, arrows + Screen 1 type the lit key, jog moves the cursor, Screen 8 OK, Screen 7 Cancel");
}

void Editor::kbdKey(int key) {
    if (!kbdOpen_ || key < 0 || key >= kbd::kKeys) return;
    kbdSel_ = key;
    if (key == kbd::kKeyCaps) {
        kbdCaps_ = !kbdCaps_;
        return;
    }
    if (key == kbd::kKeyBackspace) {
        if (kbdPos_ > 0) kbdText_.erase(--kbdPos_, 1);
        return;
    }
    if ((int)kbdText_.size() >= kbdMax_) return error("comment full (" + tnum(kbdMax_) + " characters)");
    const char c = kbdCaps_ ? kbd::kBig[key] : kbd::kSmall[key];
    kbdText_.insert(kbdPos_++, c);
    messageError_ = false;
}

void Editor::kbdAction(Action a) {
    switch (a) {
        case Action::Left: kbdSel_ = kbd::kMove[0][kbdSel_]; return;
        case Action::Right: kbdSel_ = kbd::kMove[1][kbdSel_]; return;
        case Action::Up: kbdSel_ = kbd::kMove[2][kbdSel_]; return;
        case Action::Down: kbdSel_ = kbd::kMove[3][kbdSel_]; return;
        case Action::WheelUp:
        case Action::WheelUpFast: kbdPos_ = std::min((int)kbdText_.size(), kbdPos_ + 1); return;
        case Action::WheelDown:
        case Action::WheelDownFast: kbdPos_ = std::max(0, kbdPos_ - 1); return;
        case Action::KbdType:
        case Action::Insert: return kbdKey(kbdSel_);
        case Action::Delete: return kbdKey(kbd::kKeyBackspace);
        case Action::KbdClear:
            kbdText_.clear();
            kbdPos_ = 0;
            return;
        case Action::KbdCancel:
        case Action::Cancel:
            kbdOpen_ = false;
            return info("comment unchanged");
        case Action::KbdOk:
        case Action::Confirm: {
            kbdOpen_ = false;
            if (kbdBlock_ < 0 || kbdBlock_ >= prog_->blockCount()) return error("block gone");
            const Text all = prog_->blockComment(kbdBlock_);
            const int nl = all.rfind('\n');
            Text head = nl == Text::npos ? "" : all.substr(0, nl + 1);
            Text text = kbdText_;
            while (!text.empty() && text.back() == ' ') text.pop_back();
            Text out = text.empty() ? head : head + "-- " + text;
            while (!out.empty() && out.back() == '\n') out.pop_back();
            if (out == all) return info("comment unchanged");
            if ((int)out.size() > kMaxCommentLen) return error("comment too long");
            if (uint8_t* d = reserveUndo('M', 4 + all.size() + 1)) {  // undo step: the comment before
                memcpy(d, &kbdBlock_, sizeof kbdBlock_);
                memcpy(d + 4, all.c_str(), (size_t)all.size() + 1);
            }
            memcpy(prog_->blockComment(kbdBlock_), out.c_str(), out.size() + 1);
            changes_++;
            return info(text.empty() ? "comment removed" : "comment: " + text);
        }
        default: return;  // nothing else while the keyboard is up
    }
}

}  // namespace tc
