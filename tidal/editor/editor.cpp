// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
#include "editor_impl.h"

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
    {"blockup", Action::BlockUp}, {"blockdown", Action::BlockDown}, {"linebreak", Action::LineBreak},
    {"qgain", Action::QuickGain}, {"qroom", Action::QuickRoom}, {"qdelay", Action::QuickDelay},
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
    {"kbdclear", Action::KbdClear}, {"cursormode", Action::ToggleCursorMode},
    {"backspace", Action::Backspace}, {"qshape", Action::QuickShape},
    {"repeat", Action::StepRepeat}, {"weight", Action::StepWeight}, {"unmuteall", Action::UnmuteAll},
    {"fastmark", Action::StepFast}, {"slowmark", Action::StepSlow}, {"stack", Action::StepStack},
    {"topattern", Action::ToPattern}, {"shift7", Action::Shift7},
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
const char* ed::controlDefault(const char* name) {
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
        // acid lines (docs/tidal_acid.md): Bool patterns need a string ("1", not 1)
        {"oct", "\"1 1 2 1\""},  {"acc", "\"0 0 1 0\""},  {"slid", "\"0 0 0 1\""}, {"time", "\"1 1 0 1\""},
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
    memset(tokSpans_, 0, sizeof tokSpans_);  // kept zero from here on (cursor.cpp clears what it used)
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

// ---- voices for layering functions (user, after RC15) -------------------------------
// Overlapping notes on one sample track only ring with `# voices N` (else
// each note cuts the last, docs/tidal_voices.md). How many a block's code
// asks for: jux / superimpose / off 2, echoes and ghost 3, stack / layer /
// "[a, b]" one per layer, chords 3; nested ones multiply; at most 3 (the
// Tracker's players per track).

static int layersOf(const Program& p, uint16_t list) {
    return list && p.at(list).kind == Kind::List ? p.childCount(list) : 1;
}

static int voicesNeeded(const Program& p, uint16_t n) {
    if (!n) return 1;
    const Node& x = p.at(n);
    int inner = 1;
    for (uint16_t c = x.first; c; c = p.at(c).next) {
        const int v = voicesNeeded(p, c);
        if (v > inner) inner = v;
    }
    int factor = 1;
    if (x.kind == Kind::App && x.first && p.at(x.first).kind == Kind::Fn) {
        const char* f = lib((int)p.at(x.first).a).name;
        const uint16_t a0 = p.at(x.first).next;  // the first argument
        if (nameIn(f, {"jux", "juxBy", "superimpose", "off", "overlay"})) {
            factor = 2;
        } else if (nameIn(f, {"stut", "echo", "stutter", "echoWith", "stutWith"})) {
            if (a0 && p.at(a0).kind == Kind::Num) {  // the count
                const Rational r = nodeRational(p.at(a0));
                factor = (int)(r.num / (int32_t)(r.den ? r.den : 1));
            } else {
                factor = 3;
            }
        } else if (nameIn(f, {"ghost", "ghost'", "rolled", "rolledBy"})) {
            factor = 3;
        } else if (nameIn(f, {"stack", "layer"})) {
            factor = layersOf(p, a0);
        }
    } else if (x.kind == Kind::MSub || x.kind == Kind::MAlt || x.kind == Kind::MPoly) {
        // [a, b] <a, b> {a b, c}: layers at once ({..}%n: its last child is the %n)
        factor = p.childCount(n) - (x.kind == Kind::MPoly && (x.flags & kHasSteps) ? 1 : 0);
    } else if (x.kind == Kind::MChord) {
        factor = 3;
    }
    const int v = factor * inner;
    return v < 1 ? 1 : v > kMaxVoices ? kMaxVoices : v;
}

// Inside an open edit: the block now needs more voices than before the edit
// and has fewer: raise a literal `voices`, or add `# voices N` to the whole
// pattern (part of the same undo step). A voices the user lowered stays,
// unless an edit adds more layering.
void Editor::raiseVoices() {
    if (cps_ || slot_ >= kSampleSlots) return;
    const uint16_t b = prog_->block[slot_];
    const uint16_t root = b ? prog_->at(b).first : 0;
    if (!root) return;
    const int need = voicesNeeded(*prog_, root);
    if (need <= voicesBefore_ || need <= 1) return;
    if (const uint16_t app = findControlApp(*prog_, b, "voices")) {
        const uint16_t arg = prog_->at(prog_->at(app).first).next;
        if (!arg || prog_->at(arg).kind != Kind::Num) return;  // a pattern ("<1 3>"): as the user wrote it
        const Rational have = nodeRational(prog_->at(arg));
        if (have.den == 1 && have.num >= need) return;
        setNodeRational(prog_->at(arg), makeRational(need, 1), 0);
        voicesAdded_ = need;
        return;
    }
    EditSpec sp = spec(EditKind::WrapOp, root, "voices " + tnum(need));
    sp.op = "#";
    sp.left = true;
    if (applySpec(sp)) voicesAdded_ = need;
}

bool Editor::beginEdit() {
    if (prog_->checkpointOpen()) prog_->rollback(*cp_);  // never nest
    {
        const uint16_t b = prog_->block[slot_];
        voicesBefore_ = b && !cps_ ? voicesNeeded(*prog_, prog_->at(b).first) : 1;
    }
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
    if (check) raiseVoices();
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

EditSpec ed::spec(EditKind k, uint16_t target, const Text& code) {
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
        if (cursorMode_)
            return m.printf("gap %d of %d: pads / Screen 3-6 / Insert add a step; Delete: the step after, "
                            "Shift+Delete: the one before",
                            gapPos_, count),
                   m;
        return m.printf("gap %d of %d: pads / Screen 3-6 insert, Delete removes the step before", gapPos_, count), m;
    }
    if (!cursor_)
        return "empty: Instrument, Note, Screen 1-8 or FX start a block; Up / Down leave";
    if (headGap_) return "start of the pattern: Insert adds a $ function in front, Delete the token after";
    if (preSym_) {
        const Node& sn = prog_->at(cursor_);
        if (sn.kind == Kind::App) return "before $: Insert adds a $ function here, Delete unwraps the $";
        return "before " + Label(lib((int)sn.a).name) + ": Insert adds a # control here, Delete unwraps it";
    }
    if (cps_ && cursor_ == prog_->cpsExpr && prog_->at(cursor_).kind != Kind::Num)
        return "setcps (Right: its numbers, Down: the blocks, jog: the BPM, Play sends it)";
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
    if (onOperator() && n.kind == Kind::App) return "$ (Delete unwraps it, Shift+Screen 7: a line break before it)";
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
    voicesAdded_ = 0;
    actOne(a);
    noteVoices();
    dropEmptyBlocks();
    cursorSettle();
}

void Editor::actOne(Action a) {
    if (kbdOpen_) return kbdAction(a);
    if (selSeq_ && !menuOpen_) {  // a selection lives until another kind of key
        switch (a) {
            case Action::SelLeft: case Action::SelRight: case Action::Struct3: case Action::Struct4:
            case Action::Struct5: case Action::Struct6: case Action::Struct7: case Action::Delete:
            case Action::Copy: case Action::Evaluate: case Action::PlayCancel: case Action::UnmuteAll:
                break;
            default: selA_ = selB_ = selSeq_ = 0; break;
        }
    }
    // Copy / Paste on an operator (its symbol, or the bar before it in cursor
    // mode) act on the operator and its right side; the flags go below
    keyOnOp_ = !menuOpen_ && cursor_ && prog_->at(cursor_).kind == Kind::Op && (onOperator() || preSym_);
    switch (a) {  // the bar before the pattern lasts for the cursor keys and the ones that use it
        case Action::Left: case Action::Right: case Action::Up: case Action::Down: case Action::Insert:
        case Action::Delete: case Action::Backspace: case Action::Evaluate: case Action::PlayCancel: case Action::UnmuteAll:
        case Action::Hush: case Action::SilenceSlot: case Action::Save: case Action::Copy:
            break;
        default:
            if (!menuOpen_) headGap_ = preSym_ = false;
            break;
    }
    if (a == Action::SelLeft || a == Action::SelRight) return selectMove(a == Action::SelLeft ? -1 : 1);
    const bool wasOnOp = onOperator();
    switch (a) {  // these keep the cursor on the operator's symbol; the rest leave it
        case Action::WheelUp: case Action::WheelDown: case Action::WheelUpFast: case Action::WheelDownFast:
        case Action::Replace: case Action::Left: case Action::Right: case Action::Up:
        case Action::Evaluate: case Action::PlayCancel: case Action::UnmuteAll: case Action::Confirm: case Action::Cancel:
        case Action::Delete: case Action::Backspace:  // Delete on a symbol unwraps it (not the whole expression)
        case Action::Copy:  // Copy leaves the cursor where it is
            break;
        case Action::Down:
            if (!cursorMode_) onOp_ = false;  // cursor mode: the line below from the symbol's column
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
            case Action::ToggleTrackerMode: case Action::PlayCancel: case Action::UnmuteAll: case Action::Confirm: case Action::Cancel:
            case Action::NextSlot: case Action::PrevSlot: case Action::Home: case Action::BlockUp:
            case Action::BlockDown:
                break;
            default: return error("on setcps: jog / Replace change it, Play sends it; Shift+Song or a slot pad goes back");
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
            case Action::MenuFunctions: case Action::MenuControls: case Action::QuickGain:
            case Action::QuickRoom: case Action::QuickDelay: case Action::QuickShape:
                return openReplace();
            default: break;
        }
    }
    if (gapSeq_ && !menuOpen_ && listGap()) {
        switch (a) {
            case Action::Left: case Action::Right: case Action::Up: case Action::Down:
            case Action::Delete: case Action::Insert: case Action::Paste: case Action::Replace:
            case Action::PlayCancel: case Action::UnmuteAll: case Action::Evaluate: case Action::Hush: case Action::SilenceSlot:
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
            case Action::PlayCancel: case Action::UnmuteAll: case Action::Evaluate: case Action::Hush: case Action::MenuChords:
            case Action::SilenceSlot: case Action::Backspace: case Action::ToggleCursorMode:
            case Action::StepRepeat: case Action::StepWeight: case Action::StepFast: case Action::StepSlow:
            case Action::StepStack: case Action::Shift7:
                break;
            case Action::Insert:  // cursor mode: a step into the gap
                if (!cursorMode_) leaveGap();
                break;
            default: leaveGap(); break;  // act on the whole sequence
        }
    }
    // cursor mode: a function's name stands for its whole application for
    // the keys that wrap, copy or paste (the tree mode selects that node itself)
    if (cursorMode_ && !menuOpen_ && !gapSeq_ && cursor_) {
        switch (a) {
            case Action::Struct1: case Action::Struct2: case Action::Struct3: case Action::Struct4:
            case Action::Struct5: case Action::Struct6: case Action::Struct7: case Action::Struct8:
            case Action::MenuModifiers: case Action::MenuFx: case Action::Wrap: case Action::MenuFunctions:
            case Action::MenuControls: case Action::Copy: case Action::Paste: case Action::Quick1:
            case Action::Quick2: case Action::Quick3: case Action::Quick4: case Action::LineBreak:
                if (const uint16_t h = headOf(cursor_)) cursor_ = h;
                break;
            default: break;
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
        case Action::Down:
            if (!cursorMode_) navigate(a);
            else if (a == Action::Left || a == Action::Right) cursorMove(a == Action::Left ? -1 : 1);
            else cursorLine(a == Action::Up ? -1 : 1);
            break;
        case Action::ToggleCursorMode:
            setCursorMode(!cursorMode_);
            info(cursorMode_ ? "cursor mode: arrows by token / line (Rec: tree mode)"
                             : "tree mode: arrows through the tree (Rec: cursor mode)");
            break;
        case Action::Backspace: cursorDelete(true); break;
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
        case Action::LineBreak: toggleBreak(); break;
        case Action::QuickGain: quickControl("gain 0.9"); break;
        case Action::QuickRoom: quickControl("room 0.3"); break;
        case Action::QuickDelay: quickControl("delay 0.3"); break;
        case Action::QuickShape: quickControl("shape 0.3"); break;
        case Action::StepRepeat: stepSuffix('!'); break;
        case Action::StepWeight: stepSuffix('@'); break;
        case Action::StepFast: stepSuffix('*'); break;
        case Action::StepSlow: stepSuffix('/'); break;
        case Action::StepStack: stepStack(); break;
        case Action::ToPattern: toPattern(); break;
        case Action::Shift7: shift7(); break;
        case Action::Struct1:
        case Action::Struct2:
        case Action::Struct3:
        case Action::Struct4:
        case Action::Struct5:
        case Action::Struct6:
        case Action::Struct7:
        case Action::Struct8: openStruct((int)a - (int)Action::Struct1); break;
        case Action::MenuModifiers: openWrap(WrapMode::Modifiers); break;
        case Action::UnmuteAll: unmuteAll(); break;
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
        case Action::Insert: cursorMode_ ? cursorInsert() : doInsert(); break;
        case Action::Delete: cursorMode_ ? cursorDelete(false) : doDelete(); break;
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
                info("d" + tnum(slot_ + 1) + " silenced (Play plays it again)");
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

// Insert (tree mode; cursor mode: cursorInsert): a gap: a copy in; a block's
// top: arm a new block above; a step / list element: a copy after it; else
// the # controls menu.
void Editor::doInsert() {
    if (gapSeq_) {
        insertListCopy();
        return;
    }
    if (!cursor_) return;
    if (prog_->at(cursor_).kind == Kind::Block) {
        newBlockArmed_ = true;
        info("slot pad: a new block for that dN above this one");
        return;
    }
    uint16_t par = prog_->parentOf(cursor_);
    Kind pk = par ? prog_->at(par).kind : Kind::None;
    if (pk == Kind::MSeq || pk == Kind::List) {
        // duplicate this step / element after itself
        if (!beginEdit()) return;
        Program* p = prog_;
        uint16_t c = p->copyTree(*p, cursor_);
        if (!c) {
            abortEdit();
            error("out of nodes");
            return;
        }
        p->at(c).next = p->at(cursor_).next;
        p->at(cursor_).next = c;
        if (endEdit(c)) info("inserted a copy");
    } else {
        openWrap(WrapMode::Controls);
    }
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
    info(shift ? "Shift+Pattern: pad = solo, Screen 8 = unmute all, release = hush, Delete = cancel"
               : "Pattern: pad = mute / unmute, Screen 8 = unmute all");
}

void Editor::playUp() {
    if (!playHeld_) return;
    playHeld_ = false;
    if (playUsed_) return;
    if (playShift_) {
        act(Action::Hush);
        return;
    }
    // Pattern alone does nothing (RC17 notes: unmute all is Pattern + Screen 8)
    info("Pattern: hold it and press a pad to mute, Screen 8 to unmute all");
}

void Editor::unmuteAll() {
    playUsed_ = true;  // releasing Pattern then does nothing
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
        flash(b, false);
        return error(e.msg);
    }
    if (holes) return flash(b, false), error("fill in the ?? first");
    if (trackerMode_ && !trackerCheck(*prog_, slot_, e)) return flash(b, false), error(e.msg);
    if (prog_->cpsExpr && out_) {  // a changed setcps goes first
        const Text cps = cpsText(*prog_);
        if (cps != lastCps_) {
            out_->setCps(*prog_);
            lastCps_ = cps;
        }
    }
    Label msg;
    if (out_ && !out_->evaluate(*prog_, slot_, msg)) return flash(b, false), error(msg);
    state_[slot_] = SlotState::Playing;
    playing_[slot_] = b;
    flash(b, true);
    info("d" + tnum(slot_ + 1) + " evaluated");
}

bool Editor::pollHighlights(double now) {
    bool changed = false;
    if (flashPending_) {  // the evaluate flash starts now ...
        flashPending_ = false;
        flashStart_ = now;
        flashShown_ = true;
        changed = true;
    } else if (flashShown_ && !flashOn(now)) {  // ... and is over
        flashShown_ = false;
        changed = true;
    }
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
    info("setcps: jog changes the value (BPM / 60 / beats per cycle), Play sends it, Shift+Song goes back");
}

}  // namespace tc
