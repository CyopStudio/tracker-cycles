#include "check.h"

#include <stdio.h>
#include <string.h>

#include <new>

#include "library.h"
#include "render.h"

namespace tc {

const char* tyName(uint8_t c) {
    static const char* pats[] = {"Pat Time", "Pat Double", "Pat Int", "Pat Note",
                                 "Pat String", "Pat Bool", "ControlPattern"};
    if (c >= kTyPatBase && c < kTyPatBase + (int)Base::Count) return pats[c - kTyPatBase];
    switch (c) {
        case kTyFn: return "function";
        case kTyList: return "list";
        case kTyPatVar: return "Pat a";
        default: return "?";
    }
}

bool parseNoteName(const char* s, int len, int& v) {
    static const int pcs[7] = {9, 11, 0, 2, 4, 5, 7};  // a b c d e f g
    if (len < 1 || s[0] < 'a' || s[0] > 'g') return false;
    int x = pcs[s[0] - 'a'];
    int i = 1;
    while (i < len && (s[i] == 's' || s[i] == 'f' || s[i] == 'n')) {
        x += s[i] == 's' ? 1 : s[i] == 'f' ? -1 : 0;
        i++;
    }
    int oct = 5;
    if (i < len) {
        oct = 0;
        int st = i;
        while (i < len && s[i] >= '0' && s[i] <= '9') oct = oct * 10 + (s[i++] - '0');
        if (i == st || i != len || oct > 10) return false;
    }
    v = x + (oct - 5) * 12;
    return true;
}

namespace {

struct MiniRec {
    uint16_t node;
    uint16_t elem;
};

struct Checker {
    Program& p;
    Error& err;
    TypePool pool;
    uint16_t termOf[kMaxNodes];
    uint16_t visited[kMaxBlockNodes * 2];
    int nvisited = 0;
    MiniRec minis[256];
    int nminis = 0;
    int holes = 0;
    char msg[96];

    Checker(Program& prog, Error& e) : p(prog), err(e) { pool.clear(); }

    uint16_t fail(uint16_t node, const char* m) {
        if (!err.failed) {
            err.set(-1, "%s", m);
            err.node = node;
        }
        return 0;
    }
    bool unifyAt(uint16_t node, uint16_t a, uint16_t b, const char* ctx) {
        if (!a || !b) return fail(node, "type pool full"), false;
        if (pool.unify(a, b, msg, sizeof msg)) return true;
        char full[96];
        snprintf(full, sizeof full, "%s: %s", ctx, msg);
        fail(node, full);
        return false;
    }
    void record(uint16_t node, uint16_t t) {
        termOf[node] = t;
        if (nvisited < (int)(sizeof visited / sizeof visited[0])) visited[nvisited++] = node;
    }

    uint16_t infer(uint16_t idx, int depth = 0) {
        if (err.failed) return 0;
        if (depth > kMaxDepth) return fail(idx, "nesting too deep");
        Node& n = p.at(idx);
        uint16_t t = 0;
        char ctx[64];
        switch (n.kind) {
            case Kind::Num: {
                uint8_t c = kCLit | (nodeDecimals(n) > 0 ? kCFracLit : 0);
                t = pool.pat(pool.var(c));
                break;
            }
            case Kind::Mini: {
                uint16_t e = pool.var(0);
                if (nminis >= 256) return fail(idx, "too many strings in one block");
                minis[nminis++] = MiniRec{idx, e};
                t = pool.pat(e);
                break;
            }
            case Kind::Fn:
                t = pool.instantiate(lib((int)n.a).sig);
                if (!t) return fail(idx, "bad library signature");
                break;
            case Kind::Hole:
                holes++;
                t = pool.var(0);
                break;
            case Kind::App: {
                uint16_t c = n.first;
                uint16_t f = infer(c, depth + 1);
                const char* fname = p.at(c).kind == Kind::Fn ? lib((int)p.at(c).a).name : "function";
                int argn = 1;
                for (c = p.at(c).next; c && !err.failed; c = p.at(c).next, argn++) {
                    uint16_t a = infer(c, depth + 1);
                    uint16_t r = pool.var(0);
                    snprintf(ctx, sizeof ctx, "argument %d of '%s'", argn, fname);
                    uint16_t ft = pool.resolve(f);
                    if (pool.at(ft).tag != Tag::Fn && pool.at(ft).tag != Tag::Var) {
                        snprintf(msg, sizeof msg, "'%s' takes only %d argument%s", fname,
                                 argn - 1, argn == 2 ? "" : "s");
                        return fail(c, msg);
                    }
                    if (!unifyAt(c, f, pool.fn(a, r), ctx)) return 0;
                    f = r;
                }
                t = f;
                break;
            }
            case Kind::Op: {
                const LibEntry& op = lib((int)n.a);
                uint16_t sig = pool.instantiate(op.sig);
                uint16_t l = infer(n.first, depth + 1);
                uint16_t r = infer(p.at(n.first).next, depth + 1);
                uint16_t res = pool.var(0);
                snprintf(ctx, sizeof ctx, "operator %s", op.name);
                if (!unifyAt(idx, sig, pool.fn(l, pool.fn(r, res)), ctx)) return 0;
                t = res;
                break;
            }
            case Kind::SectionR: {
                const LibEntry& op = lib((int)n.a);
                uint16_t sig = pool.instantiate(op.sig);
                uint16_t r = infer(n.first, depth + 1);
                uint16_t a = pool.var(0), c = pool.var(0);
                snprintf(ctx, sizeof ctx, "section (%s ...)", op.name);
                if (!unifyAt(idx, sig, pool.fn(a, pool.fn(r, c)), ctx)) return 0;
                t = pool.fn(a, c);
                break;
            }
            case Kind::SectionL: {
                const LibEntry& op = lib((int)n.a);
                uint16_t sig = pool.instantiate(op.sig);
                uint16_t l = infer(n.first, depth + 1);
                uint16_t b = pool.var(0), c = pool.var(0);
                snprintf(ctx, sizeof ctx, "section (... %s)", op.name);
                if (!unifyAt(idx, sig, pool.fn(l, pool.fn(b, c)), ctx)) return 0;
                t = pool.fn(b, c);
                break;
            }
            case Kind::Compose: {
                uint16_t f = infer(n.first, depth + 1);
                uint16_t g = infer(p.at(n.first).next, depth + 1);
                uint16_t a = pool.var(0), b = pool.var(0), c = pool.var(0);
                if (!unifyAt(n.first, f, pool.fn(b, c), "left of '.'")) return 0;
                if (!unifyAt(p.at(n.first).next, g, pool.fn(a, b), "right of '.'")) return 0;
                t = pool.fn(a, c);
                break;
            }
            case Kind::List: {
                uint16_t e = pool.var(0);
                for (uint16_t c = n.first; c && !err.failed; c = p.at(c).next) {
                    uint16_t ct = infer(c, depth + 1);
                    if (!unifyAt(c, e, ct, "list elements must have the same type")) return 0;
                }
                t = pool.list(e);
                break;
            }
            default: return fail(idx, "unexpected node in expression");
        }
        if (!t && !err.failed) return fail(idx, "type pool full");
        record(idx, t);
        return t;
    }

    uint8_t code(uint16_t t) {
        t = pool.resolve(t);
        const Term& x = pool.at(t);
        switch (x.tag) {
            case Tag::Pat: {
                uint16_t e = pool.resolve(x.a);
                if (pool.at(e).tag == Tag::Base) return tyPat(pool.at(e).base);
                return kTyPatVar;
            }
            case Tag::Fn: return kTyFn;
            case Tag::List: return kTyList;
            default: return kTyUnknown;
        }
    }

    // ---- mini-notation -----------------------------------------------------
    void toWord(uint16_t idx) {
        Node& n = p.at(idx);
        char buf[32];
        Renderer r(buf, sizeof buf);
        if (n.kind == Kind::MNote && n.b) {
            snprintf(buf, sizeof buf, "%s", p.str((int)n.b - 1));
        } else {
            uint16_t savedNext = n.next;
            n.next = 0;
            r.expr(p, idx);
            n.next = savedNext;
        }
        int id = p.intern(buf, (int)strlen(buf));
        if (id < 0) {
            fail(idx, "too many words");
            return;
        }
        n.kind = Kind::MWord;
        n.a = (uint32_t)id;
        n.b = 0;
        n.flags = 0;
    }

    bool atom(uint16_t idx, Base b) {
        Node& n = p.at(idx);
        char m[96];
        auto bad = [&](const char* what) {
            snprintf(m, sizeof m, "%s is not allowed in a %s pattern", what, baseName(b));
            fail(idx, m);
            return false;
        };
        // bring the atom back to a neutral form first, then refine for b
        if (b != Base::Note && (n.kind == Kind::MNote || n.kind == Kind::MChord)) toWord(idx);
        if (b != Base::Bool && n.kind == Kind::MBool) {
            if (n.flags & kDigitStyle) {
                int v = (int)n.a;
                n.kind = Kind::MNum;
                n.flags = 0;
                setNodeRational(n, Rational{v, 1}, 0);
            } else {
                toWord(idx);
            }
        }
        if (err.failed) return false;
        switch (n.kind) {
            case Kind::MRest: return true;
            case Kind::MWord: {
                const char* w = p.str((int)n.a);
                int wl = (int)strlen(w);
                if (b == Base::String) return true;
                if (b == Base::Note && !n.b) {
                    int v;
                    const char* q = strchr(w, '\'');
                    if (q) {
                        if (!parseNoteName(w, (int)(q - w), v)) return bad("this chord root");
                        n.kind = Kind::MChord;  // a stays the string id
                        return true;
                    }
                    if (parseNoteName(w, wl, v)) {
                        uint32_t spelling = n.a + 1;
                        n.kind = Kind::MNote;
                        n.a = (uint32_t)v;
                        n.b = spelling;
                        return true;
                    }
                    snprintf(m, sizeof m, "'%s' is not a note name", w);
                    return fail(idx, m), false;
                }
                if (b == Base::Bool && (strcmp(w, "t") == 0 || strcmp(w, "f") == 0)) {
                    n.kind = Kind::MBool;
                    n.a = w[0] == 't';
                    n.b = 0;
                    n.flags = 0;
                    return true;
                }
                snprintf(m, sizeof m, "'%s' is not a %s", w, baseName(b));
                return fail(idx, m), false;
            }
            case Kind::MNum: {
                Rational r = nodeRational(n);
                bool integer = r.den == 1;
                switch (b) {
                    case Base::Time:
                    case Base::Double:
                    case Base::Note: return true;
                    case Base::Int: return integer ? true : bad("a fractional number");
                    case Base::String: toWord(idx); return !err.failed;
                    case Base::Bool:
                        if (integer && (r.num == 0 || r.num == 1)) {
                            n.kind = Kind::MBool;
                            n.a = (uint32_t)r.num;
                            n.b = 0;
                            n.flags = kDigitStyle;
                            return true;
                        }
                        return bad("this number");
                    default: return bad("a number");
                }
            }
            case Kind::MNote:
            case Kind::MChord: return b == Base::Note ? true : bad("a note");
            case Kind::MBool: return b == Base::Bool ? true : bad("a bool");
            default: return bad("this");
        }
    }

    bool mini(uint16_t idx, Base b, int depth = 0) {
        if (err.failed) return false;
        if (depth > kMaxDepth) return fail(idx, "nesting too deep"), false;
        Node& n = p.at(idx);
        n.type = tyPat(b);
        if (isMiniAtom(n.kind)) return atom(idx, b);
        uint16_t first = n.first;
        switch (n.kind) {
            case Kind::MSeq:
            case Kind::MSub:
            case Kind::MAlt:
            case Kind::MChoice:
                for (uint16_t c = first; c; c = p.at(c).next)
                    if (!mini(c, b, depth + 1)) return false;
                return true;
            case Kind::MPoly:
                for (uint16_t c = first; c; c = p.at(c).next) {
                    bool stepsArg = (n.flags & kHasSteps) && !p.at(c).next;
                    if (!mini(c, stepsArg ? Base::Time : b, depth + 1)) return false;
                }
                return true;
            case Kind::MFast:
            case Kind::MSlow:
            case Kind::MWeight:
            case Kind::MDegrade:
            case Kind::MEuclid: {
                if (!mini(first, b, depth + 1)) return false;
                Base ab = n.kind == Kind::MDegrade ? Base::Double
                          : n.kind == Kind::MEuclid ? Base::Int
                                                    : Base::Time;
                for (uint16_t c = p.at(first).next; c; c = p.at(c).next)
                    if (!mini(c, ab, depth + 1)) return false;
                return true;
            }
            case Kind::MReplicate: return mini(first, b, depth + 1);
            case Kind::MRange: {
                if (b == Base::String || b == Base::Bool) {
                    fail(idx, "'..' needs numbers or notes");
                    return false;
                }
                return mini(first, b, depth + 1) && mini(p.at(first).next, b, depth + 1);
            }
            default: return fail(idx, "unexpected node in mini-notation"), false;
        }
    }

    bool finish() {
        if (err.failed) return false;
        for (int i = 0; i < nminis; i++) {
            uint16_t e = pool.resolve(minis[i].elem);
            const Term& x = pool.at(e);
            if (x.tag != Tag::Base) {
                fail(minis[i].node, "ambiguous string type; pass it to a control like s or n");
                return false;
            }
            if (x.base == Base::Control) {
                fail(minis[i].node, "a string can't be a ControlPattern; use s \"...\" or n \"...\"");
                return false;
            }
            uint16_t root = p.at(minis[i].node).first;
            if (root && !mini(root, x.base)) return false;
        }
        for (int i = 0; i < nvisited; i++) p.at(visited[i]).type = code(termOf[visited[i]]);
        return true;
    }
};

// TC_CHECKER_ATTR places the checker (~27 KB), e.g. in SDRAM on the device
// (core/tidal/tidal_config.h): it runs on evaluate / edits only, never in
// the audio path. Raw storage + placement-new, so no static constructor.
#ifndef TC_CHECKER_ATTR
#define TC_CHECKER_ATTR
#endif
Checker* gChecker = nullptr;
TC_CHECKER_ATTR alignas(Checker) unsigned char gCheckerStorage[sizeof(Checker)];

Checker& freshChecker(Program& p, Error& e) {
    // Fixed storage, no heap: the checker is large (type pool + per-node map).
    gChecker = new (gCheckerStorage) Checker(p, e);
    return *gChecker;
}

}  // namespace

bool checkBlock(Program& p, int slot, Error& err, int* holes) { return checkBlockNode(p, p.block[slot], err, holes); }

bool checkBlockNode(Program& p, uint16_t b, Error& err, int* holes) {
    if (holes) *holes = 0;
    if (!b || !p.at(b).first) return true;
    Checker& c = freshChecker(p, err);
    uint16_t e = p.at(b).first;
    uint16_t t = c.infer(e);
    if (!err.failed &&
        !c.unifyAt(e, t, c.pool.pat(c.pool.base(Base::Control)), "a block must be a ControlPattern"))
        return false;
    if (!c.finish()) return false;
    p.at(b).type = tyPat(Base::Control);
    if (holes) *holes = c.holes;
    return true;
}

bool checkCps(Program& p, Error& err) {
    if (!p.cpsExpr) return true;
    Checker& c = freshChecker(p, err);
    uint16_t t = c.infer(p.cpsExpr);
    if (!err.failed &&
        !c.unifyAt(p.cpsExpr, t, c.pool.pat(c.pool.base(Base::Double)), "setcps needs a number"))
        return false;
    return c.finish();
}

bool checkProgram(Program& p, Error& err) {
    if (!checkCps(p, err)) return false;
    for (int i = 0; i < p.blockCount(); i++)  // every block, alternatives too
        if (!checkBlockNode(p, p.blockAt(i), err)) return false;
    for (int s = 0; s < kSlots; s++)  // a selected root not in the list (built in code)
        if (p.block[s] && p.blockIndex(p.block[s]) < 0 && !checkBlock(p, s, err)) return false;
    return true;
}

}  // namespace tc
