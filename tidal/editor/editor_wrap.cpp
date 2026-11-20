// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Wrapping: the structure keys (Screen 1-8: [] <> {} () | ...), mini-notation
// wraps and the $ / # wrap menus (openWrap).
#include "editor_impl.h"

namespace tc {

// Screen 1-8 (v2, docs/tidal_ui_v2.md): structure menus at the cursor.
void Editor::openStruct(int i) {
    switch (i) {
        case 0: return openWrap(WrapMode::Dollar);
        case 1: return openWrap(WrapMode::Controls);
        case 7: {
            // RC19 notes: add a control with Screen 2, then change its
            // operator here: the nearest operator around the cursor (or the
            // one whose symbol it is on) and every operator that fits there
            // ... on the operator's symbol or its right side (where Screen 2
            // leaves the cursor); on a left side the menu that adds one stays
            uint16_t o = 0;
            if (onOperator() && prog_->at(cursor_).kind == Kind::Op) {
                o = cursor_;
            } else {
                uint16_t c = gapSeq_ ? gapSeq_ : cursor_;
                for (uint16_t up = c ? prog_->parentOf(c) : 0; up && prog_->at(up).kind != Kind::Block;
                     c = up, up = prog_->parentOf(up)) {
                    if (prog_->at(up).kind != Kind::Op) continue;
                    if (prog_->at(up).first != c) o = up;  // the right side
                    break;
                }
            }
            if (!cps_ && o) {
                gapSeq_ = 0;
                cursor_ = o;
                onOp_ = true;
                return openOps();
            }
            return openWrap(WrapMode::Ops);  // no operator yet: add one (with a control)
        }
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
// Screen 3-6 (RC19 notes): no menu, the bracket goes on at once: a step /
// selection / the quotes' sequence / a plain number becomes [x] <x> {x}%4
// x(3,8); at a gap a new [~] <~> {~}%4 ~(3,8) with the cursor on its ~ so
// pads enter into it. The cursor lands inside (euclid: on its 3).
static uint16_t firstLeaf(const Program& p, uint16_t n) {
    while (n && p.at(n).first) n = p.at(n).first;
    return n;
}

void Editor::wrapMini(int k) {
    static const char* const kWrap[4] = {"[$]", "<$>", "{$}%4", "@(3,8)"};
    static const char* const kNumWrap[4] = {"[$]", "<$>", "{$}%4", "$(3,8)"};
    static const char* const kGap[4] = {"[~]", "<~>", "{~}%4", "~(3,8)"};
    Label what = kMiniTitle[k];
    // where the cursor lands in what was made: brackets: the first step, euclid: k
    auto land = [&](uint16_t made) {
        const Program& p = *prog_;
        uint16_t c = made;
        while (c && p.at(c).kind == Kind::MSeq && p.at(c).first && !p.at(p.at(c).first).next) c = p.at(c).first;
        if (c && p.at(c).kind == Kind::MEuclid && p.at(c).first && p.at(p.at(c).first).next)
            return firstLeaf(p, p.at(p.at(c).first).next);
        return firstLeaf(p, c);
    };
    int selPos;
    const int selN = selRange(selPos);
    if (selN > 0) {
        EditSpec sp;
        sp.kind = EditKind::ReplaceRun;
        sp.mini = true;
        sp.seq = selSeq_;
        sp.pos = selPos;
        sp.cycles = (uint32_t)selN;
        const Text t = selText();
        if (!fillPattern(sp.code, kWrap[k], t, "[" + t + "]", false)) return error("selection too long");
        selA_ = selB_ = selSeq_ = 0;
        uint16_t nn;
        if (!doSpec(sp, nn)) return error(sp.code + " doesn't fit here");
        cursor_ = land(nn);
        return info(sp.code);
    }
    if (gapSeq_) {
        if (listGap()) return error(what + ": inside \"...\" only");
        EditSpec sp;
        sp.kind = EditKind::InsertGap;
        sp.seq = gapSeq_;
        sp.pos = gapPos_;
        sp.code = kGap[k];
        uint16_t nn;
        if (!doSpec(sp, nn)) return error(Text("can't insert ") + kGap[k] + " here");
        gapSeq_ = 0;
        cursor_ = firstLeaf(*prog_, nn);  // its ~: the next pad replaces it
        return info(Text(kGap[k]) + ": pads enter into it");
    }
    if (!cursor_) return error("select a step inside \"...\" first");
    Kind kind = prog_->at(cursor_).kind;
    const uint16_t oldCursor = cursor_;
    if (kind == Kind::Mini) {  // on the quotes: the whole sequence inside them
        cursor_ = prog_->at(cursor_).first;
        kind = cursor_ ? prog_->at(cursor_).kind : Kind::None;
    } else if (isMiniKind(kind) && kind != Kind::MSeq) {
        cursor_ = stepOf(*prog_, cursor_);  // `bd` of bd*2: the whole step
        kind = prog_->at(cursor_).kind;
    }
    const bool num = kind == Kind::Num;
    if (!num && !isMiniKind(kind)) {
        cursor_ = oldCursor;
        return error(what + ": on a step inside \"...\" or a number");
    }
    Text t = renderNode(*prog_, cursor_);
    if (num) {
        if (t.size() > 2 && t[0] == '(' && t.back() == ')') t = t.substr(1, t.size() - 2);  // (-1)
        if (t.find('/') != Text::npos) {
            cursor_ = oldCursor;
            return error("a fraction can't go inside \"...\"; jog it first");
        }
    }
    const bool multi = kind == Kind::MSeq && prog_->at(cursor_).first && prog_->at(prog_->at(cursor_).first).next;
    EditSpec sp;
    sp.kind = EditKind::Replace;
    sp.mini = !num;
    sp.target = cursor_;
    if (!fillPattern(sp.code, num ? kNumWrap[k] : kWrap[k], t, multi ? "[" + t + "]" : t, num)) {
        cursor_ = oldCursor;
        return error("too long");
    }
    uint16_t nn;
    if (!doSpec(sp, nn)) {
        cursor_ = oldCursor;
        return error(sp.code + " doesn't fit here");
    }
    // a number became "...": into its sequence; a step: into the new bracket
    cursor_ = land(prog_->at(nn).kind == Kind::Mini ? prog_->at(nn).first : nn);
    info(sp.code);
}

void Editor::openMini(int k) {
    if (k < 0 || k > 4) k = 4;
    if (k < 4) return wrapMini(k);
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
        sortMenu(items);
        return (void)openMenu("replace function");
    }
    if (mode == WrapMode::Fx) {
        for (int id = 0; id < libCount(); id++) {
            const LibEntry& e = lib(id);
            if (e.cat != Cat::Control || !isFx(e.name)) continue;
            Text rhs = Text(e.name) + " " + controlDefault(e.name);
            addOp("#", rhs, true, "# " + rhs);
        }
        sortMenu(items);
        return (void)openMenu("effects");
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
        sortMenu(items);
        return (void)openMenu("functions");
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
    sortMenu(items);
    openMenu(controlsOnly ? "controls" : mode == WrapMode::Functions ? "functions" : "wrap");
}

}  // namespace tc
