// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// See edit_bench.h.
#include "edit_bench.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "library.h"
#include "mapping.h"
#include "parse.h"

// where the bench's own text buffer lives (the firmware: SDRAM)
#ifndef TC_EDIT_BENCH_ATTR
#define TC_EDIT_BENCH_ATTR
#endif

namespace tc {

namespace {

const char* const kCases[] = {
    "s \"bd*2 [~ sd] bd sd\" # gain 1",
    "every 4 (fast 2) $ n \"0 .. 7\" # s \"arpy\" # speed \"<1 2>\" # room 0.3 # pan sine",
    "jux rev $ every 3 (0.25 <~) $ sometimesBy 0.3 (# speed 2) $ off 0.125 (|+ n 12) $ stut 3 0.5 0.125 $ "
    "n \"0 [3 5] <7 10> [12 ~ 7]*2 0 . 3 5 <7 [10 12]>\" # s \"<arpy pluck>\" # cutoff (range 300 3000 sine) "
    "# resonance 0.2 # room 0.4 # gain 0.9 # pan rand # legato 0.5",
};
const char* const kCaseNames[] = {"light", "everyday", "heavy"};
constexpr int kCaseCount = (int)(sizeof kCases / sizeof kCases[0]);

// d2..d8 around the measured block (a realistic file: 8 blocks in use)
const char* const kBackground =
    "d2 $ s \"hh*8\" # gain 0.8 # pan sine\n\n"
    "d3 $ n \"0 .. 7\" # s \"arpy\" # speed \"<1 2>\"\n\n"
    "d4 $ s \"cp(3,8)\" # room 0.3\n\n"
    "d5 $ n \"<0 3 5 7>\" # s \"bass\" # cutoff 800\n\n"
    "d6 $ every 2 (fast 2) $ s \"tink*4\" # gain 0.7\n\n"
    "d7 $ off 0.25 (|+ n 7) $ n \"0 4 7\" # s \"pluck\"\n\n"
    "d8 $ s \"bd(5,8,2)\" # n \"<0 1>\"\n";

const char* const kSamples[48] = {
    "bd",    "sd",    "hh",    "cp",    "ho",    "cr",     "lt",     "mt",    "ht",    "drum",  "sn",    "tink",
    "808bd", "808sd", "808hc", "808oh", "808cy", "808lt",  "808mt",  "808ht", "808lc", "808mc", "808",   "hh27",
    "arpy",  "bass",  "bass1", "bass2", "bass3", "jvbass", "pluck",  "casio", "blip",  "bend",  "bev",   "bottle",
    "break", "jazz",  "feel",  "future", "gab",  "hand",   "east",   "kurt",  "rave",  "space", "tabla", "wind"};

const char* const kReplaceLits[] = {"silence", "1",         "0.5",       "2",         "(1/8)",
                                    "\"1 2\"", "\"<0 2>\"", "\"bd sd\"", "\"c e g\"", "\"t f t t\"",
                                    "sine",    "saw",       "rand",      "rev",       "(fast 2)",
                                    "(# speed 2)", "(0.25 ~>)"};

// default argument of a control when inserted (as the prototype's)
const char* ctlDefault(const char* name) {
    static const struct {
        const char* n;
        const char* d;
    } t[] = {{"s", "\"bd sd\""}, {"sound", "\"bd sd\""}, {"n", "\"0 .. 3\""}, {"note", "\"c e g\""},
             {"pan", "sine"},    {"cutoff", "1000"},     {"hpf", "200"},      {"bandf", "1000"},
             {"crush", "4"},     {"speed", "2"},         {"ccv", "\"0 64 127\""}, {"ccn", "74"}};
    for (auto& x : t)
        if (!strcmp(x.n, name)) return x.d;
    return "0.5";
}

enum class Strat { Full, Block, InPlace };
enum class Op { Replace, Mini, Wrap };
const char* const kStratName[3] = {"full copy", "block copy", "in place"};

struct Ctx {
    Program *a, *b, *c;
    uint16_t block;   // in a (d1)
    uint16_t target;  // in a
    int targetIdx;    // preorder index of target in the block
};

int preorderIndex(const Program& p, uint16_t root, uint16_t want, int& counter) {
    if (root == want) return counter;
    counter++;
    for (uint16_t ch = p.at(root).first; ch; ch = p.at(ch).next) {
        int r = preorderIndex(p, ch, want, counter);
        if (r >= 0) return r;
    }
    return -1;
}

uint16_t nodeAtPreorder(const Program& p, uint16_t root, int idx, int& counter) {
    if (counter == idx) return root;
    counter++;
    for (uint16_t ch = p.at(root).first; ch; ch = p.at(ch).next) {
        uint16_t r = nodeAtPreorder(p, ch, idx, counter);
        if (r) return r;
    }
    return 0;
}

uint16_t firstOf(const Program& p, uint16_t root, bool (*pred)(Kind)) {
    if (pred(p.at(root).kind)) return root;
    for (uint16_t ch = p.at(root).first; ch; ch = p.at(ch).next)
        if (uint16_t r = firstOf(p, ch, pred)) return r;
    return 0;
}
bool isNumber(Kind k) { return k == Kind::Num || k == Kind::MNum; }
bool isWord(Kind k) { return k == Kind::MWord; }

// Apply edit `op` with `code` at `tgt` in P. Returns the new node (for the
// rollback) or 0 if the code doesn't parse / fit.
uint16_t apply(Program& P, uint16_t tgt, Op op, const char* code) {
    Error e;
    const int len = (int)strlen(code);
    switch (op) {
        case Op::Replace: {
            uint16_t n = parseExpr(P, code, len, e);
            if (!n) return 0;
            P.replace(tgt, n);
            return n;
        }
        case Op::Mini: {
            uint16_t sq = parseMini(P, code, len, e);
            if (!sq) return 0;
            Node& seq = P.at(sq);
            if (!seq.first || P.at(seq.first).next || (seq.flags & kDots)) {
                P.freeTree(sq);
                return 0;
            }
            uint16_t step = seq.first;
            seq.first = 0;
            P.freeTree(sq);
            P.replace(tgt, step);
            return step;
        }
        case Op::Wrap: {
            uint16_t h = parseExpr(P, code, len, e);
            if (!h) return 0;
            uint16_t app;
            if (P.at(h).kind == Kind::App && !(P.at(h).flags & kDollar)) {
                app = h;
            } else {
                app = P.alloc(Kind::App);
                if (!app) return P.freeTree(h), 0;
                P.appendChild(app, h);
            }
            if (exprPrec(P, tgt) < 11) P.at(app).flags |= kDollar;
            P.replace(tgt, app);
            P.appendChild(app, tgt);
            return app;
        }
    }
    return 0;
}

void rollback(Program& P, uint16_t tgt, uint16_t n, Op op) {
    if (op == Op::Wrap) {  // detach tgt (the last child) from the new App first
        Node& app = P.at(n);
        if (app.first == tgt) {
            app.first = 0;
        } else {
            for (uint16_t ch = app.first; ch; ch = P.at(ch).next)
                if (P.at(ch).next == tgt) {
                    P.at(ch).next = 0;
                    break;
                }
        }
    }
    P.replace(n, tgt);
    P.freeTree(n);
}

// One menu item: true if the edited block type-checks.
bool tryItem(Ctx& x, Strat s, Op op, const char* code) {
    Error e;
    switch (s) {
        case Strat::Full: {
            *x.b = *x.a;
            uint16_t n = apply(*x.b, x.target, op, code);
            return n && checkBlockNode(*x.b, x.block, e);
        }
        case Strat::Block: {
            x.c->clear();
            uint16_t blk = x.c->copyTree(*x.a, x.block);
            if (!blk) return false;
            int counter = 0;
            uint16_t tgt = nodeAtPreorder(*x.c, blk, x.targetIdx, counter);
            if (!tgt) return false;
            uint16_t n = apply(*x.c, tgt, op, code);
            return n && checkBlockNode(*x.c, blk, e);
        }
        case Strat::InPlace: {
            uint16_t n = apply(*x.a, x.target, op, code);
            if (!n) return false;
            bool ok = checkBlockNode(*x.a, x.block, e);
            rollback(*x.a, x.target, n, op);
            return ok;
        }
    }
    return false;
}

struct Out {
    char* p;
    int cap, n;
    void put(const char* fmt, ...) {
        if (n >= cap - 1) return;
        va_list ap;
        va_start(ap, fmt);
        int k = vsnprintf(p + n, cap - n, fmt, ap);
        va_end(ap);
        if (k > 0) n = n + k < cap - 1 ? n + k : cap - 1;
    }
};

// Builds menu `op` on x.target with strategy s; returns items that check.
int buildMenu(Ctx& x, Strat s, Op op, int& items, const char* stepText) {
    char code[160];
    int ok = 0;
    items = 0;
    auto one = [&](const char* c) {
        items++;
        if (tryItem(x, s, op, c)) ok++;
    };
    if (op == Op::Replace) {
        for (const char* l : kReplaceLits) one(l);
        for (int id = 0; id < libCount(); id++) {
            if (libIsOp(id)) continue;
            const LibEntry& le = lib(id);
            const char* d = le.cat == Cat::Control ? ctlDefault(le.name) : le.defaults;
            snprintf(code, sizeof code, "%s%s%s", le.name, d ? " " : "", d ? d : "");
            one(code);
        }
    } else if (op == Op::Wrap) {
        for (int id = 0; id < libCount(); id++) {
            if (libIsOp(id) || lib(id).cat == Cat::Control) continue;
            const LibEntry& le = lib(id);
            snprintf(code, sizeof code, "%s%s%s", le.name, le.defaults ? " " : "", le.defaults ? le.defaults : "");
            one(code);
        }
    } else {
        one("~");
        for (const char* smp : kSamples) one(smp);
        static const char* const kVals[] = {"0", "1", "2", "3", "0.5", "c", "e", "g", "t", "f"};
        for (const char* v : kVals) one(v);
        snprintf(code, sizeof code, "[%s %s]", stepText, stepText);
        one(code);
        snprintf(code, sizeof code, "<%s %s>", stepText, stepText);
        one(code);
        snprintf(code, sizeof code, "[%s, %s]", stepText, stepText);
        one(code);
    }
    return ok;
}

}  // namespace

int editBenchCaseCount() { return kCaseCount; }
const char* editBenchCase(int i) { return i >= 0 && i < kCaseCount ? kCases[i] : ""; }
const char* editBenchCaseName(int i) { return i >= 0 && i < kCaseCount ? kCaseNames[i] : ""; }

int runEditBench(Program* a, Program* b, Program* c, Span* spans, char* text, int textCap, EditBenchClock clock,
                 int reps, char* outBuf, int cap) {
    Out out{outBuf, cap, 0};
    outBuf[0] = 0;
    if (reps < 1) reps = 1;
    // whole-Program copy (what the prototype does for every edit and item)
    a->clear();
    {
        uint32_t t0 = clock();
        for (int r = 0; r < reps; r++) {
            *b = *a;
            __asm__ volatile("" ::: "memory");  // keep every copy
        }
        uint32_t us = (clock() - t0) / reps;
        out.put("Program: %u B; whole copy %lu us\n\n", (unsigned)sizeof(Program), (unsigned long)us);
    }
    TC_EDIT_BENCH_ATTR static char file[2048];
    for (int ci = 0; ci < kCaseCount; ci++) {
        snprintf(file, sizeof file, "setcps (120/60/4)\n\nd1 $ %s\n\n%s", kCases[ci], kBackground);
        Error e;
        // parse the whole file (8 blocks)
        uint32_t t0 = clock();
        bool parsed = true;
        for (int r = 0; r < reps && parsed; r++) parsed = parseFile(*a, file, (int)strlen(file), e);
        const uint32_t parseUs = (clock() - t0) / reps;
        if (!parsed) {
            out.put("%s: parse error: %s\n", kCaseNames[ci], e.msg);
            continue;
        }
        if (!checkProgram(*a, e)) {
            out.put("%s: type error: %s\n", kCaseNames[ci], e.msg);
            continue;
        }
        Ctx x{a, b, c, a->block[0], 0, 0};
        const uint16_t expr = a->at(x.block).first;
        const int nodes = a->subtreeSize(x.block);
        out.put("== %s (d1: %d nodes; program %d live nodes, %d strings)\n   %s\n", kCaseNames[ci], nodes,
                a->liveNodes(), a->stringCount(), kCases[ci]);

        // single operations
        uint32_t checkUs = 0, trackUs = 0, renderUs = 0, blockCopyUs = 0, undoUs = 0;
        int textLen = 0;
        t0 = clock();
        for (int r = 0; r < reps; r++) checkBlockNode(*a, x.block, e);
        checkUs = (clock() - t0) / reps;
        t0 = clock();
        for (int r = 0; r < reps; r++) trackerCheck(*a, 0, e);
        trackUs = (clock() - t0) / reps;
        t0 = clock();
        for (int r = 0; r < reps; r++) {
            Renderer rd(text, textCap, spans);
            textLen = rd.blockNode(*a, x.block);
        }
        renderUs = (clock() - t0) / reps;
        t0 = clock();
        for (int r = 0; r < reps; r++) {
            c->clear();
            c->copyTree(*a, x.block);
        }
        blockCopyUs = (clock() - t0) / reps;
        // undo by text: render the expression, parse it back in place
        t0 = clock();
        for (int r = 0; r < reps; r++) {
            const uint16_t old = a->at(x.block).first;
            Renderer rd(text, textCap);
            int len = rd.expr(*a, old);
            uint16_t n = parseExpr(*a, text, len, e);
            if (!n) break;
            a->replace(old, n);
            a->freeTree(old);
            checkBlockNode(*a, x.block, e);
        }
        undoUs = (clock() - t0) / reps;
        out.put("   parse file %lu us, check d1 %lu us, trackerCheck %lu us, render %lu us (%d chars),\n"
                "   block copy %lu us, undo by text (render + parse + check) %lu us\n",
                (unsigned long)parseUs, (unsigned long)checkUs, (unsigned long)trackUs, (unsigned long)renderUs,
                textLen, (unsigned long)blockCopyUs, (unsigned long)undoUs);

        // a jog step on the block's first number: edit + check
        const uint16_t num = firstOf(*a, a->at(x.block).first, isNumber);
        if (num) {
            int counter = 0;
            const int numIdx = preorderIndex(*a, x.block, num, counter);
            uint32_t jog[3];
            for (int s = 0; s < 3; s++) {
                t0 = clock();
                for (int r = 0; r < reps; r++) {
                    if (s == 0) {
                        *b = *a;
                        b->at(num).a += 1;
                        checkBlockNode(*b, x.block, e);
                    } else if (s == 1) {
                        c->clear();
                        uint16_t blk = c->copyTree(*a, x.block);
                        int k = 0;
                        uint16_t n = nodeAtPreorder(*c, blk, numIdx, k);
                        if (n) c->at(n).a += 1;
                        checkBlockNode(*c, blk, e);
                    } else {
                        a->at(num).a += 1;
                        checkBlockNode(*a, x.block, e);
                        a->at(num).a -= 1;
                    }
                }
                jog[s] = (clock() - t0) / reps;
            }
            checkBlockNode(*a, x.block, e);
            out.put("   jog edit (number + check): full %lu us, block %lu us, in place %lu us\n", (unsigned long)jog[0],
                    (unsigned long)jog[1], (unsigned long)jog[2]);
        }

        // menus built by trial
        struct M {
            const char* name;
            Op op;
            uint16_t target;
        } menus[] = {
            {"replace (pattern)", Op::Replace, a->at(x.block).first},
            {"$ functions (wrap)", Op::Wrap, a->at(x.block).first},
            {"mini step", Op::Mini, firstOf(*a, a->at(x.block).first, isWord)},
        };
        for (const M& m : menus) {
            if (!m.target) continue;
            x.target = m.target;
            int counter = 0;
            x.targetIdx = preorderIndex(*a, x.block, m.target, counter);
            char step[32] = "bd";
            if (m.op == Op::Mini) snprintf(step, sizeof step, "%s", a->str((int)a->at(m.target).a));
            out.put("   menu %-19s", m.name);
            int items = 0;
            for (int s = 0; s < 3; s++) {
                t0 = clock();
                int ok = buildMenu(x, (Strat)s, m.op, items, step);
                if (s == 2) checkBlockNode(*a, x.block, e);  // restore the types after the trials
                uint32_t us = clock() - t0;
                out.put("%s %s %lu us (%d/%d ok)", s ? "," : "", kStratName[s], (unsigned long)us, ok, items);
            }
            out.put("\n");
        }
        (void)expr;
        out.put("\n");
    }
    return out.n;
}

}  // namespace tc
