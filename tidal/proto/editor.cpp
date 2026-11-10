#include "editor.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>

#include "check.h"
#include "library.h"
#include "mapping.h"
#include "parse.h"
#include "render.h"

using namespace tc;

namespace proto {

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
    {"notes", Action::MenuNotes}, {"samples", Action::MenuSamples},
    {"functions", Action::MenuFunctions}, {"controls", Action::MenuControls},
    {"quick1", Action::Quick1},   {"quick2", Action::Quick2},     {"quick3", Action::Quick3},
    {"quick4", Action::Quick4},   {"launch", Action::MenuLaunch},
    {"struct1", Action::Struct1}, {"struct2", Action::Struct2},   {"struct3", Action::Struct3},
    {"struct4", Action::Struct4}, {"struct5", Action::Struct5},   {"struct6", Action::Struct6},
    {"struct7", Action::Struct7}, {"struct8", Action::Struct8},
    {"modifiers", Action::MenuModifiers}, {"fx", Action::MenuFx},
    {"jumpn", Action::JumpNotes}, {"jumps", Action::JumpSamples},
    {"playcancel", Action::PlayCancel},
};

const char* actionName(Action a) {
    for (auto& x : kActions)
        if (x.a == a) return x.name;
    return "?";
}
bool actionFromName(const std::string& s, Action& out) {
    for (auto& x : kActions)
        if (s == x.name) {
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

Editor::Editor() : prog_(new Program) {
    for (auto& s : state_) s = SlotState::Empty;
    // 48 instrument slots, one per pad (the Tracker has 48 sample
    // instruments). Default: Dirt-Samples folders, so SuperDirt plays them;
    // --instruments FILE replaces it.
    samples_ = {"bd",    "sd",    "hh",    "cp",    "ho",     "cr",     "lt",    "mt",    "ht",    "drum",
                "sn",    "tink",  "808bd", "808sd", "808hc",  "808oh",  "808cy", "808lt", "808mt", "808ht",
                "808lc", "808mc", "808",   "hh27",  "arpy",   "bass",   "bass1", "bass2", "bass3", "jvbass",
                "pluck", "casio", "blip",  "bend",  "bev",    "bottle", "break", "jazz",  "feel",  "future",
                "gab",   "hand",  "east",  "kurt",  "rave",   "space",  "tabla", "wind"};
    info("ready");
}

// ---------------------------------------------------------------------------
// files

bool Editor::loadText(const std::string& text, std::string& err) {
    auto p = std::unique_ptr<Program>(new Program);
    Error e;
    if (!parseFile(*p, text.c_str(), (int)text.size(), e)) {
        int line, col;
        errorLineCol(text.c_str(), e.pos, line, col);
        err = std::to_string(line) + ":" + std::to_string(col) + ": " + e.msg;
        return false;
    }
    Error ce;
    if (!checkProgram(*p, ce)) {
        err = std::string("type error: ") + ce.msg;
        return false;
    }
    prog_ = std::move(p);
    undo_.clear();
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

bool Editor::load(const std::string& file, std::string& err) {
    std::ifstream in(file);
    if (!in) {
        err = "can't open " + file;
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    path = file;
    return loadText(ss.str(), err);
}

std::string Editor::fileText() const {
    static char buf[65536];
    Renderer r(buf, sizeof buf);
    buf[0] = 0;
    r.file(*prog_);
    return buf;
}

bool Editor::save(const std::string& file, std::string& err) {
    std::ofstream out(file);
    if (!out) {
        err = "can't write " + file;
        return false;
    }
    out << fileText();
    return true;
}

// ---------------------------------------------------------------------------
// helpers

std::string Editor::renderNode(const Program& p, uint16_t n) const {
    char buf[2048];
    Renderer r(buf, sizeof buf);
    buf[0] = 0;
    // Render just this subtree: temporarily it may have siblings, which the
    // renderer ignores for a single node.
    r.expr(p, n);
    return buf;
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

void Editor::enterSlot(int s) {
    slot_ = s;
    uint16_t b = prog_->block[s];
    cursor_ = b ? (prog_->at(b).first ? prog_->at(b).first : b) : 0;
    menuOpen_ = false;
}

bool Editor::recheck(Program& p, std::string& err) const {
    Error e;
    if (!checkBlock(p, slot_, e)) {
        err = e.msg;
        return false;
    }
    // Tracker mode: refuse edits (and so menu items) that bring in something
    // the device can't play; a block that already has such a thing (loaded
    // from a file) stays editable, evaluate() reports it.
    if (trackerMode_) {
        Error now, before;
        if (!trackerCheck(p, slot_, now) && trackerCheck(*prog_, slot_, before)) {
            err = now.msg;
            return false;
        }
    }
    return true;
}

void Editor::snapshot() {
    undo_.emplace_back(new Program(*prog_));
    if (undo_.size() > 64) undo_.erase(undo_.begin());
}

void Editor::commit(std::unique_ptr<Program> p, uint16_t cursor) {
    gapSeq_ = 0;
    std::string err;
    if (!recheck(*p, err)) {
        error(err);
        return;
    }
    snapshot();
    prog_ = std::move(p);
    cursor_ = cursor;
    messageError_ = false;  // callers test it after commit; an earlier error must not stick
    if (state_[slot_] != SlotState::Empty || prog_->block[slot_]) state_[slot_] = SlotState::Edited;
}

std::shared_ptr<Program> Editor::tryReplace(const std::string& code, bool mini, uint16_t& newNode) const {
    auto p = std::make_shared<Program>(*prog_);
    Error e;
    uint16_t n = mini ? parseMini(*p, code.c_str(), (int)code.size(), e)
                      : parseExpr(*p, code.c_str(), (int)code.size(), e);
    if (!n) return nullptr;
    if (mini && p->at(cursor_).kind != Kind::MSeq) {
        // replacing a single step: unwrap the parsed one-step sequence
        Node& sq = p->at(n);
        if (!sq.first || p->at(sq.first).next || (sq.flags & kDots)) return nullptr;
        uint16_t step = sq.first;
        sq.first = 0;
        p->freeTree(n);
        n = step;
    }
    Node& cur = p->at(cursor_);
    if (cur.kind == Kind::Block) {
        if (cur.first) {
            uint16_t old = cur.first;
            p->replace(old, n);
            p->freeTree(old);
        } else {
            p->appendChild(cursor_, n);
        }
    } else {
        p->replace(cursor_, n);
        p->freeTree(cursor_);
    }
    std::string err;
    if (!recheck(*p, err)) return nullptr;
    newNode = n;
    return p;
}

std::shared_ptr<Program> Editor::tryWrapApp(const std::string& head, uint16_t& newNode) const {
    auto p = std::make_shared<Program>(*prog_);
    Error e;
    uint16_t h = parseExpr(*p, head.c_str(), (int)head.size(), e);
    if (!h) return nullptr;
    uint16_t app;
    if (p->at(h).kind == Kind::App && !(p->at(h).flags & kDollar)) {
        app = h;
    } else {
        app = p->alloc(Kind::App);
        if (!app) return nullptr;
        p->appendChild(app, h);
    }
    if (exprPrec(*p, cursor_) < 11) p->at(app).flags |= kDollar;
    p->replace(cursor_, app);
    p->appendChild(app, cursor_);
    std::string err;
    if (!recheck(*p, err)) return nullptr;
    newNode = app;
    return p;
}

std::shared_ptr<Program> Editor::tryWrapOp(const char* op, const std::string& rhs, bool xOnLeft,
                                           uint16_t& newNode) const {
    int id = libFind(op, (int)strlen(op));
    if (id < 0) return nullptr;
    auto p = std::make_shared<Program>(*prog_);
    Error e;
    uint16_t r = parseExpr(*p, rhs.c_str(), (int)rhs.size(), e);
    if (!r) return nullptr;
    uint16_t o = p->alloc(Kind::Op);
    if (!o) return nullptr;
    p->at(o).a = (uint32_t)id;
    p->replace(cursor_, o);
    if (xOnLeft) {
        p->appendChild(o, cursor_);
        p->appendChild(o, r);
    } else {
        p->appendChild(o, r);
        p->appendChild(o, cursor_);
    }
    std::string err;
    if (!recheck(*p, err)) return nullptr;
    newNode = o;
    return p;
}

void Editor::openMenu(const std::string& title, std::vector<MenuItem> items) {
    if (items.empty()) {
        error("nothing fits here");
        return;
    }
    menuTitle_ = title;
    menu_ = std::move(items);
    menuSel_ = 0;
    menuOpen_ = true;
    info(title + ": wheel to choose, ok to apply, cancel to close");
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
    if (!padNumeric()) return error("pads enter numbers inside a numeric pattern");
    std::string n = std::to_string(v);
    if (rest) enterStep("~", "", "~");
    else enterStep(n, v < 0 ? "(" + n + ")" : n, n);
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
    std::string w = prog_->str((int)prog_->at(cursor_).a);
    for (size_t i = 0; i < samples_.size(); i++)
        if (samples_[i] == w) return (int)i;
    return -1;
}

void Editor::enterInstrument(int i) {
    if (!padInstruments()) return error("pads pick instruments inside an s pattern");
    if (i < 0 || i >= (int)samples_.size() || samples_[i].empty())
        return error("instrument slot " + std::to_string(i + 1) + " is empty");
    enterStep(samples_[i], "", samples_[i]);
}

// Pad entry: replace the value at the cursor (mini code inside "...",
// exprCode for a number argument; empty = not allowed there), then select
// the next step's value or append a copy to type over (Delete removes it).
void Editor::enterStep(const std::string& miniCode, const std::string& exprCode, const std::string& label) {
    if (gapSeq_) return insertAtGap(miniCode, label);
    const uint16_t oldCursor = cursor_;
    if (prog_->at(cursor_).kind == Kind::Mini) cursor_ = prog_->at(cursor_).first;  // whole "..."
    bool mini = isMiniKind(prog_->at(cursor_).kind);
    std::string code = mini ? miniCode : exprCode;
    if (code.empty()) {
        cursor_ = oldCursor;
        return error(label + " only goes inside \"...\"");
    }
    uint16_t nn;
    auto r = tryReplace(code, mini, nn);
    if (!r) {
        cursor_ = oldCursor;
        return error(label + " doesn't fit here");
    }
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
            uint16_t next = r->at(step).next;
            if (next) {
                cur = valueOf(*r, next);
            } else {
                // end of the sequence: append a copy to type over (of the
                // value just entered, not of a whole range)
                uint16_t src = r->at(step).kind == Kind::MRange ? cur : step;
                uint16_t c2 = r->copyTree(*r, src);
                if (c2) {
                    r->at(c2).next = 0;
                    r->at(step).next = c2;
                    cur = appended = valueOf(*r, c2);
                }
            }
        }
    }
    commit(std::unique_ptr<Program>(new Program(*r)), cur);
    if (messageError_) {
        cursor_ = oldCursor;
        return;
    }
    pending_ = appended;
    info("= " + label + (appended ? "  (next value; Del ends entry)" : ""));
}

void Editor::gotoSlot(int s) {
    if (s < 0 || s >= kSlots) return;
    pending_ = 0;
    enterSlot(s);
    info(cursorInfo());
}

void Editor::selectMenu(int i) {
    if (menuOpen_ && i >= 0 && i < (int)menu_.size()) menuSel_ = i;
}

std::string Editor::cursorInfo() const {
    if (gapSeq_) {
        const int count = prog_->childCount(gapSeq_);
        return "gap " + std::to_string(gapPos_) + " of " + std::to_string(count) +
               ": pads / Screen 3-7 insert, Delete removes the step before";
    }
    if (!cursor_) return prog_->block[slot_] ? "" : "empty slot: replace to start a pattern";
    const Node& n = prog_->at(cursor_);
    if (n.kind == Kind::Block)  // the block's top: Up / Down = other blocks, Right = into it
        return "d" + std::to_string(slot_ + 1) + " block (Up / Down: blocks, Right: into it)";
    std::string s = kindName(n.kind);
    if (n.kind == Kind::Fn || n.kind == Kind::Op || n.kind == Kind::SectionL || n.kind == Kind::SectionR)
        s += std::string(" ") + lib((int)n.a).name;
    if (n.type) s += std::string(" : ") + tyName(n.type);
    return s;
}

// ---------------------------------------------------------------------------
// actions

void Editor::act(Action a) {
    if (gapSeq_ && !menuOpen_) {
        switch (a) {
            case Action::Left: case Action::Right: case Action::Up: case Action::Down:
            case Action::Delete: case Action::Struct1: case Action::Struct2: case Action::Struct3:
            case Action::Struct4: case Action::Struct5: case Action::Struct6: case Action::Struct7:
            case Action::Struct8: case Action::MenuModifiers: case Action::MenuFx:
            case Action::PlayCancel: case Action::Evaluate: case Action::Hush:
                break;
            default: leaveGap(); break;  // act on the whole sequence
        }
    }
    uint16_t pending = pending_;
    pending_ = 0;  // only the very next Delete may remove an appended step
    if (a == Action::Delete && pending && pending == cursor_) pending_ = pending;
    if (menuOpen_) {
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
            case Action::Evaluate: {
                MenuItem& it = menu_[menuSel_];
                if (it.label.rfind("# ", 0) == 0) lastControl_ = it.label.substr(2);
                snapshot();
                prog_.reset(new Program(*it.result));
                cursor_ = it.cursor;
                gapSeq_ = 0;
                state_[slot_] = SlotState::Edited;
                menuOpen_ = false;
                info(menuTitle_ + ": " + it.label);
                return;
            }
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
        case Action::Home:  // the block's top (dN $ / jumpMod N k $)
            if (prog_->block[slot_]) cursor_ = prog_->block[slot_];
            info(cursorInfo());
            break;
        case Action::MenuNotes: openNotes(); break;
        case Action::MenuSamples: openSamples(); break;
        case Action::MenuFunctions: openWrap(WrapMode::Functions); break;
        case Action::MenuControls: openWrap(WrapMode::Controls); break;
        case Action::MenuLaunch: openLaunch(); break;
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
            if (!cursor_) break;
            uint16_t par = prog_->parentOf(cursor_);
            Kind pk = par ? prog_->at(par).kind : Kind::None;
            if (pk == Kind::MSeq || pk == Kind::List) {
                // duplicate this step / element after itself
                auto p = std::unique_ptr<Program>(new Program(*prog_));
                uint16_t c = p->copyTree(*p, cursor_);
                if (!c) {
                    error("out of nodes");
                    break;
                }
                p->at(c).next = p->at(cursor_).next;
                p->at(cursor_).next = c;
                commit(std::move(p), c);
                if (!messageError_) info("inserted a copy");
            } else {
                openWrap(WrapMode::Controls);
            }
            break;
        }
        case Action::Delete: doDelete(); break;
        case Action::Unwrap: doUnwrap(); break;
        case Action::Undo:
            if (undo_.empty()) {
                error("nothing to undo");
            } else {
                prog_ = std::move(undo_.back());
                undo_.pop_back();
                enterSlot(slot_);
                state_[slot_] = prog_->block[slot_] ? SlotState::Edited : SlotState::Empty;
                info("undone");
            }
            break;
        case Action::Evaluate: evaluate(); break;
        case Action::Hush:
            if (backend_) backend_->send("hush");
            for (int s = 0; s < kSlots; s++)
                if (state_[s] == SlotState::Playing) state_[s] = SlotState::Edited;
            info("hush");
            break;
        case Action::NextSlot: enterSlot((slot_ + 1) % kSlots); info(cursorInfo()); break;
        case Action::PrevSlot: enterSlot((slot_ + kSlots - 1) % kSlots); info(cursorInfo()); break;
        case Action::Save: {
            std::string err;
            if (path.empty()) error("no file path (use --save)");
            else if (!save(path, err)) error(err);
            else if (fileText().find("??") != std::string::npos)
                error("saved " + path + ", but it has ?? holes: Tidal can't run those blocks");
            else info("saved " + path);
            break;
        }
        case Action::ToggleTrackerMode:
            trackerMode_ = !trackerMode_;
            info(trackerMode_ ? "tracker mode on" : "tracker mode off");
            break;
        default: break;
    }
}

void Editor::navigate(Action a) {
    const Program& p = *prog_;
    if (gapSeq_) {
        // on a gap: Right = the step after it, Left = the step before, Up = out
        const int count = p.childCount(gapSeq_);
        if (a == Action::Right && gapPos_ < count) {
            cursor_ = p.child(gapSeq_, gapPos_);
            gapSeq_ = 0;
        } else if (a == Action::Left && gapPos_ > 0) {
            cursor_ = p.child(gapSeq_, gapPos_ - 1);
            gapSeq_ = 0;
        } else if (a == Action::Up) {
            cursor_ = gapSeq_;
            gapSeq_ = 0;
            return navigate(Action::Up);
        }
        info(cursorInfo());
        return;
    }
    if (!cursor_) return;
    // a step of a "..." sequence: Left / Right go to the gap before / after it
    if (a == Action::Left || a == Action::Right) {
        const uint16_t par = p.parentOf(cursor_);
        if (par && p.at(par).kind == Kind::MSeq) {
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
            const int dir = a == Action::Up ? -1 : 1;
            for (int s = slot_ + dir; s >= 0 && s < kSlots; s += dir)
                if (p.block[s]) {
                    slot_ = s;
                    pending_ = 0;
                    cursor_ = p.block[s];
                    break;
                }
        } else if (a == Action::Right && p.at(cursor_).first) {
            cursor_ = p.at(cursor_).first;
        }
        info(cursorInfo());
        return;
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

void Editor::wheel(int dir, bool fast) {
    if (!cursor_) return;
    Node& n = prog_->at(cursor_);
    auto p = std::unique_ptr<Program>(new Program(*prog_));
    Node& m = p->at(cursor_);
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
            } else if (!samples_.empty()) {
                std::string cur = p->str((int)m.a);
                int i = 0;
                for (; i < (int)samples_.size(); i++)
                    if (samples_[i] == cur) break;
                int ns = (int)samples_.size(), k = i < ns ? i : 0;
                for (int t = 0; t < ns; t++) {  // next non-empty slot
                    k = ((k + dir) % ns + ns) % ns;
                    if (!samples_[k].empty()) break;
                }
                if (samples_[k].empty()) return error("no instruments");
                int id = p->intern(samples_[k].c_str(), (int)samples_[k].size());
                if (id < 0) return error("too many words");
                m.a = (uint32_t)id;
            }
            break;
        }
        case Kind::Fn: wheelFn(dir); return;
        default: error("the wheel changes numbers, notes, samples and functions"); return;
    }
    std::string err;
    if (!recheck(*p, err)) return error(err);
    snapshot();
    prog_ = std::move(p);
    state_[slot_] = SlotState::Edited;
    info(renderNode(*prog_, cursor_));
}

// Wheel on a function name: step through the functions that type-check here.
void Editor::wheelFn(int dir) {
    std::vector<int> ok;
    int cur = (int)prog_->at(cursor_).a;
    for (int id = 0; id < libCount(); id++) {
        if (libIsOp(id)) continue;
        if (id == cur) {
            ok.push_back(id);
            continue;
        }
        Program* p = new Program(*prog_);
        p->at(cursor_).a = (uint32_t)id;
        std::string err;
        if (recheck(*p, err)) ok.push_back(id);
        delete p;
    }
    int at = 0;
    for (int i = 0; i < (int)ok.size(); i++)
        if (ok[i] == cur) at = i;
    int next = ok[(at + dir + (int)ok.size()) % (int)ok.size()];
    if (next == cur) return info("no other function fits here");
    auto p = std::unique_ptr<Program>(new Program(*prog_));
    p->at(cursor_).a = (uint32_t)next;
    commit(std::move(p), cursor_);
    if (!messageError_) info(lib(next).name);
}

// Empty slot: create its (empty) block and put the cursor on it.
void Editor::ensureBlock() {
    if (cursor_) return;
    auto p = std::unique_ptr<Program>(new Program(*prog_));
    uint16_t b = p->alloc(Kind::Block);
    p->at(b).a = (uint32_t)slot_;
    p->block[slot_] = b;
    prog_ = std::move(p);
    cursor_ = b;
}

void Editor::addReplaceItems(std::vector<MenuItem>& items, const std::string& label, const std::string& code,
                             bool mini) const {
    uint16_t nn;
    auto r = tryReplace(code, mini, nn);
    if (r) items.push_back(MenuItem{label, r, nn});
}

// Note button: pitches. Inside "..." single steps, otherwise note patterns.
void Editor::openNotes() {
    ensureBlock();
    std::vector<MenuItem> items;
    auto add = [&](const std::string& l, const std::string& code, bool mini) { addReplaceItems(items, l, code, mini); };
    if (isMiniKind(prog_->at(cursor_).kind)) {
        for (const char* v : {"c", "d", "e", "f", "g", "a", "b", "cs", "ef", "fs", "af", "bf", "c4", "c6",
                              "c'maj", "c'min", "c'maj7", "c'min7", "c'dom7", "c'sus4", "e'min", "g'maj"})
            add(v, v, true);
        for (int i = 0; i <= 12; i++) add(std::to_string(i), std::to_string(i), true);
        add("rest ~", "~", true);
    } else {
        for (const char* v : {"note \"c e g\"", "n \"0 .. 7\"", "n \"c'maj e'min\"",
                              "n (scale \"major\" \"0 .. 7\")", "n (arp \"up\" \"<c'maj e'min>\")",
                              "\"c e g\"", "\"0 .. 7\"", "\"<c'maj e'min>\"", "(scale \"major\" \"0 .. 7\")",
                              "(arp \"up\" \"<c'maj e'min>\")"})
            add(v, v, false);
    }
    openMenu("notes", std::move(items));
}

// Instrument button: samples. Inside "..." names (and name:n), otherwise s "name".
void Editor::openSamples() {
    ensureBlock();
    std::vector<MenuItem> items;
    bool mini = isMiniKind(prog_->at(cursor_).kind);
    for (auto& s : samples_) {
        if (s.empty()) continue;
        if (mini) {
            addReplaceItems(items, s, s, true);
            for (int i = 1; i <= 3; i++) addReplaceItems(items, s + ":" + std::to_string(i), s + ":" + std::to_string(i), true);
        } else {
            addReplaceItems(items, "s \"" + s + "\"", "s \"" + s + "\"", false);
        }
    }
    openMenu("samples", std::move(items));
}

void Editor::doCopy() {
    if (!cursor_) return error("nothing to copy");
    uint16_t n = prog_->at(cursor_).kind == Kind::Block ? exprRoot() : cursor_;
    if (!n) return error("nothing to copy");
    clip_ = renderNode(*prog_, n);
    clipMini_ = isMiniKind(prog_->at(n).kind);
    info("copied " + clip_);
}

// Replace the selection with the clipboard; a copied step can also go where
// a pattern is expected (as "step"). Type-checked like every other edit.
void Editor::doPaste() {
    if (clip_.empty()) return error("clipboard is empty (Copy first)");
    std::unique_ptr<Program> before(new Program(*prog_));
    uint16_t beforeCursor = cursor_;
    ensureBlock();
    bool targetMini = isMiniKind(prog_->at(cursor_).kind);
    uint16_t nn;
    std::shared_ptr<Program> r;
    if (targetMini == clipMini_) r = tryReplace(clip_, clipMini_, nn);
    else if (clipMini_) r = tryReplace("\"" + clip_ + "\"", false, nn);
    if (!r) {
        prog_ = std::move(before);  // ensureBlock may have added an empty block
        cursor_ = beforeCursor;
        return error("can't paste " + clip_ + " here");
    }
    commit(std::unique_ptr<Program>(new Program(*r)), nn);
    if (!messageError_) info("pasted " + clip_);
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

std::string Editor::quickLabel(int i) const {
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
    std::string label = quickLabel(i);
    if (label.empty()) return error("select a pattern first");
    Kind k = prog_->at(cursor_).kind;
    uint16_t nn = 0, target = 0;
    std::shared_ptr<Program> r;
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
        std::string t = renderNode(*prog_, cursor_);
        bool multi = k == Kind::MSeq && prog_->at(cursor_).first && prog_->at(prog_->at(cursor_).first).next;
        std::string step = multi ? "[" + t + "]" : t;  // modifiers bind to one step
        static const char* pre[4] = {"[", "<", "{", ""};
        static const char* post[4] = {"]", ">", "}%4", "(3,8)"};
        std::string code = std::string(pre[i]) + (i == 3 ? step : t) + post[i];
        r = tryReplace(code, true, nn);
        if (r) {
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
        r = tryWrapOp(o.op, o.rhs, o.left, nn);
        if (!r && o.bare) r = tryWrapOp(o.op, o.bare, o.left, nn);  // plain value patterns
        if (r) target = firstNumber(*r, o.left ? childAt(*r, nn, 1) : childAt(*r, nn, 0));
    }
    if (!r) {
        cursor_ = oldCursor;
        return error(label + " doesn't fit here");
    }
    commit(std::unique_ptr<Program>(new Program(*r)), target ? target : nn);
    if (!messageError_) info(label + (target ? ": jog changes the number" : ""));
}

// ---------------------------------------------------------------------------
// mute / solo (Play as a modifier)

void Editor::setMuted(int s, bool on) {
    if (muted_[s] == on) return;
    muted_[s] = on;
    if (backend_) backend_->send(std::string(on ? "mute " : "unmute ") + std::to_string(s + 1));
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
    if (backend_) backend_->send("unmuteAll");
    info(any ? "unmuted all" : "nothing was muted");
}

void Editor::mutePad(int slot) {
    if (!playHeld_ || slot < 0 || slot >= kSlots) return;
    playUsed_ = true;
    const std::string d = "d" + std::to_string(slot + 1);
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
        if (backend_) backend_->send("unmuteAll");
        info(d + " unsoloed: all unmuted");
        return;
    }
    for (int s = 0; s < kSlots; s++) setMuted(s, s != slot);
    info(d + " solo");
}

// The quoted pattern of the first `name "..."` (or `alt "..."`) in the tree
// under `n`, preorder; 0 if none.
static uint16_t findControlMini(const Program& p, uint16_t n, const char* name, const char* alt) {
    if (!n) return 0;
    const Node& x = p.at(n);
    if (x.kind == Kind::App && x.first && p.at(x.first).kind == Kind::Fn) {
        const char* fn = lib((int)p.at(x.first).a).name;
        uint16_t arg = p.at(x.first).next;
        if ((!strcmp(fn, name) || !strcmp(fn, alt)) && arg && p.at(arg).kind == Kind::Mini) return arg;
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
    uint16_t b = prog_->block[slot_];
    if (!b || !exprRoot()) return error("empty slot: replace to start a pattern");
    uint16_t mini = notes ? findControlMini(*prog_, b, "n", "note") : findControlMini(*prog_, b, "s", "sound");
    if (!mini) {
        const uint16_t old = cursor_;
        cursor_ = exprRoot();
        uint16_t nn;
        auto r = tryWrapOp("#", notes ? "n \"0\"" : "s \"bd\"", true, nn);
        if (!r) {
            cursor_ = old;
            return error(std::string("can't add # ") + what + " to this block");
        }
        commit(std::unique_ptr<Program>(new Program(*r)), nn);
        if (messageError_) return;
        mini = notes ? findControlMini(*prog_, prog_->block[slot_], "n", "note")
                     : findControlMini(*prog_, prog_->block[slot_], "s", "sound");
        if (!mini) return;
    }
    uint16_t step = prog_->at(mini).first;
    while (step && prog_->at(step).kind == Kind::MSeq && prog_->at(step).first) step = prog_->at(step).first;
    cursor_ = step ? valueOf(*prog_, step) : mini;
    info(std::string(what) + " pattern: " + (notes ? "pads enter notes" : "pads pick instruments"));
}

// ---------------------------------------------------------------------------
// gap cursor (v2 step 5)

void Editor::leaveGap() {
    if (!gapSeq_) return;
    cursor_ = gapSeq_;  // the whole sequence
    gapSeq_ = 0;
}

std::shared_ptr<Program> Editor::tryInsertAtGap(const std::string& code, uint16_t& newStep) const {
    auto p = std::make_shared<Program>(*prog_);
    Error e;
    uint16_t sq = parseMini(*p, code.c_str(), (int)code.size(), e);
    if (!sq) return nullptr;
    Node& seq = p->at(sq);
    if (!seq.first || p->at(seq.first).next || (seq.flags & kDots)) return nullptr;  // one step only
    const uint16_t step = seq.first;
    seq.first = 0;
    p->freeTree(sq);
    if (gapPos_ == 0) {
        p->at(step).next = p->at(gapSeq_).first;
        p->at(gapSeq_).first = step;
    } else {
        const uint16_t prev = p->child(gapSeq_, gapPos_ - 1);
        if (!prev) return nullptr;
        p->at(step).next = p->at(prev).next;
        p->at(prev).next = step;
    }
    std::string err;
    if (!recheck(*p, err)) return nullptr;
    newStep = step;
    return p;
}

// Pads at a gap: insert the step and stay in the gap after it, so several
// pads insert several steps.
void Editor::insertAtGap(const std::string& code, const std::string& label) {
    if (code.empty()) return error(label + " only goes inside \"...\"");
    const uint16_t seq = gapSeq_;
    const int pos = gapPos_;
    uint16_t nn;
    auto r = tryInsertAtGap(code, nn);
    if (!r) return error(label + " doesn't fit here");
    commit(std::unique_ptr<Program>(new Program(*r)), seq);
    if (messageError_) return;
    gapSeq_ = seq;
    gapPos_ = pos + 1;
    cursor_ = seq;
    info("+ " + label + "  (gap: more pads insert more)");
}

// Delete at a gap: remove the step before it (Backspace).
void Editor::deleteAtGap() {
    if (gapPos_ == 0) return error("start of the sequence: nothing before the gap");
    const uint16_t seq = gapSeq_;
    const int pos = gapPos_;
    if (prog_->childCount(seq) < 2) return error("the last step: Delete on it makes a rest");
    auto p = std::unique_ptr<Program>(new Program(*prog_));
    const uint16_t target = p->child(seq, pos - 1);
    if (pos - 1 == 0) p->at(seq).first = p->at(target).next;
    else p->at(p->child(seq, pos - 2)).next = p->at(target).next;
    p->at(target).next = 0;
    p->freeTree(target);
    commit(std::move(p), seq);
    if (messageError_) return;
    gapSeq_ = seq;
    gapPos_ = pos - 1;
    cursor_ = seq;
    info("removed the step before the gap");
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

// [] <> {} () | around the step(s) at the cursor (inside "..."); the first
// item of each is what the old one-touch buttons did.
void Editor::openMini(int k) {
    if (gapSeq_) {
        // at a gap: insert, built from the neighbouring step
        const Program& p = *prog_;
        const uint16_t nb = gapPos_ > 0 ? p.child(gapSeq_, gapPos_ - 1) : p.child(gapSeq_, 0);
        const std::string t = nb ? renderNode(p, nb) : "~";
        std::vector<std::string> codes;
        switch (k) {
            case 0: codes = {"[" + t + " " + t + "]", "[" + t + " " + t + " " + t + "]", "[" + t + ", " + t + "]"}; break;
            case 1: codes = {"<" + t + " ~>", "<" + t + " " + t + ">"}; break;
            case 2: codes = {"{" + t + " " + t + " " + t + "}%4", "{" + t + ", " + t + " " + t + " " + t + "}"}; break;
            case 3: codes = {t + "(3,8)", t + "(5,8)", t + "(3,8,2)"}; break;
            default: codes = {"[" + t + " | ~]", "[" + t + " | " + t + "]"}; break;
        }
        std::vector<MenuItem> items;
        for (const std::string& code : codes) {
            uint16_t nn;
            auto r = tryInsertAtGap(code, nn);
            if (r) items.push_back(MenuItem{"insert " + code, r, nn});
        }
        static const char* const kTitle[5] = {"[ ]", "< >", "{ }", "( , )", "|"};
        return openMenu(std::string("insert ") + kTitle[k < 5 ? k : 4], std::move(items));
    }
    if (!cursor_) return error("select a step inside \"...\" first");
    Kind kind = prog_->at(cursor_).kind;
    const uint16_t oldCursor = cursor_;
    if (kind == Kind::Mini) {  // on the quotes: the whole sequence inside them
        cursor_ = prog_->at(cursor_).first;
        kind = cursor_ ? prog_->at(cursor_).kind : Kind::None;
    }
    if (!isMiniKind(kind)) {
        cursor_ = oldCursor;
        return error("inside \"...\" only");
    }
    const std::string t = renderNode(*prog_, cursor_);
    const bool multi = kind == Kind::MSeq && prog_->at(cursor_).first && prog_->at(prog_->at(cursor_).first).next;
    const std::string step = multi ? "[" + t + "]" : t;  // modifiers bind to one step
    std::vector<std::string> codes;
    static const char* const kTitle[5] = {"[ ]", "< >", "{ }", "( , )", "|"};
    switch (k) {
        case 0: codes = {"[" + t + "]", "[" + t + " " + t + "]", "[" + t + " " + t + " " + t + "]", "[" + t + ", " + t + "]"}; break;
        case 1: codes = {"<" + t + ">", "<" + t + " ~>", "<" + t + " " + t + ">"}; break;
        case 2: codes = {"{" + t + "}%4", "{" + t + " " + t + " " + t + "}%4", "{" + t + ", " + t + " " + t + " " + t + "}"}; break;
        case 3: codes = {step + "(3,8)", step + "(5,8)", step + "(3,8,2)", step + "(<3 5>,8)"}; break;
        default: codes = {"[" + t + " | ~]", "[" + t + " | " + t + "]", "[" + t + " | " + t + " | ~]"}; break;
    }
    std::vector<MenuItem> items;
    for (const std::string& code : codes) {
        uint16_t nn;
        auto r = tryReplace(code, true, nn);
        if (r) items.push_back(MenuItem{code, r, nn});
    }
    if (items.empty()) cursor_ = oldCursor;
    openMenu(kTitle[k < 5 ? k : 4], std::move(items));
}

void Editor::openReplace() {
    std::vector<MenuItem> items;
    auto add = [&](const std::string& label, const std::string& code, bool mini) {
        addReplaceItems(items, label, code, mini);
    };
    ensureBlock();
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
        std::string t = renderNode(*prog_, cursor_);
        add("rest ~", "~", true);
        for (auto& s : samples_)
            if (!s.empty()) add(s, s, true);
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
            std::string code = e.name;
            if (e.cat == Cat::Control) code += std::string(" ") + controlDefault(e.name);
            else if (e.defaults) code += std::string(" ") + e.defaults;
            add(code, code, false);
        }
    }
    openMenu("replace", std::move(items));
}

void Editor::openWrap(WrapMode mode) {
    if (!cursor_ || prog_->at(cursor_).kind == Kind::Block) return error("select a pattern first");
    bool controlsOnly = mode == WrapMode::Controls || mode == WrapMode::Fx;
    const bool patternLevel = controlsOnly || mode == WrapMode::Dollar || mode == WrapMode::Modifiers ||
                              mode == WrapMode::Ops;
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
    std::vector<MenuItem> items;
    const Node& n = prog_->at(cursor_);
    if (isMiniKind(n.kind)) {
        std::string t = renderNode(*prog_, cursor_);
        auto add = [&](const std::string& label, const std::string& code) {
            uint16_t nn;
            auto r = tryReplace(code, true, nn);
            if (r) items.push_back(MenuItem{label, r, nn});
        };
        for (const char* suf : {"*2", "/2", "!2", "@2", "?", "(3,8)", "*<2 3>", " _"})
            add(t + suf, t + suf);
        add("[" + t + " " + t + "]", "[" + t + " " + t + "]");
        add("<" + t + " ~>", "<" + t + " ~>");
        add("[" + t + ", " + t + "]", "[" + t + ", " + t + "]");
        return openMenu("wrap", std::move(items));
    }
    auto addOp = [&](const char* op, const std::string& rhs, bool left, const std::string& label) {
        uint16_t nn;
        auto r = tryWrapOp(op, rhs, left, nn);
        if (r) items.push_back(MenuItem{label, r, nn});
    };
    auto addControls = [&]() {
        for (int id = 0; id < libCount(); id++) {
            const LibEntry& e = lib(id);
            if (e.cat != Cat::Control) continue;
            std::string rhs = std::string(e.name) + " " + controlDefault(e.name);
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
    if (mode == WrapMode::Fx) {
        for (int id = 0; id < libCount(); id++) {
            const LibEntry& e = lib(id);
            if (e.cat != Cat::Control || !isFx(e.name)) continue;
            std::string rhs = std::string(e.name) + " " + controlDefault(e.name);
            addOp("#", rhs, true, "# " + rhs);
        }
        return openMenu("FX", std::move(items));
    }
    if (mode == WrapMode::Ops) {  // |+ |- |* |/ |< |> with a control
        for (const char* op : {"|+", "|-", "|*", "|/", "|<", "|>"})
            for (const char* rhs : {"n 12", "note 7", "speed 2", "gain 0.8", "pan 0.5", "cutoff 1000"})
                addOp(op, rhs, true, std::string(op) + " " + rhs);
        return openMenu("operators", std::move(items));
    }
    if (mode == WrapMode::Dollar || mode == WrapMode::Modifiers) {
        // functions applied with $ (Screen 1: all of them; FX1: pattern
        // modifiers = time, structure, randomness, layering, sample cutting)
        for (int id = 0; id < libCount(); id++) {
            const LibEntry& e = lib(id);
            if (libIsOp(id) || e.cat == Cat::Control) continue;
            if (mode == WrapMode::Modifiers && !(e.cat == Cat::Time || e.cat == Cat::Structure ||
                                                 e.cat == Cat::Random || e.cat == Cat::Layer ||
                                                 e.cat == Cat::Sample))
                continue;
            std::string head = e.name;
            if (e.defaults) head += std::string(" ") + e.defaults;
            uint16_t nn;
            auto r = tryWrapApp(head, nn);
            if (r) items.push_back(MenuItem{head + " $ ...", r, nn});
        }
        return openMenu(mode == WrapMode::Dollar ? "$ functions" : "pattern modifiers", std::move(items));
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
            if (libIsOp(id) || e.cat == Cat::Control) continue;
            std::string head = e.name;
            if (e.defaults) head += std::string(" ") + e.defaults;
            uint16_t nn;
            auto r = tryWrapApp(head, nn);
            if (r) items.push_back(MenuItem{head + " $ ...", r, nn});
        }
        if (mode == WrapMode::All) addControls();
    }
    openMenu(controlsOnly ? "add control" : mode == WrapMode::Functions ? "functions" : "wrap", std::move(items));
}

// Delete inside "...": a step becomes a rest (timing stays), a rest is
// removed, and the step pad entry just appended is removed (ends entry).
void Editor::doDelete() {
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
        auto r = tryReplace("~", true, nn);
        if (r) {
            commit(std::unique_ptr<Program>(new Program(*r)), nn);
            if (!messageError_) info("rest (Delete again removes the step)");
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
            auto p = std::unique_ptr<Program>(new Program(*prog_));
            uint16_t b = p->block[slot_];
            p->freeTree(b);
            p->block[slot_] = 0;
            snapshot();
            prog_ = std::move(p);
            cursor_ = 0;
            if (backend_ && state_[slot_] == SlotState::Playing)
                backend_->send("d" + std::to_string(slot_ + 1) + " silence");
            state_[slot_] = SlotState::Empty;
            info("slot cleared");
        }
        return;
    }
    Kind pk = cur.at(par).kind;
    auto p = std::unique_ptr<Program>(new Program(*prog_));
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
    commit(std::move(p), newCursor);
    if (!messageError_) info("deleted");
}

void Editor::doUnwrap() {
    if (!cursor_) return;
    const Node& n = prog_->at(cursor_);
    std::vector<uint16_t> tries;
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
        auto p = std::unique_ptr<Program>(new Program(*prog_));
        // detach `keep` from its parent, then put it where the cursor is
        uint16_t kp = p->parentOf(keep);
        uint16_t prev = p->prevSibling(keep);
        if (prev) p->at(prev).next = p->at(keep).next;
        else p->at(kp).first = p->at(keep).next;
        p->at(keep).next = 0;
        p->replace(cursor_, keep);
        p->freeTree(cursor_);
        std::string err;
        if (!recheck(*p, err)) continue;
        uint16_t c = keep;
        while (isTransparent(*p, c)) c = p->at(c).first;
        commit(std::move(p), c);
        if (!messageError_) info("unwrapped");
        return;
    }
    error("can't unwrap this");
}

// "d3", "jumpMod 3 4" or "jumpIn 3 2" for the slot's launch mode.
std::string Editor::launchHead(int slot) const {
    uint16_t b = prog_->block[slot];
    std::string n = std::to_string(slot + 1);
    if (!b) return "d" + n;
    const Node& bn = prog_->at(b);
    switch (blockLaunch(bn)) {
        case Launch::JumpMod: return "jumpMod " + n + " " + std::to_string(bn.b);
        case Launch::JumpIn: return "jumpIn " + n + " " + std::to_string(bn.b);
        default: return "d" + n;
    }
}

// Pattern button: when this block's next evaluation takes effect.
void Editor::openLaunch() {
    uint16_t b = prog_->block[slot_];
    if (!b || !prog_->at(b).first) return error("empty slot: write a pattern first");
    std::string n = std::to_string(slot_ + 1);
    struct { Launch l; uint32_t k; std::string label; } opts[] = {
        {Launch::Now, 0, "d" + n + " $  (now)"},
        {Launch::JumpMod, 2, "jumpMod " + n + " 2  (every 2 cycles)"},
        {Launch::JumpMod, 4, "jumpMod " + n + " 4  (every 4 cycles)"},
        {Launch::JumpMod, 8, "jumpMod " + n + " 8  (every 8 cycles)"},
        {Launch::JumpMod, 16, "jumpMod " + n + " 16  (every 16 cycles)"},
        {Launch::JumpIn, 1, "jumpIn " + n + " 1  (after 1 cycle)"},
        {Launch::JumpIn, 2, "jumpIn " + n + " 2  (after 2 cycles)"},
        {Launch::JumpIn, 4, "jumpIn " + n + " 4  (after 4 cycles)"},
    };
    std::vector<MenuItem> items;
    int sel = 0;
    const Node& cur = prog_->at(b);
    for (auto& o : opts) {
        auto p = std::make_shared<Program>(*prog_);
        setBlockLaunch(p->at(b), o.l, o.k);
        if (o.l == blockLaunch(cur) && (o.l == Launch::Now || o.k == cur.b)) sel = (int)items.size();
        items.push_back(MenuItem{o.label, p, cursor_});
    }
    openMenu("launch", std::move(items));
    menuSel_ = sel;
}

std::string Editor::evalText(int slot, std::string* errOut) const {
    char buf[8192];
    Renderer r(buf, sizeof buf);
    buf[0] = 0;
    uint16_t b = prog_->block[slot];
    if (!b || !prog_->at(b).first) return "d" + std::to_string(slot + 1) + " silence";
    if (slot < kSampleSlots) {
        r.block(*prog_, slot);
        return buf;
    }
    // MIDI channel: route to SuperDirt's MIDI target (spec 9.1); this never
    // goes into the tree or the .tidal file.
    r.expr(*prog_, prog_->at(b).first);
    if (r.overflow() && errOut) *errOut = "block too long";
    // |< : the block's own values win (a `# midichan` in the code), the
    // slot's channel only fills in (same rule as the device mapper)
    return launchHead(slot) + " $ (" + buf + ") |< s \"midi\" |< midichan " + std::to_string(slot - kSampleSlots);
}

void Editor::evaluate() {
    uint16_t b = prog_->block[slot_];
    if (!b || !prog_->at(b).first) return error("nothing to evaluate");
    Error e;
    int holes = 0;
    if (!checkBlock(*prog_, slot_, e, &holes)) {
        state_[slot_] = SlotState::Error;
        return error(e.msg);
    }
    if (holes) return error("fill in the ?? first");
    if (trackerMode_ && !trackerCheck(*prog_, slot_, e)) return error(e.msg);
    std::string err;
    std::string text = evalText(slot_, &err);
    if (!err.empty()) return error(err);
    if (prog_->cpsExpr && backend_) {
        char buf[256];
        Renderer r(buf, sizeof buf);
        buf[0] = 0;
        r.expr(*prog_, prog_->cpsExpr);
        std::string cps = std::string("setcps (") + buf + ")";
        if (cps != lastCps_) {
            backend_->send(cps);
            lastCps_ = cps;
        }
    }
    if (backend_) backend_->send(text);
    state_[slot_] = SlotState::Playing;
    info("d" + std::to_string(slot_ + 1) + " evaluated");
}

}  // namespace proto
