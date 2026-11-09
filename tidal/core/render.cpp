#include "render.h"

#include <stdio.h>
#include <string.h>

#include "library.h"

namespace tc {

int exprPrec(const Program& p, uint16_t idx) {
    const Node& n = p.at(idx);
    switch (n.kind) {
        case Kind::App: return (n.flags & kDollar) ? kPrecDollar : 10;
        case Kind::Op: return lib((int)n.a).prec;
        case Kind::Compose: return kPrecCompose;
        default: return 11;
    }
}

static Assoc exprAssoc(const Program& p, uint16_t idx) {
    const Node& n = p.at(idx);
    switch (n.kind) {
        case Kind::App: return (n.flags & kDollar) ? Assoc::Right : Assoc::Left;
        case Kind::Op: return lib((int)n.a).assoc;
        case Kind::Compose: return Assoc::Right;
        default: return Assoc::None;
    }
}

// Does child need parentheses as the left (side -1) or right (side +1)
// operand of an operator with precedence prec / associativity assoc?
static bool operandParens(const Program& p, uint16_t child, int prec, Assoc assoc, int side) {
    int cp = exprPrec(p, child);
    if (cp > prec) return false;
    if (cp < prec) return true;
    Assoc ca = exprAssoc(p, child);
    if (ca != assoc) return true;
    return side < 0 ? assoc != Assoc::Left : assoc != Assoc::Right;
}

void Renderer::put(char c) {
    if (n_ + 1 >= cap_) {
        overflow_ = true;
        return;
    }
    out_[n_++] = c;
    out_[n_] = 0;
}
void Renderer::puts(const char* s) {
    while (*s) put(*s++);
}
void Renderer::begin(uint16_t idx) {
    if (spans_) spans_[idx].start = (uint16_t)n_;
}
void Renderer::end(uint16_t idx) {
    if (spans_) spans_[idx].end = (uint16_t)n_;
}

void Renderer::putRational(int32_t num, uint32_t den, int dec) {
    if (den == 0) den = 1;
    int64_t a = num < 0 ? -(int64_t)num : num;
    // find a decimal count that represents the value exactly
    int64_t scale = 1;
    for (int k = 0; k < dec; k++) scale *= 10;
    while ((a * scale) % den != 0 && dec < 9) {
        dec++;
        scale *= 10;
    }
    int64_t scaled = a * scale / den;
    char buf[32];
    if (num < 0) put('-');
    snprintf(buf, sizeof buf, "%lld", (long long)(scaled / scale));
    puts(buf);
    if (dec > 0) {
        put('.');
        snprintf(buf, sizeof buf, "%0*lld", dec, (long long)(scaled % scale));
        puts(buf);
    }
}

void Renderer::node(const Program& p, uint16_t idx, int needPrec, int side) {
    (void)side;
    const Node& n = p.at(idx);
    begin(idx);
    switch (n.kind) {
        case Kind::Num:
            if ((int32_t)n.a < 0) {
                put('(');
                putRational((int32_t)n.a, n.b, nodeDecimals(n));
                put(')');
            } else {
                putRational((int32_t)n.a, n.b, nodeDecimals(n));
            }
            break;
        case Kind::Mini:
            put('"');
            if (n.first) mini(p, n.first);
            put('"');
            break;
        case Kind::Fn: puts(lib((int)n.a).name); break;
        case Kind::Hole: puts("??"); break;
        case Kind::App: {
            bool dollar = n.flags & kDollar;
            uint16_t c = n.first;
            int i = 0;
            int argc = p.childCount(idx);
            for (; c; c = p.at(c).next, i++) {
                if (i > 0) puts(dollar && i == argc - 1 ? " $ " : " ");
                bool last = dollar && i == argc - 1;
                bool paren;
                if (last) paren = false;
                else if (i == 0) paren = exprPrec(p, c) < 10;
                else paren = exprPrec(p, c) < 11;
                if (paren) put('(');
                node(p, c, 0, 0);
                if (paren) put(')');
            }
            break;
        }
        case Kind::Op:
        case Kind::Compose: {
            int prec = n.kind == Kind::Op ? lib((int)n.a).prec : kPrecCompose;
            Assoc assoc = n.kind == Kind::Op ? lib((int)n.a).assoc : Assoc::Right;
            uint16_t l = n.first, r = l ? p.at(l).next : 0;
            bool pl = operandParens(p, l, prec, assoc, -1);
            if (pl) put('(');
            node(p, l, 0, -1);
            if (pl) put(')');
            // ratios of two literals read better unspaced: (1/8), (135/60)
            bool tight = n.kind == Kind::Op && strcmp(lib((int)n.a).name, "/") == 0 &&
                         p.at(l).kind != Kind::App && p.at(r).kind == Kind::Num &&
                         (p.at(l).kind == Kind::Num || (p.at(l).kind == Kind::Op && !pl));
            if (!tight) put(' ');
            puts(n.kind == Kind::Op ? lib((int)n.a).name : ".");
            if (!tight) put(' ');
            bool pr = operandParens(p, r, prec, assoc, +1);
            if (pr) put('(');
            node(p, r, 0, +1);
            if (pr) put(')');
            break;
        }
        case Kind::SectionR: {
            const LibEntry& op = lib((int)n.a);
            put('(');
            puts(op.name);
            put(' ');
            bool pr = operandParens(p, n.first, op.prec, op.assoc, +1);
            if (pr) put('(');
            node(p, n.first, 0, +1);
            if (pr) put(')');
            put(')');
            break;
        }
        case Kind::SectionL: {
            const LibEntry& op = lib((int)n.a);
            put('(');
            bool pl = operandParens(p, n.first, op.prec, op.assoc, -1);
            if (pl) put('(');
            node(p, n.first, 0, -1);
            if (pl) put(')');
            put(' ');
            puts(op.name);
            put(')');
            break;
        }
        case Kind::List: {
            put('[');
            int i = 0;
            for (uint16_t c = n.first; c; c = p.at(c).next, i++) {
                if (i) puts(", ");
                node(p, c, 0, 0);
            }
            put(']');
            break;
        }
        default:
            if (isMiniKind(n.kind)) mini(p, idx);
            else puts("<?>");
            break;
    }
    (void)needPrec;
    end(idx);
}

// Modifier argument: a one-step MSeq renders as that step, otherwise [ ].
void Renderer::miniArg(const Program& p, uint16_t seq) {
    const Node& s = p.at(seq);
    if (s.kind == Kind::MSeq && s.first && !p.at(s.first).next && !(s.flags & kDots)) {
        begin(seq);
        mini(p, s.first);
        end(seq);
    } else if (s.kind == Kind::MSeq) {
        put('[');
        mini(p, seq);
        put(']');
    } else {
        mini(p, seq);
    }
}

static const char* kNoteNames[12] = {"c", "cs", "d", "ds", "e", "f", "fs", "g", "gs", "a", "as", "b"};

void Renderer::mini(const Program& p, uint16_t idx) {
    const Node& n = p.at(idx);
    begin(idx);
    auto layers = [&](const char* open, const char* sep, const char* close, bool skipLast) {
        puts(open);
        int i = 0;
        for (uint16_t c = n.first; c; c = p.at(c).next, i++) {
            if (skipLast && !p.at(c).next) break;
            if (i) puts(sep);
            mini(p, c);
        }
        puts(close);
    };
    switch (n.kind) {
        case Kind::MSeq: {
            int i = 0;
            for (uint16_t c = n.first; c; c = p.at(c).next, i++) {
                if (i) puts((n.flags & kDots) ? " . " : " ");
                mini(p, c);
            }
            break;
        }
        case Kind::MSub: layers("[", ", ", "]", false); break;
        case Kind::MChoice: layers("[", " | ", "]", false); break;
        case Kind::MAlt: layers("<", ", ", ">", false); break;
        case Kind::MPoly: {
            bool steps = n.flags & kHasSteps;
            layers("{", ", ", "}", steps);
            if (steps) {
                uint16_t last = n.first;
                while (p.at(last).next) last = p.at(last).next;
                put('%');
                mini(p, last);
            }
            break;
        }
        case Kind::MFast:
        case Kind::MSlow: {
            mini(p, n.first);
            put(n.kind == Kind::MFast ? '*' : '/');
            miniArg(p, p.at(n.first).next);
            break;
        }
        case Kind::MReplicate: {
            mini(p, n.first);
            if (n.flags & kBare) {
                for (uint32_t k = 1; k < n.a; k++) puts(" !");
            } else {
                char buf[16];
                snprintf(buf, sizeof buf, "!%u", n.a);
                puts(buf);
            }
            break;
        }
        case Kind::MWeight: {
            mini(p, n.first);
            if (n.flags & kUnderscore) {
                for (uint32_t k = 1; k < n.a; k++) puts(" _");
            } else {
                put('@');
                mini(p, p.at(n.first).next);
            }
            break;
        }
        case Kind::MDegrade: {
            mini(p, n.first);
            put('?');
            uint16_t a = p.at(n.first).next;
            if (a) mini(p, a);
            break;
        }
        case Kind::MEuclid: {
            mini(p, n.first);
            put('(');
            int i = 0;
            for (uint16_t c = p.at(n.first).next; c; c = p.at(c).next, i++) {
                if (i) put(',');
                mini(p, c);
            }
            put(')');
            break;
        }
        case Kind::MRange: {
            mini(p, n.first);
            puts(" .. ");
            mini(p, p.at(n.first).next);
            break;
        }
        case Kind::MRest: put('~'); break;
        case Kind::MWord: {
            puts(p.str((int)n.a));
            if (n.b) {
                char buf[16];
                snprintf(buf, sizeof buf, ":%u", n.b - 1);
                puts(buf);
            }
            break;
        }
        case Kind::MNum: putRational((int32_t)n.a, n.b, nodeDecimals(n)); break;
        case Kind::MNote: {
            if (n.b) {
                puts(p.str((int)n.b - 1));
            } else {
                int v = (int32_t)n.a;
                int oct = 5 + (v >= 0 ? v / 12 : -((11 - v) / 12));
                int pc = v - (oct - 5) * 12;
                puts(kNoteNames[pc]);
                if (oct != 5) {
                    char buf[8];
                    snprintf(buf, sizeof buf, "%d", oct);
                    puts(buf);
                }
            }
            break;
        }
        case Kind::MChord: puts(p.str((int)n.a)); break;
        case Kind::MBool:
            put((n.flags & kDigitStyle) ? (n.a ? '1' : '0') : (n.a ? 't' : 'f'));
            break;
        default: puts("<?>"); break;
    }
    end(idx);
}

int Renderer::expr(const Program& p, uint16_t root) {
    if (root) node(p, root, 0, 0);
    return n_;
}

int Renderer::block(const Program& p, int slot) {
    uint16_t b = p.block[slot];
    if (!b) return n_;
    begin(b);
    char buf[40];
    const Node& bn = p.at(b);
    switch (blockLaunch(bn)) {
        case Launch::JumpMod: snprintf(buf, sizeof buf, "jumpMod %d %u", slot + 1, (unsigned)bn.b); break;
        case Launch::JumpIn: snprintf(buf, sizeof buf, "jumpIn %d %u", slot + 1, (unsigned)bn.b); break;
        default: snprintf(buf, sizeof buf, "d%d", slot + 1); break;
    }
    puts(buf);
    uint16_t e = p.at(b).first;
    if (e) {
        puts(exprPrec(p, e) >= 11 ? " " : " $ ");
        node(p, e, 0, 0);
    }
    end(b);
    return n_;
}

int Renderer::file(const Program& p) {
    bool any = false;
    if (p.cpsExpr) {
        puts("setcps ");
        bool paren = exprPrec(p, p.cpsExpr) < 11;
        if (paren) put('(');
        node(p, p.cpsExpr, 0, 0);
        if (paren) put(')');
        put('\n');
        any = true;
    }
    for (int s = 0; s < kSlots; s++) {
        if (!p.block[s]) continue;
        if (any) put('\n');
        if (p.comment[s][0]) {
            puts(p.comment[s]);
            put('\n');
        }
        block(p, s);
        put('\n');
        any = true;
    }
    return n_;
}

}  // namespace tc
