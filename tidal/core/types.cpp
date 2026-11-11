#include "types.h"

#include <stdio.h>
#include <string.h>

#include <initializer_list>

namespace tc {

const char* baseName(Base b) {
    static const char* n[] = {"Time", "Double", "Int", "Note", "String", "Bool", "Control"};
    return (int)b < (int)Base::Count ? n[(int)b] : "?";
}

bool baseSatisfies(Base b, uint8_t c) {
    auto in = [&](std::initializer_list<Base> set) {
        for (Base x : set)
            if (x == b) return true;
        return false;
    };
    if ((c & kCNum) && !in({Base::Time, Base::Double, Base::Int, Base::Note, Base::Control}))
        return false;
    if ((c & kCFrac) && !in({Base::Time, Base::Double, Base::Note, Base::Control})) return false;
    if ((c & kCLit) && !in({Base::Time, Base::Double, Base::Int, Base::Note})) return false;
    if ((c & kCFracLit) && !in({Base::Time, Base::Double, Base::Note})) return false;
    return true;
}

uint16_t TypePool::add(Term t) {
    if (n_ >= kMax) return 0;
    t_[n_] = t;
    return (uint16_t)n_++;
}
uint16_t TypePool::var(uint8_t cons) { return add(Term{Tag::Var, Base::Time, cons, 0, 0, 0}); }
uint16_t TypePool::base(Base b) { return add(Term{Tag::Base, b, 0, 0, 0, 0}); }
uint16_t TypePool::pat(uint16_t e) { return e ? add(Term{Tag::Pat, Base::Time, 0, 0, e, 0}) : 0; }
uint16_t TypePool::fn(uint16_t a, uint16_t r) {
    return (a && r) ? add(Term{Tag::Fn, Base::Time, 0, 0, a, r}) : 0;
}
uint16_t TypePool::list(uint16_t e) { return e ? add(Term{Tag::List, Base::Time, 0, 0, e, 0}) : 0; }
uint16_t TypePool::tuple(uint16_t a, uint16_t b) { return a && b ? add(Term{Tag::Tuple, Base::Time, 0, 0, a, b}) : 0; }

uint16_t TypePool::resolve(uint16_t t) const {
    while (t && t_[t].tag == Tag::Var && t_[t].a) t = t_[t].a;
    return t;
}

bool TypePool::occurs(uint16_t v, uint16_t t) const {
    t = resolve(t);
    if (t == v) return true;
    const Term& x = t_[t];
    switch (x.tag) {
        case Tag::Pat:
        case Tag::List: return occurs(v, x.a);
        case Tag::Fn:
        case Tag::Tuple: return occurs(v, x.a) || occurs(v, x.b);
        default: return false;
    }
}

bool TypePool::isGround(uint16_t t) const {
    t = resolve(t);
    const Term& x = t_[t];
    switch (x.tag) {
        case Tag::Var: return false;
        case Tag::Base: return true;
        case Tag::Pat:
        case Tag::List: return isGround(x.a);
        case Tag::Fn:
        case Tag::Tuple: return isGround(x.a) && isGround(x.b);
    }
    return false;
}

bool TypePool::unify(uint16_t a, uint16_t b, char* msg, int len) {
    a = resolve(a);
    b = resolve(b);
    if (a == b) return true;
    Term& x = t_[a];
    Term& y = t_[b];
    auto fail = [&]() {
        char sa[64], sb[64];
        print(a, sa, sizeof sa);
        print(b, sb, sizeof sb);
        snprintf(msg, len, "type mismatch: %s vs %s", sa, sb);
        return false;
    };
    if (x.tag == Tag::Var || y.tag == Tag::Var) {
        uint16_t v = x.tag == Tag::Var ? a : b;
        uint16_t o = v == a ? b : a;
        Term& vt = t_[v];
        Term& ot = t_[o];
        if (ot.tag == Tag::Var) {
            ot.cons |= vt.cons;
            vt.a = o;
            return true;
        }
        if (occurs(v, o)) {
            snprintf(msg, len, "recursive type");
            return false;
        }
        if (vt.cons) {
            if (ot.tag != Tag::Base || !baseSatisfies(ot.base, vt.cons)) {
                char so[64];
                print(o, so, sizeof so);
                if (ot.tag == Tag::Base && (vt.cons & kCFracLit) &&
                    baseSatisfies(ot.base, kCLit))
                    snprintf(msg, len, "%s can't hold a fractional number", so);
                else if (vt.cons & (kCLit | kCFracLit))
                    snprintf(msg, len, "%s can't hold a number", so);
                else
                    snprintf(msg, len, "%s is not %s", so,
                             (vt.cons & kCFrac) ? "Fractional" : "Num");
                return false;
            }
        }
        vt.a = o;
        return true;
    }
    if (x.tag != y.tag) return fail();
    switch (x.tag) {
        case Tag::Base: return x.base == y.base ? true : fail();
        case Tag::Pat:
        case Tag::List: return unify(x.a, y.a, msg, len);
        case Tag::Fn:
        case Tag::Tuple: return unify(x.a, y.a, msg, len) && unify(x.b, y.b, msg, len);
        default: return fail();
    }
}

// ---- signature parser -------------------------------------------------------
namespace {
struct SigParser {
    TypePool& pool;
    const char* p;
    char vars[8];
    uint16_t varTerm[8];
    uint8_t varCons[8];
    int nvars = 0;
    bool err = false;

    void ws() {
        while (*p == ' ') p++;
    }
    bool lit(const char* s) {
        ws();
        size_t n = strlen(s);
        if (strncmp(p, s, n) == 0) {
            p += n;
            return true;
        }
        return false;
    }
    int findVar(char c) {
        for (int i = 0; i < nvars; i++)
            if (vars[i] == c) return i;
        if (nvars >= 8) {
            err = true;
            return 0;
        }
        vars[nvars] = c;
        varTerm[nvars] = 0;
        varCons[nvars] = 0;
        return nvars++;
    }
    uint16_t varRef(char c) {
        int i = findVar(c);
        if (!varTerm[i]) varTerm[i] = pool.var(varCons[i]);
        return varTerm[i];
    }
    void constraints() {
        // optional "Num a, Fractional b =>" prefix
        const char* save = p;
        for (;;) {
            ws();
            uint8_t c = 0;
            if (lit("Num ")) c = kCNum;
            else if (lit("Fractional ")) c = kCFrac;
            else if (lit("Frac ")) c = kCFrac;
            else if (lit("Unionable ")) c = 0;
            else break;
            ws();
            int i = findVar(*p++);
            varCons[i] |= c;
            ws();
            if (*p == ',') {
                p++;
                continue;
            }
            if (lit("=>")) return;
            break;
        }
        p = save;
        nvars = 0;
    }
    uint16_t baseOrVar() {
        ws();
        static const struct { const char* n; Base b; } bases[] = {
            {"Time", Base::Time},     {"Double", Base::Double}, {"Int", Base::Int},
            {"Note", Base::Note},     {"String", Base::String}, {"Bool", Base::Bool},
            {"Control", Base::Control},
        };
        for (auto& b : bases) {
            size_t n = strlen(b.n);
            if (strncmp(p, b.n, n) == 0) {
                p += n;
                return pool.base(b.b);
            }
        }
        if (*p >= 'a' && *p <= 'z') return varRef(*p++);
        err = true;
        return 0;
    }
    uint16_t atom() {
        ws();
        if (lit("(")) {
            uint16_t t = type();
            if (lit(",")) t = pool.tuple(t, type());  // (a, b)
            if (!lit(")")) err = true;
            return t;
        }
        if (lit("[")) {
            uint16_t t = type();
            if (!lit("]")) err = true;
            return pool.list(t);
        }
        if (lit("Pat ")) return pool.pat(baseOrVar());
        return baseOrVar();
    }
    uint16_t type() {
        uint16_t a = atom();
        if (lit("->")) return pool.fn(a, type());
        return a;
    }
};
}  // namespace

uint16_t TypePool::instantiate(const char* sig) {
    SigParser sp{*this, sig, {}, {}, {}};
    sp.constraints();
    uint16_t t = sp.type();
    sp.ws();
    if (sp.err || *sp.p || !t) return 0;
    return t;
}

void TypePool::print(uint16_t t, char* buf, int len) const {
    t = resolve(t);
    const Term& x = t_[t];
    char a[64], b[64];
    switch (x.tag) {
        case Tag::Var: snprintf(buf, len, "t%u", t); break;
        case Tag::Base: snprintf(buf, len, "%s", baseName(x.base)); break;
        case Tag::Pat:
            print(x.a, a, sizeof a);
            snprintf(buf, len, "Pat %s", a);
            break;
        case Tag::List:
            print(x.a, a, sizeof a);
            snprintf(buf, len, "[%s]", a);
            break;
        case Tag::Tuple:
            print(x.a, a, sizeof a);
            print(x.b, b, sizeof b);
            snprintf(buf, len, "(%s, %s)", a, b);
            break;
        case Tag::Fn: {
            print(x.a, a, sizeof a);
            print(x.b, b, sizeof b);
            bool paren = t_[resolve(x.a)].tag == Tag::Fn;
            snprintf(buf, len, paren ? "(%s) -> %s" : "%s -> %s", a, b);
            break;
        }
    }
}

}  // namespace tc
