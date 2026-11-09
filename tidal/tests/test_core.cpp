// Core tests: parse/render round trips, fixity, type checking, refinement.
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "check.h"
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
    }
    EXPECT(events > 0, "golden: no events captured");
    printf("golden: %zu cases, %zu events, max denominator %lld\n", cases.size(), events, maxDen);
}

int main() {
    testNodeSize();
    testTypesPool();
    testRoundTrips();
    testParseErrors();
    testFiles();
    testTypes();
    testRefinement();
    testGolden();
    printf("%d passed, %d failed\n", gPass, gFail);
    return gFail ? 1 : 0;
}
