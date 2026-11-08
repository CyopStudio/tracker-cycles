#include "editor.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>

#include "check.h"
#include "library.h"
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
        {"cut", "1"},           {"orbit", "1"},
    };
    for (auto& x : t)
        if (strcmp(x.n, name) == 0) return x.d;
    return "1";
}

Editor::Editor() : prog_(new Program) {
    for (auto& s : state_) s = SlotState::Empty;
    samples_ = {"bd", "sd", "hh", "cp", "arpy", "drum", "bass", "superpiano", "808", "jvbass", "feel", "sn"};
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
    return true;
}

void Editor::snapshot() {
    undo_.emplace_back(new Program(*prog_));
    if (undo_.size() > 64) undo_.erase(undo_.begin());
}

void Editor::commit(std::unique_ptr<Program> p, uint16_t cursor) {
    std::string err;
    if (!recheck(*p, err)) {
        error(err);
        return;
    }
    snapshot();
    prog_ = std::move(p);
    cursor_ = cursor;
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

void Editor::selectMenu(int i) {
    if (menuOpen_ && i >= 0 && i < (int)menu_.size()) menuSel_ = i;
}

std::string Editor::cursorInfo() const {
    if (!cursor_) return prog_->block[slot_] ? "" : "empty slot: replace to start a pattern";
    const Node& n = prog_->at(cursor_);
    std::string s = kindName(n.kind);
    if (n.kind == Kind::Fn || n.kind == Kind::Op || n.kind == Kind::SectionL || n.kind == Kind::SectionR)
        s += std::string(" ") + lib((int)n.a).name;
    if (n.type) s += std::string(" : ") + tyName(n.type);
    return s;
}

// ---------------------------------------------------------------------------
// actions

void Editor::act(Action a) {
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
                snapshot();
                prog_.reset(new Program(*it.result));
                cursor_ = it.cursor;
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
        case Action::Wrap: openWrap(false); break;
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
                openWrap(true);
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
    if (!cursor_) return;
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
                int k = i < (int)samples_.size() ? i + dir : 0;
                k = (k % (int)samples_.size() + (int)samples_.size()) % (int)samples_.size();
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

void Editor::openReplace() {
    std::vector<MenuItem> items;
    auto add = [&](const std::string& label, const std::string& code, bool mini) {
        uint16_t nn;
        auto r = tryReplace(code, mini, nn);
        if (r) items.push_back(MenuItem{label, r, nn});
    };
    if (!cursor_) {
        // empty slot: create the block, then offer starters
        auto p = std::unique_ptr<Program>(new Program(*prog_));
        uint16_t b = p->alloc(Kind::Block);
        p->at(b).a = (uint32_t)slot_;
        p->block[slot_] = b;
        prog_ = std::move(p);
        cursor_ = b;
    }
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
        for (auto& s : samples_) add(s, s, true);
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

void Editor::openWrap(bool controlsOnly) {
    if (!cursor_ || prog_->at(cursor_).kind == Kind::Block) return error("select a pattern first");
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
        addControls();
    }
    openMenu(controlsOnly ? "add control" : "wrap", std::move(items));
}

void Editor::doDelete() {
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
    return "d" + std::to_string(slot + 1) + " $ (" + buf + ") # s \"midi\" # midichan " +
           std::to_string(slot - kSampleSlots);
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
    if (trackerMode_) {
        // walk this block's subtree for functions the device won't have
        int hostOnly = -1;
        std::vector<uint16_t> stack{b};
        while (!stack.empty() && hostOnly < 0) {
            uint16_t x = stack.back();
            stack.pop_back();
            const Node& n = prog_->at(x);
            if (n.kind == Kind::Fn && lib((int)n.a).device == Device::HostOnly) hostOnly = (int)n.a;
            for (uint16_t c = n.first; c; c = prog_->at(c).next) stack.push_back(c);
        }
        if (hostOnly >= 0)
            return error(std::string("tracker mode: '") + lib(hostOnly).name +
                         "' isn't available on the Tracker");
    }
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
