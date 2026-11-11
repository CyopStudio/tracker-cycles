#include "parse.h"

#include <ctype.h>
#include <string.h>

#include "library.h"

namespace tc {

void errorLineCol(const char* text, int pos, int& line, int& col) {
    line = 1;
    col = 1;
    for (int i = 0; i < pos && text[i]; i++) {
        if (text[i] == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }
}

// Parse an unsigned decimal "12" / "0.25" at s[0..len). Returns false if it
// isn't a number or doesn't fit (spec 6.3).
static bool parseDecimal(const char* s, int len, bool neg, Rational& out, int& decimals) {
    int64_t num = 0, den = 1;
    decimals = 0;
    bool frac = false, any = false;
    for (int i = 0; i < len; i++) {
        char c = s[i];
        if (c == '.' && !frac) {
            frac = true;
            continue;
        }
        if (!isdigit((unsigned char)c)) return false;
        any = true;
        num = num * 10 + (c - '0');
        if (frac) {
            den *= 10;
            decimals++;
        }
        if (decimals > 9 || num > 0x7FFFFFFFLL) return false;
    }
    if (!any || (frac && decimals == 0)) return false;
    out = makeRational(neg ? -num : num, den);
    return true;
}

// ============================================================================
// Mini-notation
// ============================================================================
namespace {

struct MiniParser {
    Program& prog;
    const char* s;
    int len;
    int i = 0;
    int base;
    Error& err;
    int depth = 0;

    MiniParser(Program& p, const char* t, int l, int b, Error& e)
        : prog(p), s(t), len(l), base(b), err(e) {}

    bool ok() const { return !err.failed; }
    char peek(int o = 0) const { return i + o < len ? s[i + o] : 0; }
    void ws() {
        while (i < len && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
    }
    void fail(const char* m) { err.set(base + i, "mini-notation: %s", m); }
    uint16_t node(Kind k) {
        uint16_t n = prog.alloc(k);
        if (!n) fail("too many nodes");
        return n;
    }
    static bool wordStart(char c) { return isalpha((unsigned char)c); }
    bool wordChar(int at) const {
        char c = at < len ? s[at] : 0;
        if (isalnum((unsigned char)c) || c == '\'' || c == '_' || c == '#') return true;
        // '-' inside a word, as Tidal reads it: "606-kick", "hi-hat"
        if (c == '-' && at > 0 && at < len) {
            const char p = s[at - 1];
            if (isalnum((unsigned char)p) || p == '_' || p == '-') return true;
        }
        // '.' inside a word only when followed by an alphanumeric ("bd.x")
        if (c == '.' && at + 1 < len && isalnum((unsigned char)s[at + 1])) return true;
        return false;
    }

    // number atom: [-]digits[.digits]
    uint16_t number() {
        int st = i;
        bool neg = false;
        if (peek() == '-') {
            neg = true;
            i++;
        }
        int ds = i;
        while (isdigit((unsigned char)peek()) ||
               (peek() == '.' && isdigit((unsigned char)peek(1)))) i++;
        Rational r;
        int dec;
        if (!parseDecimal(s + ds, i - ds, neg, r, dec)) {
            i = st;
            fail("bad number");
            return 0;
        }
        uint16_t n = node(Kind::MNum);
        if (n) setNodeRational(prog.at(n), r, dec);
        return n;
    }

    uint16_t word() {
        int st = i;
        while (wordChar(i)) i++;
        int id = prog.intern(s + st, i - st);
        if (id < 0) {
            i = st;
            fail("word too long (max 15 chars) or too many words");
            return 0;
        }
        uint16_t n = node(Kind::MWord);
        if (!n) return 0;
        prog.at(n).a = (uint32_t)id;
        if (peek() == ':' && isdigit((unsigned char)peek(1))) {
            i++;
            int v = 0;
            while (isdigit((unsigned char)peek())) v = v * 10 + (s[i++] - '0');
            prog.at(n).b = (uint32_t)v + 1;
        }
        return n;
    }

    // [ ... ], < ... >, { ... }: layers separated by ',' (or '|' for [ ])
    uint16_t group(char open, char close) {
        i++;  // open
        if (++depth > kMaxDepth) {
            fail("nesting too deep");
            return 0;
        }
        Kind k = open == '[' ? Kind::MSub : open == '<' ? Kind::MAlt : Kind::MPoly;
        uint16_t g = node(k);
        if (!g) return 0;
        char sepSeen = 0;
        for (;;) {
            uint16_t seq = sequence();
            if (!ok()) return 0;
            prog.appendChild(g, seq);
            ws();
            char c = peek();
            if (c == close) {
                i++;
                break;
            }
            if (c == ',' || (c == '|' && open == '[')) {
                if (sepSeen && sepSeen != c) {
                    fail("can't mix ',' and '|' in one group");
                    return 0;
                }
                sepSeen = c;
                i++;
                continue;
            }
            fail(c ? "unexpected character" : "unclosed bracket");
            return 0;
        }
        if (sepSeen == '|') prog.at(g).kind = Kind::MChoice;
        depth--;
        if (k == Kind::MPoly && peek() == '%') {
            i++;
            uint16_t st = term();
            if (!ok()) return 0;
            prog.appendChild(g, st);
            prog.at(g).flags |= kHasSteps;
        }
        return g;
    }

    uint16_t term() {
        char c = peek();
        if (c == '~' ) {
            i++;
            return node(Kind::MRest);
        }
        if (c == '[') return group('[', ']');
        if (c == '<') return group('<', '>');
        if (c == '{') return group('{', '}');
        if (isdigit((unsigned char)c)) {
            // digits running into letters, or with a sample index, are a word
            // as in Tidal's s patterns: "808bd", "808:3"
            int j = i;
            while (j < len && isdigit((unsigned char)s[j])) j++;
            if (j < len && isalpha((unsigned char)s[j])) return word();
            // digits, then - _ ' and alphanumerics with a letter among them:
            // a sample name ("606-kick", "808_-_kick_5", from "808 - kick 5.wav";
            // 1.0.0 device pass). Without a letter ("1-2") they stay numbers, as
            // Tidal reads number patterns (in sound patterns Tidal also takes
            // "808_5" as a name; we can't tell the type while parsing)
            {
                int k = j;
                bool letter = false;
                while (k < len && (isalnum((unsigned char)s[k]) || s[k] == '_' || s[k] == '-' || s[k] == '\'')) {
                    letter |= isalpha((unsigned char)s[k]) != 0;
                    k++;
                }
                if (letter) return word();
            }
            if (j + 1 < len && s[j] == ':' && isdigit((unsigned char)s[j + 1])) return word();
            return number();
        }
        if (c == '-' && isdigit((unsigned char)peek(1))) return number();
        if (wordStart(c)) return word();
        fail(c ? "unexpected character" : "unexpected end");
        return 0;
    }

    // Argument of a modifier: a single term (patterned via < > or [ ]),
    // wrapped in an MSeq so every modifier arg has the same shape.
    uint16_t argSeq() {
        uint16_t t = term();
        if (!ok()) return 0;
        uint16_t sq = node(Kind::MSeq);
        if (!sq) return 0;
        prog.appendChild(sq, t);
        return sq;
    }

    uint16_t wrap(Kind k, uint16_t target) {
        uint16_t m = node(k);
        if (!m) return 0;
        prog.appendChild(m, target);
        return m;
    }

    uint16_t modifiers(uint16_t t) {
        for (;;) {
            if (!ok()) return 0;
            char c = peek();
            if (c == '*' || c == '/') {
                i++;
                uint16_t m = wrap(c == '*' ? Kind::MFast : Kind::MSlow, t);
                uint16_t a = argSeq();
                if (!ok()) return 0;
                prog.appendChild(m, a);
                t = m;
            } else if (c == '!' && isdigit((unsigned char)peek(1))) {
                i++;
                int v = 0;
                while (isdigit((unsigned char)peek())) v = v * 10 + (s[i++] - '0');
                if (v < 1 || v > kMaxSeqSteps) {
                    fail("bad replicate count");
                    return 0;
                }
                t = wrap(Kind::MReplicate, t);
                if (t) prog.at(t).a = (uint32_t)v;
            } else if (c == '@') {
                i++;
                uint16_t m = wrap(Kind::MWeight, t);
                uint16_t a = number();
                if (!ok()) return 0;
                prog.appendChild(m, a);
                t = m;
            } else if (c == '?') {
                i++;
                uint16_t m = wrap(Kind::MDegrade, t);
                if (isdigit((unsigned char)peek()) || peek() == '.') {
                    uint16_t a = number();
                    if (!ok()) return 0;
                    prog.appendChild(m, a);
                }
                t = m;
            } else if (c == '(') {
                i++;
                uint16_t m = wrap(Kind::MEuclid, t);
                for (int argn = 0;; argn++) {
                    uint16_t a = sequence();
                    if (!ok()) return 0;
                    prog.appendChild(m, a);
                    ws();
                    if (peek() == ',' && argn < 2) {
                        i++;
                        continue;
                    }
                    if (peek() == ')' && argn >= 1) {
                        i++;
                        break;
                    }
                    fail("euclid needs (k,n) or (k,n,r)");
                    return 0;
                }
                t = m;
            } else {
                return t;
            }
        }
    }

    // Steps until ',', '|', a closing bracket or the end. Handles '.'
    // grouping, '_' elongation, bare '!' repeats and '..' ranges.
    uint16_t sequence() {
        uint16_t sq = node(Kind::MSeq);
        if (!sq) return 0;
        uint16_t group = 0;     // current '.' group when dotted
        bool dotted = false;
        uint16_t last = 0;      // last step in the current (group) sequence
        int steps = 0;
        auto target = [&]() { return dotted ? group : sq; };
        for (;;) {
            ws();
            char c = peek();
            if (!c || c == ',' || c == '|' || c == ']' || c == '>' || c == '}' || c == ')') break;
            if (c == '.' && peek(1) == '.') {  // range
                i += 2;
                ws();
                if (!last || !isMiniAtom(prog.at(last).kind)) {
                    fail("'..' needs a value on each side");
                    return 0;
                }
                uint16_t hi = term();
                if (!ok()) return 0;
                uint16_t r = node(Kind::MRange);
                if (!r) return 0;
                prog.replace(last, r);
                prog.appendChild(r, last);
                prog.appendChild(r, hi);
                last = r;
                continue;
            }
            if (c == '.' && !isdigit((unsigned char)peek(1))) {  // group separator
                i++;
                if (!dotted) {
                    // move existing steps into the first group
                    dotted = true;
                    uint16_t g0 = node(Kind::MSeq);
                    if (!g0) return 0;
                    prog.at(g0).first = prog.at(sq).first;
                    prog.at(sq).first = 0;
                    prog.appendChild(sq, g0);
                    prog.at(sq).flags |= kDots;
                }
                group = node(Kind::MSeq);
                if (!group) return 0;
                prog.appendChild(sq, group);
                last = 0;
                continue;
            }
            if (c == '_' ) {
                i++;
                if (!last) {
                    fail("'_' needs a step before it");
                    return 0;
                }
                Node& ln = prog.at(last);
                if (ln.kind == Kind::MWeight && (ln.flags & kUnderscore)) {
                    ln.a++;
                } else {
                    uint16_t w = node(Kind::MWeight);
                    if (!w) return 0;
                    prog.replace(last, w);
                    prog.appendChild(w, last);
                    prog.at(w).flags |= kUnderscore;
                    prog.at(w).a = 2;
                    last = w;
                }
                continue;
            }
            if (c == '!') {
                i++;
                if (!last) {
                    fail("'!' needs a step before it");
                    return 0;
                }
                Node& ln = prog.at(last);
                if (ln.kind == Kind::MReplicate && (ln.flags & kBare)) {
                    ln.a++;
                } else {
                    uint16_t r = node(Kind::MReplicate);
                    if (!r) return 0;
                    prog.replace(last, r);
                    prog.appendChild(r, last);
                    prog.at(r).flags |= kBare;
                    prog.at(r).a = 2;
                    last = r;
                }
                continue;
            }
            uint16_t t = term();
            if (!ok()) return 0;
            t = modifiers(t);
            if (!ok()) return 0;
            if (++steps > kMaxSeqSteps) {
                fail("too many steps");
                return 0;
            }
            prog.appendChild(target(), t);
            last = t;
        }
        return sq;
    }

    uint16_t top() {
        uint16_t sq = sequence();
        if (!ok()) return 0;
        ws();
        char c = peek();
        if (c == ',' || c == '|') {
            // top-level layers: same tree as "[a, b]"
            uint16_t g = node(c == ',' ? Kind::MSub : Kind::MChoice);
            if (!g) return 0;
            prog.appendChild(g, sq);
            while (peek() == c) {
                i++;
                uint16_t more = sequence();
                if (!ok()) return 0;
                prog.appendChild(g, more);
                ws();
            }
            uint16_t root = node(Kind::MSeq);
            if (!root) return 0;
            prog.appendChild(root, g);
            sq = root;
        }
        ws();
        if (i < len) {
            fail("unexpected character");
            return 0;
        }
        return sq;
    }
};

// ============================================================================
// Expressions
// ============================================================================
enum class Tok : uint8_t { End, Ident, Num, Str, Op, LParen, RParen, LBrack, RBrack, Comma, Bad };

struct Token {
    Tok t;
    int pos, len;
    bool nl;  // a line break before it (kept as kBreak / kBreakOp)
};

static bool isSym(char c) { return c && strchr("!#$%&*+./<=>?@\\^|-~:", c) != nullptr; }

struct ExprParser {
    Program& prog;
    const char* s;
    int len;
    int base;
    Error& err;
    int i = 0;
    Token cur{};
    int depth = 0;

    ExprParser(Program& p, const char* t, int l, int b, Error& e)
        : prog(p), s(t), len(l), base(b), err(e) {
        next();
    }

    bool ok() const { return !err.failed; }
    void fail(int pos, const char* m) { err.set(base + pos, "%s", m); }
    uint16_t node(Kind k) {
        uint16_t n = prog.alloc(k);
        if (!n) fail(cur.pos, "too many nodes");
        return n;
    }

    Token lex() {
        // skip whitespace and -- comments
        bool nl = false;
        for (;;) {
            while (i < len && isspace((unsigned char)s[i])) nl |= s[i++] == '\n';
            if (i + 1 < len && s[i] == '-' && s[i + 1] == '-') {
                // "--" starts a comment only if not part of a longer operator
                int j = i;
                while (j < len && s[j] == '-') j++;
                if (j >= len || !isSym(s[j])) {
                    while (i < len && s[i] != '\n') i++;
                    continue;
                }
            }
            break;
        }
        Token t{Tok::End, i, 0, nl};
        if (i >= len) return t;
        char c = s[i];
        int st = i;
        if (isalpha((unsigned char)c) || c == '_') {
            while (i < len && (isalnum((unsigned char)s[i]) || s[i] == '_' || s[i] == '\'')) i++;
            t.t = Tok::Ident;
        } else if (isdigit((unsigned char)c)) {
            while (i < len && (isdigit((unsigned char)s[i]) ||
                               (s[i] == '.' && i + 1 < len && isdigit((unsigned char)s[i + 1]))))
                i++;
            t.t = Tok::Num;
        } else if (c == '"') {
            i++;
            while (i < len && s[i] != '"' && s[i] != '\n') i++;
            if (i >= len || s[i] != '"') {
                t.t = Tok::Bad;
                t.len = i - st;
                return t;
            }
            i++;
            t.t = Tok::Str;
        } else if (c == '(') {
            i++;
            t.t = Tok::LParen;
        } else if (c == ')') {
            i++;
            t.t = Tok::RParen;
        } else if (c == '[') {
            i++;
            t.t = Tok::LBrack;
        } else if (c == ']') {
            i++;
            t.t = Tok::RBrack;
        } else if (c == ',') {
            i++;
            t.t = Tok::Comma;
        } else if (isSym(c)) {
            while (i < len && isSym(s[i])) i++;
            t.t = Tok::Op;
        } else {
            i++;
            t.t = Tok::Bad;
        }
        t.len = i - st;
        return t;
    }
    void next() {
        cur = lex();
        if (cur.t == Tok::Bad)
            fail(cur.pos, s[cur.pos] == '"' ? "unterminated string" : "unexpected character");
        else if (cur.t == Tok::Op && s[cur.pos] == '\\')
            fail(cur.pos, "lambdas (\\x -> ...) are not supported");
    }
    Token peekTok() {
        int save = i;
        Token t = lex();
        i = save;
        return t;
    }
    bool isOp(const Token& t, const char* o) const {
        return t.t == Tok::Op && t.len == (int)strlen(o) && strncmp(s + t.pos, o, t.len) == 0;
    }

    // Fixity of an operator token. Returns false if unknown.
    bool fixity(const Token& t, int& prec, Assoc& assoc, int& libId) {
        libId = -1;
        if (isOp(t, "$")) {
            prec = kPrecDollar;
            assoc = Assoc::Right;
            return true;
        }
        if (isOp(t, ".")) {
            prec = kPrecCompose;
            assoc = Assoc::Right;
            return true;
        }
        int id = libFind(s + t.pos, t.len);
        if (id < 0 || !libIsOp(id)) return false;
        libId = id;
        prec = lib(id).prec;
        assoc = lib(id).assoc;
        return true;
    }

    bool startsAtom(const Token& t) const {
        return t.t == Tok::Ident || t.t == Tok::Num || t.t == Tok::Str || t.t == Tok::LParen ||
               t.t == Tok::LBrack || isOp(t, "??");
    }

    uint16_t number(bool neg) {
        Rational r;
        int dec;
        if (!parseDecimal(s + cur.pos, cur.len, neg, r, dec)) {
            fail(cur.pos, "number out of range (max 9 decimals, 32-bit)");
            return 0;
        }
        uint16_t n = node(Kind::Num);
        if (n) {
            setNodeRational(prog.at(n), r, dec);
            if (neg) prog.at(n).flags |= kNegParen;
        }
        next();
        return n;
    }

    uint16_t atom() {
        if (++depth > kMaxDepth) {
            fail(cur.pos, "nesting too deep");
            return 0;
        }
        uint16_t r = atomInner();
        depth--;
        return r;
    }

    uint16_t atomInner() {
        Token t = cur;
        switch (t.t) {
            case Tok::Ident: {
                int id = libFind(s + t.pos, t.len);
                if (id < 0 || libIsOp(id)) {
                    char name[32];
                    int n = t.len < 31 ? t.len : 31;
                    memcpy(name, s + t.pos, n);
                    name[n] = 0;
                    err.set(base + t.pos, "unknown or unsupported function '%s'", name);
                    return 0;
                }
                uint16_t f = node(Kind::Fn);
                if (f) prog.at(f).a = (uint32_t)id;
                next();
                return f;
            }
            case Tok::Num: return number(false);
            case Tok::Str: {
                uint16_t m = node(Kind::Mini);
                if (!m) return 0;
                uint16_t root = parseMini(prog, s + t.pos + 1, t.len - 2, err, base + t.pos + 1);
                if (!root) return 0;
                prog.appendChild(m, root);
                next();
                return m;
            }
            case Tok::LBrack: {
                next();
                uint16_t l = node(Kind::List);
                if (!l) return 0;
                if (cur.t == Tok::RBrack) {
                    next();
                    return l;
                }
                for (;;) {
                    uint16_t e = expr(0);
                    if (!ok()) return 0;
                    prog.appendChild(l, e);
                    if (cur.t == Tok::Comma) {
                        next();
                        continue;
                    }
                    if (cur.t == Tok::RBrack) {
                        next();
                        return l;
                    }
                    fail(cur.pos, "expected ',' or ']' in list");
                    return 0;
                }
            }
            case Tok::LParen: return paren();
            case Tok::Op:
                if (isOp(t, "??")) {  // editor hole, kept so drafts survive save/load
                    next();
                    return node(Kind::Hole);
                }
                fail(t.pos, "expected an expression");
                return 0;
            case Tok::Bad:
                fail(t.pos, s[t.pos] == '"' ? "unterminated string" : "unexpected character");
                return 0;
            default: fail(t.pos, "expected an expression"); return 0;
        }
    }

    uint16_t paren() {
        int open = cur.pos;
        next();
        if (cur.t == Tok::RParen) {
            fail(open, "() is not supported");
            return 0;
        }
        if (cur.t == Tok::Op) {
            // negative literal (-1) or right section (# speed 2)
            if (isOp(cur, "-")) {
                Token nt = peekTok();
                if (nt.t == Tok::Num) {
                    next();
                    uint16_t n = number(true);
                    if (!ok()) return 0;
                    if (cur.t != Tok::RParen) {
                        fail(cur.pos, "expected ')' after negative number");
                        return 0;
                    }
                    next();
                    return n;
                }
            }
            int prec, libId;
            Assoc assoc;
            if (!fixity(cur, prec, assoc, libId) || libId < 0) {
                fail(cur.pos, "unsupported operator section");
                return 0;
            }
            next();
            if (cur.t == Tok::RParen) {
                fail(cur.pos, "bare operators like (+) are not supported");
                return 0;
            }
            uint16_t rhs = expr(assoc == Assoc::Left ? prec + 1 : prec);
            if (!ok()) return 0;
            if (cur.t != Tok::RParen) {
                fail(cur.pos, "expected ')'");
                return 0;
            }
            next();
            uint16_t sec = node(Kind::SectionR);
            if (!sec) return 0;
            prog.at(sec).a = (uint32_t)libId;
            prog.appendChild(sec, rhs);
            return sec;
        }
        uint16_t e = expr(0);
        if (!ok()) return 0;
        if (cur.t == Tok::Op && peekTok().t == Tok::RParen) {
            // left section (0.25 ~>)
            int prec, libId;
            Assoc assoc;
            if (!fixity(cur, prec, assoc, libId) || libId < 0) {
                fail(cur.pos, "unsupported operator section");
                return 0;
            }
            next();
            next();
            uint16_t sec = node(Kind::SectionL);
            if (!sec) return 0;
            prog.at(sec).a = (uint32_t)libId;
            prog.appendChild(sec, e);
            return sec;
        }
        if (cur.t == Tok::Comma) {  // a pair (a, b), as within's (0, 0.5)
            next();
            uint16_t b = expr(0);
            if (!ok()) return 0;
            if (cur.t == Tok::Comma) {
                fail(cur.pos, "only pairs (a, b) are supported");
                return 0;
            }
            if (cur.t != Tok::RParen) {
                fail(cur.pos, "expected ')' after the pair");
                return 0;
            }
            next();
            uint16_t t = node(Kind::Tuple);
            if (!t) return 0;
            prog.appendChild(t, e);
            prog.appendChild(t, b);
            return t;
        }
        if (cur.t != Tok::RParen) {
            fail(cur.pos, "expected ')'");
            return 0;
        }
        next();
        return e;
    }

    // Function application: atom atom ...
    uint16_t app() {
        uint16_t head = atom();
        if (!ok()) return 0;
        if (!startsAtom(cur)) return head;
        uint16_t a;
        Node& hn = prog.at(head);
        if (hn.kind == Kind::App && !(hn.flags & kDollar)) {
            a = head;  // (f x) y  ==  f x y
        } else {
            a = node(Kind::App);
            if (!a) return 0;
            prog.appendChild(a, head);
        }
        int argc = prog.childCount(a) - 1;
        while (startsAtom(cur)) {
            const bool nl = cur.nl;
            uint16_t arg = atom();
            if (!ok()) return 0;
            if (++argc >= kMaxChildren) {
                fail(cur.pos, "too many arguments");
                return 0;
            }
            prog.appendChild(a, arg);
            if (nl) prog.at(arg).flags |= kBreak;
        }
        return a;
    }

    uint16_t makeDollar(uint16_t lhs, uint16_t rhs) {
        Node& ln = prog.at(lhs);
        uint16_t a;
        if (ln.kind == Kind::App && !(ln.flags & kDollar)) {
            a = lhs;
        } else {
            a = node(Kind::App);
            if (!a) return 0;
            prog.appendChild(a, lhs);
        }
        prog.appendChild(a, rhs);
        prog.at(a).flags |= kDollar;
        return a;
    }

    // Precedence climbing. ctxPrec/ctxAssoc: the operator whose right operand
    // we are parsing (to reject mixing same-precedence operators of
    // different associativity, as GHC does).
    uint16_t expr(int minPrec, int ctxPrec = -1, Assoc ctxAssoc = Assoc::None) {
        if (++depth > kMaxDepth) {
            fail(cur.pos, "nesting too deep");
            return 0;
        }
        uint16_t lhs = app();
        int lastPrec = -1;
        Assoc lastAssoc = Assoc::None;
        while (ok() && cur.t == Tok::Op) {
            if (peekTok().t == Tok::RParen) break;  // left section, handled by paren()
            int prec, libId;
            Assoc assoc;
            Token opTok = cur;
            if (!fixity(cur, prec, assoc, libId)) {
                char name[16];
                int n = cur.len < 15 ? cur.len : 15;
                memcpy(name, s + cur.pos, n);
                name[n] = 0;
                err.set(base + cur.pos, "unknown or unsupported operator '%s'", name);
                break;
            }
            if (prec < minPrec) break;
            if ((prec == lastPrec && assoc != lastAssoc) || (prec == ctxPrec && assoc != ctxAssoc)) {
                fail(opTok.pos, "can't mix operators of the same precedence with "
                                "different associativity; add parentheses");
                break;
            }
            next();
            uint16_t rhs = expr(assoc == Assoc::Left ? prec + 1 : prec, prec, assoc);
            if (!ok()) break;
            if (isOp(opTok, "$")) {
                lhs = makeDollar(lhs, rhs);
            } else if (isOp(opTok, ".")) {
                uint16_t c = node(Kind::Compose);
                if (!c) break;
                prog.appendChild(c, lhs);
                prog.appendChild(c, rhs);
                lhs = c;
            } else {
                uint16_t o = node(Kind::Op);
                if (!o) break;
                prog.at(o).a = (uint32_t)libId;
                prog.appendChild(o, lhs);
                prog.appendChild(o, rhs);
                lhs = o;
            }
            if (opTok.nl && lhs) prog.at(lhs).flags |= kBreakOp;  // a line break before the symbol
            lastPrec = prec;
            lastAssoc = assoc;
        }
        depth--;
        return ok() ? lhs : 0;
    }
};

}  // namespace

uint16_t parseMini(Program& prog, const char* text, int len, Error& err, int base) {
    MiniParser mp(prog, text, len, base, err);
    uint16_t r = mp.top();
    return err.failed ? 0 : r;
}

uint16_t parseExpr(Program& prog, const char* text, int len, Error& err, int base) {
    ExprParser ep(prog, text, len, base, err);
    uint16_t r = ep.expr(0);
    if (!err.failed && ep.cur.t != Tok::End) {
        err.set(base + ep.cur.pos,
                ep.cur.t == Tok::RParen ? "unmatched ')'" : "unexpected input after expression");
    }
    return err.failed ? 0 : r;
}

// ============================================================================
// Files
// ============================================================================
static bool lineBlank(const char* s, int a, int b) {
    for (int i = a; i < b; i++)
        if (!isspace((unsigned char)s[i])) return false;
    return true;
}
static bool lineIsComment(const char* s, int a, int b) {
    while (a < b && isspace((unsigned char)s[a])) a++;
    return a + 1 < b && s[a] == '-' && s[a + 1] == '-';
}
// Statement keyword at column 0 starts a new block even without a blank line.
static bool startsStatement(const char* s, int a, int b) {
    if (a >= b || isspace((unsigned char)s[a])) return false;
    if (s[a] == 'd' && a + 1 < b && isdigit((unsigned char)s[a + 1])) return true;
    return (b - a >= 7 && strncmp(s + a, "jumpMod", 7) == 0) ||
           (b - a >= 6 && strncmp(s + a, "jumpIn", 6) == 0) ||
           (b - a >= 6 && strncmp(s + a, "setcps", 6) == 0) ||
           (b - a >= 4 && strncmp(s + a, "hush", 4) == 0);
}

static bool parseStatement(Program& prog, const char* text, int a, int b, int commentA,
                           int commentB, Error& err) {
    int i = a;
    while (i < b && isspace((unsigned char)text[i])) i++;
    int st = i;
    while (i < b && (isalnum((unsigned char)text[i]) || text[i] == '_')) i++;
    int wl = i - st;
    const char* w = text + st;
    if (wl == 4 && strncmp(w, "hush", 4) == 0) return true;  // command, not stored
    if (wl == 6 && strncmp(w, "setcps", 6) == 0) {
        if (prog.cpsExpr) {
            err.set(st, "setcps appears more than once");
            return false;
        }
        uint16_t e = parseExpr(prog, text + i, b - i, err, i);
        if (!e) return false;
        prog.cpsExpr = e;
        return true;
    }
    // "d1 $ expr", "d1 expr", or with a transition "jumpMod 1 4 $ expr" /
    // "jumpIn 1 4 $ expr" (slot number, then cycles)
    Launch launch = Launch::Now;
    uint32_t cycles = 0;
    int n = -1;
    if ((wl == 7 && strncmp(w, "jumpMod", 7) == 0) || (wl == 6 && strncmp(w, "jumpIn", 6) == 0)) {
        launch = wl == 7 ? Launch::JumpMod : Launch::JumpIn;
        const char* name = wl == 7 ? "jumpMod" : "jumpIn";
        long v[2];
        for (int k = 0; k < 2; k++) {
            while (i < b && isspace((unsigned char)text[i])) i++;
            int ns = i;
            while (i < b && isdigit((unsigned char)text[i])) i++;
            if (ns == i || ns + 4 < i || (i < b && !isspace((unsigned char)text[i]) && text[i] != '$')) {
                err.set(ns, k == 0 ? "%s: expected the slot number (1..16), e.g. %s 1 4 $ ..."
                                   : "%s: expected a whole number of cycles, e.g. %s 1 4 $ ...",
                        name, name);
                return false;
            }
            v[k] = 0;
            for (int d = ns; d < i; d++) v[k] = v[k] * 10 + (text[d] - '0');  // at most 4 digits
        }
        n = (int)v[0];
        if (n < 1 || n > kSlots) {
            err.set(st, "%s: only slots 1..16 are supported", name);
            return false;
        }
        long lo = launch == Launch::JumpMod ? 1 : 0;
        if (v[1] < lo || v[1] > (long)kMaxLaunchCycles) {
            err.set(st, "%s: cycles must be %ld..%u", name, lo, (unsigned)kMaxLaunchCycles);
            return false;
        }
        cycles = (uint32_t)v[1];
    } else if (wl >= 2 && w[0] == 'd' && isdigit((unsigned char)w[1])) {
        n = 0;
        for (int k = 1; k < wl; k++) {
            if (!isdigit((unsigned char)w[k])) {
                n = -1;
                break;
            }
            n = n * 10 + (w[k] - '0');
        }
        if (n < 1 || n > kSlots) {
            err.set(st, "only d1..d16 are supported");
            return false;
        }
    }
    if (n > 0) {
        int slot = n - 1;
        if (prog.blockCount() >= kMaxBlocks) {
            err.set(st, "too many blocks (max %d)", kMaxBlocks);
            return false;
        }
        int j = i;
        while (j < b && isspace((unsigned char)text[j])) j++;
        if (j < b && text[j] == '$' && (j + 1 >= b || !isSym(text[j + 1]))) j++;
        uint16_t blk = prog.alloc(Kind::Block);
        if (!blk) {
            err.set(st, "too many nodes");
            return false;
        }
        prog.at(blk).a = (uint32_t)slot;
        setBlockLaunch(prog.at(blk), launch, cycles);
        int before = prog.liveNodes();
        uint16_t e = parseExpr(prog, text + j, b - j, err, j);
        if (!e) return false;
        if (prog.liveNodes() - before > kMaxBlockNodes) {
            err.set(st, "d%d is too large (max %d nodes)", n, kMaxBlockNodes);
            return false;
        }
        prog.appendChild(blk, e);
        prog.block[slot] = blk;  // several blocks for one dN: the last is selected
        const int idx = prog.blockCount();
        prog.insertBlock(idx, blk);
        // comment lines directly above the block
        int out = 0;
        char* c = prog.blockComment(idx);
        for (int k = commentA; k < commentB && out < kMaxCommentLen; k++) c[out++] = text[k];
        while (out > 0 && (c[out - 1] == '\n' || c[out - 1] == '\r')) out--;
        c[out] = 0;
        return true;
    }
    err.set(st, "unsupported statement (expected d1..d16, jumpMod, jumpIn, setcps or hush)");
    return false;
}

bool parseFile(Program& prog, const char* text, int len, Error& err) {
    prog.clear();
    int i = 0;
    int stmtA = -1, stmtB = -1;          // current statement range
    int comA = -1, comB = -1;            // comment lines above it
    auto flush = [&]() -> bool {
        bool r = true;
        if (stmtA >= 0) r = parseStatement(prog, text, stmtA, stmtB, comA < 0 ? 0 : comA,
                                           comA < 0 ? 0 : comB, err);
        stmtA = stmtB = comA = comB = -1;
        return r;
    };
    while (i < len) {
        int a = i;
        while (i < len && text[i] != '\n') i++;
        int b = i;
        if (i < len) i++;  // newline
        if (lineBlank(text, a, b)) {
            if (!flush()) return false;
            continue;
        }
        if (lineIsComment(text, a, b) && stmtA < 0) {
            if (comA < 0) comA = a;
            comB = i;
            continue;
        }
        if (startsStatement(text, a, b) && stmtA >= 0) {
            if (!flush()) return false;
        }
        if (stmtA < 0) stmtA = a;
        stmtB = b;
    }
    return flush();
}

}  // namespace tc
