// Core tests: parse/render round trips, fixity, type checking, refinement.
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <math.h>
#include <stdlib.h>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

#include "check.h"
#include "engine.h"
#include "golden.h"
#include "library.h"
#include "parse.h"
#include "render.h"
#include "types.h"

using namespace tc;

static int gFail = 0, gPass = 0;
static Program gProg;
static Program gProg2;

#define EXPECT(cond, ...)                          \
    do {                                           \
        if (cond) {                                \
            gPass++;                               \
        } else {                                   \
            gFail++;                               \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                   \
            printf("\n");                          \
        }                                          \
    } while (0)

static bool render(Program& p, uint16_t root, char* out, int cap) {
    Renderer r(out, cap);
    out[0] = 0;
    r.expr(p, root);
    return !r.overflow();
}

// Structural equality of two subtrees (kind, flags, payload, children).
static bool sameTree(const Program& a, uint16_t x, const Program& b, uint16_t y) {
    if (!x || !y) return x == y;
    const Node& m = a.at(x);
    const Node& n = b.at(y);
    if (m.kind != n.kind || m.flags != n.flags) return false;
    bool strA = m.kind == Kind::MWord || m.kind == Kind::MChord;
    if (strA) {
        if (strcmp(a.str((int)m.a), b.str((int)n.a)) != 0 || m.b != n.b) return false;
    } else if (m.a != n.a || m.b != n.b) {
        return false;
    }
    uint16_t cx = m.first, cy = n.first;
    while (cx && cy) {
        if (!sameTree(a, cx, b, cy)) return false;
        cx = a.at(cx).next;
        cy = b.at(cy).next;
    }
    return !cx && !cy;
}

// parse(in) renders as `expect` (in if null), and the render parses back
// to the same tree.
static void roundTrip(const char* in, const char* expect = nullptr) {
    if (!expect) expect = in;
    gProg.clear();
    Error e;
    uint16_t r = parseExpr(gProg, in, (int)strlen(in), e);
    if (!r) {
        EXPECT(false, "parse '%s': %s (pos %d)", in, e.msg, e.pos);
        return;
    }
    char out[1024];
    render(gProg, r, out, sizeof out);
    EXPECT(strcmp(out, expect) == 0, "render '%s'\n   got '%s'\n  want '%s'", in, out, expect);
    gProg2.clear();
    Error e2;
    uint16_t r2 = parseExpr(gProg2, out, (int)strlen(out), e2);
    EXPECT(r2 && sameTree(gProg, r, gProg2, r2), "reparse of '%s' differs (%s)", out, e2.msg);
}

static void parseFails(const char* in, const char* msgPart) {
    gProg.clear();
    Error e;
    uint16_t r = parseExpr(gProg, in, (int)strlen(in), e);
    EXPECT(!r && e.failed && strstr(e.msg, msgPart), "expected parse error '%s' for '%s', got '%s'",
           msgPart, in, r ? "(success)" : e.msg);
}

// Check `code` as block d1. If errPart is set, expect an error containing it.
static void typeCheck(const char* code, const char* errPart = nullptr) {
    char file[512];
    snprintf(file, sizeof file, "d1 $ %s\n", code);
    gProg.clear();
    Error e;
    if (!parseFile(gProg, file, (int)strlen(file), e)) {
        EXPECT(false, "parse '%s': %s", code, e.msg);
        return;
    }
    Error ce;
    bool ok = checkBlock(gProg, 0, ce);
    if (errPart)
        EXPECT(!ok && strstr(ce.msg, errPart), "expected type error '%s' for '%s', got '%s'",
               errPart, code, ok ? "(ok)" : ce.msg);
    else
        EXPECT(ok, "type error for '%s': %s", code, ce.msg);
}

// Rational fast paths (qtime.h) against a straightforward 128-bit reference:
// exact when the result fits 32 bits, otherwise flagged as overflow (never a
// silently wrong value).
static bool refQ(__int128 n, __int128 d, Q& out) {
    if (d == 0) return false;
    if (d < 0) n = -n, d = -d;
    __int128 a = n < 0 ? -n : n, b = d;
    while (b) {
        __int128 t = a % b;
        a = b;
        b = t;
    }
    if (a > 1) n /= a, d /= a;
    if (n > INT32_MAX || n < INT32_MIN || d > INT32_MAX) return false;
    out = Q{(int32_t)n, (int32_t)d};
    return true;
}
static void testRationals() {
    uint64_t seed = 12345;
    auto rnd = [&]() {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        return (uint32_t)(seed >> 33);
    };
    const int32_t edge[] = {0, 1, -1, 2, 3, 4, 7, 12, 16, 48, 65536, 1 << 30, INT32_MAX, INT32_MIN, -INT32_MAX};
    auto pick = [&](bool den) -> int32_t {
        uint32_t r = rnd() % 4;
        int32_t v;
        if (r == 0) v = edge[rnd() % (sizeof edge / sizeof edge[0])];
        else if (r == 1) v = (int32_t)(rnd() % 33) - 16;
        else if (r == 2) v = (int32_t)(rnd() % 2001) - 1000;
        else v = (int32_t)rnd();
        if (den) {
            if (v == INT32_MIN) v = INT32_MAX;
            v = v < 0 ? -v : v;
            if (v == 0) v = 1;
        }
        return v;
    };
    int bad = 0, exact = 0;
    for (int i = 0; i < 200000 && bad < 5; i++) {
        Q a, b;
        if (!refQ(pick(false), pick(true), a) || !refQ(pick(false), pick(true), b)) continue;
        for (int op = 0; op < 5; op++) {
            Q want{0, 1};
            bool fits;
            switch (op) {
                case 0: fits = refQ((__int128)a.n * b.d + (__int128)b.n * a.d, (__int128)a.d * b.d, want); break;
                case 1: fits = refQ((__int128)a.n * b.d - (__int128)b.n * a.d, (__int128)a.d * b.d, want); break;
                case 2: fits = refQ((__int128)a.n * b.n, (__int128)a.d * b.d, want); break;
                case 3: fits = refQ((__int128)a.n * b.d, (__int128)a.d * b.n, want); break;
                default: fits = true; break;
            }
            qOverflowFlag() = false;
            Q got = op == 0 ? a + b : op == 1 ? a - b : op == 2 ? a * b : op == 3 ? a / b : Q{0, 1};
            bool over = qOverflow();
            if (op < 4 && got.d > kQLazyMax && (qnorm(got).n != got.n || qnorm(got).d != got.d))
                bad++, printf("FAIL q op %d: %d/%d unreduced over the lazy limit\n", op, got.n, got.d);
            got = qnorm(got);
            if (op == 4) {
                __int128 fl = a.n >= 0 ? a.n / a.d : -((-(__int128)a.n + a.d - 1) / a.d);
                if (qfloor(a) != (int64_t)fl) bad++, printf("FAIL qfloor %d/%d\n", a.n, a.d);
                continue;
            }
            if (fits && !over && got == want) exact++;
            else if (!fits && over) exact++;
            else if (fits && over) {
                // allowed only when an intermediate overflows 64 bits
                __int128 x = (__int128)a.n * b.d, y = (__int128)b.n * a.d;
                __int128 s = op == 0 ? x + y : x - y;
                if (op > 1 || (s <= INT64_MAX && s >= INT64_MIN)) {
                    bad++;
                    printf("FAIL q op %d: %d/%d, %d/%d flagged overflow, want %d/%d\n", op, a.n, a.d, b.n, b.d, want.n,
                           want.d);
                }
            } else {
                bad++;
                printf("FAIL q op %d: %d/%d, %d/%d -> %d/%d (over %d), want %d/%d (fits %d)\n", op, a.n, a.d, b.n,
                       b.d, got.n, got.d, over, want.n, want.d, fits);
            }
        }
    }
    qOverflowFlag() = false;
    EXPECT(bad == 0, "rationals: %d mismatches", bad);
    EXPECT(exact > 500000, "rationals: only %d checks", exact);
}

static void testRoundTrips() {
    // expressions
    roundTrip("s \"bd*2 [sd cp]\"");
    roundTrip("fast 2 $ s \"bd sd\"");
    roundTrip("fast 2 (s \"bd sd\")");
    roundTrip("every 4 (fast 2) $ s \"bd sd\" # n \"1 2\"");
    roundTrip("jux rev $ s \"bd*2\" # speed 2");
    roundTrip("s \"bd\" # gain 0.8 # pan sine");
    roundTrip("n (\"0 2\" + \"<12 7>\") # s \"superpiano\"");
    roundTrip("(0.25 ~>) $ s \"bd\"");
    roundTrip("off 0.25 (# crush 4) $ s \"bd\"");
    roundTrip("stack [s \"bd*2\", s \"hh*3\"]");
    roundTrip("(rev . fast 2) $ s \"bd sd\"");
    roundTrip("every 3 (rev . fast 2) $ s \"bd\"");
    roundTrip("slow (1/8) $ s \"bd\"");
    roundTrip("n (-12) # s \"x\"");
    roundTrip("s \"bd\" # n 1 + n 2");
    roundTrip("0.25 <~ s \"bd\" # n 1");
    roundTrip("sometimesBy 0.3 (# speed \"2\") $ s \"bd*8\"");
    roundTrip("whenmod 8 6 rev $ n \"0 .. 7\" # s \"arpy\"");
    // normalisation: redundant parens dropped, spacing canonical
    roundTrip("(fast 2) $ s \"bd\"", "fast 2 $ s \"bd\"");
    roundTrip("fast  2  $s \"bd   sd\"", "fast 2 $ s \"bd sd\"");
    roundTrip("(s \"bd\" # n 1) # gain 1", "s \"bd\" # n 1 # gain 1");
    roundTrip("s \"bd\" # (n 1 # gain 1)");  // right-nested needs parens (infixl)
    roundTrip("fast 2 $ rev $ s \"bd\"");
    // mini-notation
    roundTrip("s \"bd? sd*<2 3> [hh hh, cp] <a b>!2\"");
    roundTrip("s \"bd(3,8) sd(<3 5>,8,2)\"");
    roundTrip("s \"bd _ _ ~ sd _\"");
    roundTrip("s \"bd ! ! sd\"");
    roundTrip("s \"bd@3 sd@1.5\"");
    roundTrip("s \"{bd sd hh, cp arpy}%4\"");
    roundTrip("s \"[bd | sd | hh]\"");
    roundTrip("s \"bd sd . hh hh hh\"");
    roundTrip("s \"bd:3 sd:12\"");
    roundTrip("s \"bd*2!2\"");
    roundTrip("s \"bd(3,8)?\"");
    roundTrip("s \"bd?0.8\"");
    roundTrip("n \"c e g\"");
    roundTrip("n \"c'maj e'min9'i2'o\"");
    roundTrip("n \"0 .. 7\"");
    roundTrip("n \"c .. e\"");
    roundTrip("n \"-1 0.5 -0.25\"");
    roundTrip("s \"bd, hh*2\"", "s \"[bd, hh*2]\"");
    roundTrip("s \"bd/2 sd*[2 3]\"");
    // editor holes survive save/load
    roundTrip("fast ?? $ s ??");
    roundTrip("s \"bd\" # ??");
}

static void testParseErrors() {
    parseFails("fast 2 $ foo \"bd\"", "unknown or unsupported function 'foo'");
    parseFails("rev . fast 2 # speed 2", "can't mix");
    parseFails("s \"bd\" # speed 2 . rev", "can't mix");
    parseFails("\\x -> x", "lambdas");
    parseFails("s \"bd", "unterminated string");
    parseFails("s \"bd [sd\"", "unclosed bracket");
    parseFails("s \"bd(3)\"", "euclid");
    parseFails("(fast 2", "expected ')'");
    parseFails("(1, 2)", "tuples");
    parseFails("s \"averyveryverylongsamplename\"", "too long");
    parseFails("n 0.1234567891", "out of range");
}

static void testFiles() {
    const char* f =
        "setcps (135/60/4)\n"
        "\n"
        "-- kick\n"
        "d1 $ s \"bd*4\"\n"
        "\n"
        "d2 $ every 2 (fast 2)\n"
        "   $ s \"hh*8\"\n"
        "d3 silence\n"
        "\n"
        "hush\n"
        "\n"
        "d9 $ n \"c e g\"\n";
    Error e;
    bool ok = parseFile(gProg, f, (int)strlen(f), e);
    EXPECT(ok, "file parse: %s (pos %d)", e.msg, e.pos);
    if (!ok) return;
    EXPECT(gProg.block[0] && gProg.block[1] && gProg.block[2] && gProg.block[8],
           "blocks d1 d2 d3 d9 present");
    EXPECT(strcmp(gProg.comment[0], "-- kick") == 0, "comment kept: '%s'", gProg.comment[0]);
    char out[1024];
    Renderer r(out, sizeof out);
    out[0] = 0;
    r.file(gProg);
    const char* want =
        "setcps (135/60/4)\n"
        "\n"
        "-- kick\n"
        "d1 $ s \"bd*4\"\n"
        "\n"
        "d2 $ every 2 (fast 2) $ s \"hh*8\"\n"
        "\n"
        "d3 silence\n"
        "\n"
        "d9 $ n \"c e g\"\n";
    EXPECT(strcmp(out, want) == 0, "file render:\n%s---\nwant:\n%s", out, want);
    Error ce;
    EXPECT(checkProgram(gProg, ce), "file type check: %s", ce.msg);

    const char* bad = "d1 $ s \"bd\"\nd1 $ s \"sd\"\n";
    Error e2;
    EXPECT(!parseFile(gProg, bad, (int)strlen(bad), e2) && strstr(e2.msg, "more than once"),
           "duplicate block rejected: %s", e2.msg);
    // launch modes (spec 2.1)
    const char* jf = "jumpMod 1 4 $ s \"bd sd\"\n\njumpIn 9 0 $ n \"c e\"\n\nd2 $ s \"hh\"\n";
    Error je;
    bool jok = parseFile(gProg, jf, (int)strlen(jf), je);
    EXPECT(jok, "jump file parse: %s", je.msg);
    if (jok) {
        EXPECT(blockLaunch(gProg.at(gProg.block[0])) == Launch::JumpMod && gProg.at(gProg.block[0]).b == 4,
               "jumpMod 1 4 stored");
        EXPECT(blockLaunch(gProg.at(gProg.block[8])) == Launch::JumpIn && gProg.at(gProg.block[8]).b == 0,
               "jumpIn 9 0 stored");
        EXPECT(blockLaunch(gProg.at(gProg.block[1])) == Launch::Now, "d2 is Now");
        char jo[512];
        Renderer jr(jo, sizeof jo);
        jo[0] = 0;
        jr.file(gProg);
        const char* jwant = "jumpMod 1 4 $ s \"bd sd\"\n\nd2 $ s \"hh\"\n\njumpIn 9 0 $ n \"c e\"\n";
        EXPECT(strcmp(jo, jwant) == 0, "jump render:\n%s---\nwant:\n%s", jo, jwant);
    }
    struct { const char* text; const char* msg; } jbad[] = {
        {"jumpMod 1 0 $ s \"bd\"\n", "cycles must be 1..256"},
        {"jumpIn 1 300 $ s \"bd\"\n", "cycles must be 0..256"},
        {"jumpMod 17 4 $ s \"bd\"\n", "slots 1..16"},
        {"jumpIn 1 $ s \"bd\"\n", "whole number of cycles"},
        {"jumpMod 1 4.5 $ s \"bd\"\n", "whole number of cycles"},
        {"jumpMod x 4 $ s \"bd\"\n", "slot number"},
        {"jumpMod 1 4 $ s \"bd\"\nd1 $ s \"sd\"\n", "more than once"},
    };
    for (auto& jb : jbad) {
        Error x;
        EXPECT(!parseFile(gProg, jb.text, (int)strlen(jb.text), x) && strstr(x.msg, jb.msg),
               "'%s' rejected with '%s' (got '%s')", jb.text, jb.msg, x.msg);
    }

    const char* bad2 = "d17 $ s \"bd\"\n";
    Error e3;
    EXPECT(!parseFile(gProg, bad2, (int)strlen(bad2), e3) && strstr(e3.msg, "d1..d16"),
           "d17 rejected: %s", e3.msg);
}

static void testTypes() {
    typeCheck("s \"bd*2 sd\"");
    typeCheck("fast 2 $ s \"bd sd\"");
    typeCheck("fast \"<1 2>\" $ s \"bd sd\"");
    typeCheck("every 4 (fast 2) $ s \"bd sd\" # n \"1 2\"");
    typeCheck("jux rev $ s \"bd*2\" # speed 2");
    typeCheck("s \"bd\" # gain 0.8 # pan sine");
    typeCheck("n (\"0 2\" + \"<12 7>\") # s \"superpiano\"");
    typeCheck("(0.25 ~>) $ s \"bd\"");
    typeCheck("off 0.25 (# crush 4) $ s \"bd\"");
    typeCheck("stack [s \"bd*2\", s \"hh*3\" # gain 1.2]");
    typeCheck("every 3 (rev . fast 2) $ s \"bd\"");
    typeCheck("n \"c e g\" # s \"superpiano\"");
    typeCheck("n \"c'maj e'min\" # s \"superpiano\"");
    typeCheck("struct \"t f t t\" $ s \"bd\"");
    typeCheck("struct \"1 0 1 1\" $ s \"bd\"");
    typeCheck("n (segment 4 $ range 1 5 saw) # s \"drum\"");
    typeCheck("s \"bd*8\" # cut 1 # orbit 0");
    typeCheck("n (\"0 .. 7\" + 12) # s \"arpy\"");
    typeCheck("s \"bd\" # n 1 |+ n 2");
    typeCheck("sometimesBy 0.3 (# speed 2) $ s \"bd*8\"");
    typeCheck("whenmod 8 6 rev $ s \"bd sd\"");
    typeCheck("degradeBy 0.9 $ s \"hh*16\"");
    typeCheck("loopAt 2 $ chop 4 $ s \"break\"");
    typeCheck("loopAt \"<1 2>\" $ s \"break\"");
    typeCheck("splice 8 \"0 2 4 <6 7>\" $ s \"break\"");
    typeCheck("splice 4 \"0 1.5\" $ s \"break\"", "fractional");
    typeCheck("s \"808 909\"");  // digits in a String pattern become words
    typeCheck("s \"808bd 808:3 808bd:2\"");  // digits + letters / :index are words
    typeCheck("n \"808bd\"", "not a note name");  // Tidal splits it into 808 b d
    // signatures checked against GHCi :t (golden/types.txt)
    typeCheck("whenmod \"<8 4>\" 1.5 rev $ s \"bd sd\"");  // Pat Time, not Pat Int
    typeCheck("n (scale \"major\" \"0 .. 7\")");
    typeCheck("n (scale \"major\" \"0.5\")", "fractional");  // steps are Pat Int
    // errors
    typeCheck("fast 2", "ControlPattern");
    typeCheck("fast \"bd\" $ s \"bd\"", "not a Time");
    typeCheck("s \"bd\" # gain \"x\"", "not a Double");
    typeCheck("n \"c q\" # s \"x\"", "not a note name");
    typeCheck("cut 1.5", "fractional");
    typeCheck("s \"bd\" # speed 2 # s 1", "can't hold a number");
    typeCheck("rev 1 2", "takes only");
    typeCheck("struct \"t x\" $ s \"bd\"", "not a Bool");
    typeCheck("s \"bd .. sd\"", "numbers or notes");
    typeCheck("s \"bd(3,8.5)\"", "fractional");
}

static void testRefinement() {
    const char* f = "d1 $ n \"c cs5 ef3\" # s \"c\"\n";
    Error e;
    parseFile(gProg, f, (int)strlen(f), e);
    Error ce;
    EXPECT(checkBlock(gProg, 0, ce), "check: %s", ce.msg);
    // find MNote atoms and their values
    int notes[3], nn = 0, words = 0;
    for (int i = 1; i < kMaxNodes; i++) {
        const Node& n = gProg.at((uint16_t)i);
        if (n.kind == Kind::MNote && nn < 3) notes[nn++] = (int32_t)n.a;
        if (n.kind == Kind::MWord) words++;
    }
    EXPECT(nn == 3, "3 notes refined (got %d)", nn);
    if (nn == 3) {
        // parse order isn't guaranteed by node index; check the set
        int sum = notes[0] + notes[1] + notes[2];
        EXPECT(sum == 0 + 1 + (3 - 24), "note values c=0 cs5=1 ef3=-21 (sum %d)", sum);
    }
    EXPECT(words == 1, "'c' in s stays a word (words=%d)", words);
    char out[256];
    Renderer r(out, sizeof out);
    out[0] = 0;
    r.block(gProg, 0);
    EXPECT(strcmp(out, "d1 $ n \"c cs5 ef3\" # s \"c\"") == 0, "spelling kept: %s", out);
}

static void testTypesPool() {
    TypePool tp;
    tp.clear();
    uint16_t t = tp.instantiate("Pat Int -> (Pat a -> Pat a) -> Pat a -> Pat a");
    char buf[128];
    tp.print(t, buf, sizeof buf);
    EXPECT(strstr(buf, "Pat Int -> (Pat t") == buf, "print sig: %s", buf);
    EXPECT(tp.instantiate("Num a => Pat a -> Pat a") != 0, "constraint sig parses");
    for (int i = 0; i < libCount(); i++) {
        tp.clear();
        EXPECT(tp.instantiate(lib(i).sig) != 0, "library sig of '%s' parses", lib(i).name);
    }
}

static void testNodeSize() {
    EXPECT(sizeof(Node) == 16, "node is 16 bytes");
    gProg.clear();
    int n = 0;
    while (gProg.alloc(Kind::Num)) n++;
    EXPECT(n == kMaxNodes - 1, "pool holds %d nodes (got %d)", kMaxNodes - 1, n);
}

// ---- engine vs golden events ----------------------------------------------------
static Event gEvents[4096];

static bool sameRat(const golden::Rat& g, Q q) { return g.num == q.n && g.den == q.d; }

static bool sameValue(const Program& p, const golden::Value& g, const Ctl& c) {
    if (g.key != controlDef(c.key).name || g.tag != c.tag) return false;
    if (g.tag == 's') {
        char buf[64];
        formatCtl(p, c, buf, sizeof buf);
        return g.text == buf;
    }
    // float32 on our side (what OSC sends), double in Tidal
    double want = atof(g.text.c_str()), got = c.f;
    return fabs(want - got) <= 1e-6 * (fabs(want) > 1 ? fabs(want) : 1);
}

static bool sameEvent(const Program& p, const golden::Event& g, const Event& e) {
    if (g.hasWhole != e.hasWhole) return false;
    if (g.hasWhole && (!sameRat(g.ws, e.whole.s) || !sameRat(g.we, e.whole.e))) return false;
    if (!sameRat(g.ps, e.part.s) || !sameRat(g.pe, e.part.e)) return false;
    if (!e.isMap || (int)g.values.size() != e.nctl) return false;
    for (const auto& gv : g.values) {
        bool found = false;
        for (int i = 0; i < e.nctl && !found; i++) found = sameValue(p, gv, e.ctl[i]);
        if (!found) return false;
    }
    return true;
}

static std::string eventText(const Program& p, const Event& e) {
    char buf[512];
    int n = 0;
    if (e.hasWhole) n += snprintf(buf + n, sizeof buf - n, "%d/%d %d/%d ", e.whole.s.n, e.whole.s.d, e.whole.e.n, e.whole.e.d);
    else n += snprintf(buf + n, sizeof buf - n, "~ ~ ");
    n += snprintf(buf + n, sizeof buf - n, "%d/%d %d/%d", e.part.s.n, e.part.s.d, e.part.e.n, e.part.e.d);
    for (int i = 0; i < e.nctl && n < (int)sizeof buf - 40; i++) {
        char v[64];
        formatCtl(p, e.ctl[i], v, sizeof v);
        n += snprintf(buf + n, sizeof buf - n, " %s=%c:%s", controlDef(e.ctl[i].key).name, e.ctl[i].tag, v);
    }
    return buf;
}

// Engine events for case `c` vs the captured ones. Empty string = match.
static std::string engineDiff(const golden::Case& c, const golden::Case& x, const Program& p, uint16_t expr) {
    Error e;
    int n = queryPattern(p, expr, Arc{Q{0, 1}, Q{c.cycles, 1}}, gEvents, 4096, e);
    if (n < 0) return std::string("engine error: ") + e.msg;
    std::vector<bool> used(n, false);
    for (const auto& g : x.events) {
        bool ok = false;
        for (int i = 0; i < n && !ok; i++)
            if (!used[i] && sameEvent(p, g, gEvents[i])) used[i] = ok = true;
        if (!ok) {
            std::string s = "missing an event at part " + std::to_string(g.ps.num) + "/" + std::to_string(g.ps.den);
            for (int i = 0; i < n; i++)
                if (!used[i]) return s + "; first unmatched of ours: " + eventText(p, gEvents[i]);
            return s;
        }
    }
    for (int i = 0; i < n; i++)
        if (!used[i]) return "extra event: " + eventText(p, gEvents[i]);
    return "";
}

// ---- property checks for random cases (golden/random.txt) ------------------------
static Program gProg3;
static Event gRef[4096];

static bool sameTimes(const Event& a, const Event& b) {
    if (a.hasWhole != b.hasWhole || a.part.s != b.part.s || a.part.e != b.part.e) return false;
    return !a.hasWhole || (a.whole.s == b.whole.s && a.whole.e == b.whole.e);
}
static bool sameEngineEvent(const Event& a, const Event& b) {
    if (!sameTimes(a, b) || a.nctl != b.nctl) return false;
    for (int i = 0; i < a.nctl; i++) {
        bool found = false;
        for (int j = 0; j < b.nctl && !found; j++) {
            const Ctl &x = a.ctl[i], &y = b.ctl[j];
            if (x.key != y.key || x.tag != y.tag) continue;
            found = fabsf(x.f - y.f) < 1e-6f;
            if (x.tag == 's') {
                char s1[64], s2[64];
                formatCtl(gProg, x, s1, sizeof s1);
                formatCtl(gProg3, y, s2, sizeof s2);
                found = strcmp(s1, s2) == 0;
            }
        }
        if (!found) return false;
    }
    return true;
}

// "" if every check on the line holds for our events of case c.
static std::string randomChecks(const golden::Case& c, const std::vector<std::string>& checks, uint16_t expr) {
    Error e;
    Arc arc{Q{0, 1}, Q{c.cycles, 1}};
    int n = queryPattern(gProg, expr, arc, gEvents, 4096, e);
    if (n < 0) return std::string("engine error: ") + e.msg;
    for (const auto& chk : checks) {
        std::istringstream in(chk);
        std::string what;
        in >> what;
        std::string rest;
        std::getline(in, rest);
        if (what == "subset" || what == "like") {
            std::string text = "d1 $" + rest + "\n";
            Error pe, ce, qe;
            if (!parseFile(gProg3, text.c_str(), (int)text.size(), pe) || !checkProgram(gProg3, ce))
                return "bad reference: " + rest;
            int m = queryPattern(gProg3, gProg3.at(gProg3.block[0]).first, arc, gRef, 4096, qe);
            if (m < 0) return std::string("reference error: ") + qe.msg;
            std::vector<bool> used(m, false);
            if (what == "like" && m != n) return "like:" + rest + ": " + std::to_string(n) + " events, want " + std::to_string(m);
            for (int i = 0; i < n; i++) {
                bool ok = false;
                for (int j = 0; j < m && !ok; j++)
                    if (!used[j] && (what == "like" ? sameTimes(gEvents[i], gRef[j]) : sameEngineEvent(gEvents[i], gRef[j])))
                        used[j] = ok = true;
                if (!ok) return what + ":" + rest + ": not expected: " + eventText(gProg, gEvents[i]);
            }
        } else if (what == "count") {
            int lo, hi;
            in.clear();
            std::istringstream r(rest);
            r >> lo >> hi;
            if (n < lo || n > hi) return "count " + std::to_string(n) + " not in " + rest;
        } else if (what == "key" || what == "value") {
            std::istringstream r(rest);
            std::string key;
            double lo, hi;
            r >> key >> lo >> hi;
            int k = controlKey(key.c_str()), with = 0;
            for (int i = 0; i < n; i++) {
                const Ctl* found = nullptr;
                for (int j = 0; j < gEvents[i].nctl; j++)
                    if (gEvents[i].ctl[j].key == k) found = &gEvents[i].ctl[j];
                if (found) with++;
                if (what == "value" && (!found || found->f < lo || found->f >= hi))
                    return "value " + key + " out of range in " + eventText(gProg, gEvents[i]);
            }
            double frac = n ? (double)with / n : 0;
            if (what == "key" && (frac < lo || frac > hi))
                return "key " + key + ": fraction " + std::to_string(frac) + " not in" + rest.substr(key.size() + 1);
        } else {
            return "unknown check " + what;
        }
    }
    return "";
}

// Golden cases (golden/cases.txt, captured from real Tidal into
// golden/expected.txt by golden/capture.py): every case must parse and
// type-check here, render stably, and match its captured block. Event
// times must fit the engine's 32-bit rationals. The event comparison
// itself comes with the query engine.
static void testGolden() {
    std::vector<golden::Case> cases, expected;
    std::string err;
    bool ok = golden::loadCases("golden/cases.txt", cases, err) &&
              golden::loadExpected("golden/expected.txt", expected, err);
    EXPECT(ok, "golden files: %s", err.c_str());
    if (!ok) return;
    EXPECT(cases.size() == expected.size(), "golden: %zu cases, %zu captured (rerun make golden)", cases.size(),
           expected.size());
    size_t events = 0;
    long long maxDen = 0;
    // engine: cases in golden/engine_pass.txt must match (ratchet)
    std::set<std::string> mustPass;
    {
        std::ifstream f("golden/engine_pass.txt");
        std::string line;
        while (std::getline(f, line))
            if (!line.empty() && line[0] != '#') mustPass.insert(line);
    }
    std::map<std::string, std::vector<std::string>> randomCases;
    {
        std::ifstream f("golden/random.txt");
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::vector<std::string> parts;
            size_t pos;
            while ((pos = line.find(" | ")) != std::string::npos) {
                parts.push_back(line.substr(0, pos));
                line = line.substr(pos + 3);
            }
            parts.push_back(line);
            std::string name = parts[0];
            name.erase(name.find_last_not_of(' ') + 1);
            parts.erase(parts.begin());
            randomCases[name] = parts;
        }
    }
    const char* verbose = getenv("GOLDEN_VERBOSE");
    engineResetStats();
    int engineOk = 0, engineTotal = 0;
    std::vector<std::string> newlyPassing;
    for (size_t i = 0; i < cases.size(); i++) {
        const golden::Case& c = cases[i];
        if (i < expected.size()) {
            const golden::Case& x = expected[i];
            EXPECT(x.name == c.name && x.cycles == c.cycles && x.code == c.code,
                   "golden %s: expected.txt is stale (rerun make golden)", c.name.c_str());
            events += x.events.size();
            for (const auto& e : x.events) {
                for (const golden::Rat* r : {&e.ws, &e.we, &e.ps, &e.pe}) {
                    EXPECT(r->num >= INT32_MIN && r->num <= INT32_MAX && r->den <= INT32_MAX,
                           "golden %s: time %lld/%lld exceeds 32 bits", c.name.c_str(), r->num, r->den);
                    if (r->den > maxDen) maxDen = r->den;
                }
            }
        }
        if (c.name[0] == '@') continue;  // Tidal-only expression (transitions)
        std::string text = "d1 $ " + c.code + "\n";
        Error e;
        bool parsed = parseFile(gProg, text.c_str(), (int)text.size(), e);
        EXPECT(parsed, "golden %s (cases.txt:%d): parse: %s at %d", c.name.c_str(), c.line, e.msg, e.pos);
        if (!parsed) continue;
        Error ce;
        EXPECT(checkProgram(gProg, ce), "golden %s (cases.txt:%d): type: %s", c.name.c_str(), c.line, ce.msg);
        // render, reparse, render again: must be stable
        char a[1024], b[1024];
        Renderer r1(a, sizeof a);
        a[0] = 0;
        r1.file(gProg);
        Error e2;
        bool reparsed = parseFile(gProg2, a, (int)strlen(a), e2);
        EXPECT(reparsed, "golden %s: reparse of '%s': %s", c.name.c_str(), a, e2.msg);
        if (!reparsed) continue;
        Renderer r2(b, sizeof b);
        b[0] = 0;
        r2.file(gProg2);
        EXPECT(strcmp(a, b) == 0, "golden %s: render not stable:\n%s%s", c.name.c_str(), a, b);
        if (i < expected.size() && gProg.block[0]) {
            engineTotal++;
            uint16_t expr = gProg.at(gProg.block[0]).first;
            auto rc = randomCases.find(c.name);
            std::string d = rc != randomCases.end() ? randomChecks(c, rc->second, expr)
                                                    : engineDiff(c, expected[i], gProg, expr);
            bool must = mustPass.count(c.name) > 0;
            if (d.empty()) {
                engineOk++;
                if (!must) newlyPassing.push_back(c.name);
            }
            EXPECT(!must || d.empty(), "engine %s regressed: %s", c.name.c_str(), d.c_str());
            if (!d.empty() && verbose && (!*verbose || c.name.find(verbose) != std::string::npos))
                printf("  engine %-18s %s\n", c.name.c_str(), d.c_str());
        }
    }
    printf("engine: %d/%d golden cases match, arena high water %d/%d events (%zu B each)", engineOk, engineTotal,
           engineHighWater(), kEventArena, (size_t)engineEventBytes());
    if (!newlyPassing.empty()) {
        printf(" (%zu not yet in golden/engine_pass.txt:", newlyPassing.size());
        for (auto& s : newlyPassing) printf(" %s", s.c_str());
        printf(")");
    }
    printf("\n");
    EXPECT(events > 0, "golden: no events captured");
    printf("golden: %zu cases, %zu events, max denominator %lld\n", cases.size(), events, maxDen);
}

void testScheduler(int& pass, int& fail);

int main() {
    testNodeSize();
    testTypesPool();
    testRationals();
    testRoundTrips();
    testParseErrors();
    testFiles();
    testTypes();
    testRefinement();
    testGolden();
    testScheduler(gPass, gFail);
    printf("%d passed, %d failed\n", gPass, gFail);
    return gFail ? 1 : 0;
}
