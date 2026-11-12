// Query engine. See engine.h. Behaviour notes (all checked against
// golden/expected.txt, captured from Tidal 1.10.3):
//  - A sequence squeezes each step into its slot of every cycle. Parts map
//    with the queried cycle, wholes with the cycle the whole starts in, so
//    a slowed step ("bd sd/2") keeps one whole across cycles.
//  - "<a b>" is the sequence slowed to one step per cycle; cat / slowcat
//    give each pattern its own cycle count (cycle c plays pattern c mod n
//    at its cycle c div n).
//  - Patterned arguments ("fast "1 2"") query the argument and apply the
//    function inside each of its events; constants are pure patterns, so
//    queries split at cycle boundaries.
//  - `#`, `|+`, ... take structure from the left: the right side is queried
//    over each left whole, and parts are intersected (fragments).
//  - Signals have no whole; their value is taken at the query start.
//  - Randomness is ours, not Tidal's (its generator is only defined in GPL
//    source): deterministic like Tidal (a hash of exact time and a seed),
//    same probabilities and structure, different values. Events are tested
//    at their whole start; each ? / | in mini-notation has its own seed
//    (its position among the ? and | of that pattern).
#include "engine.h"

// On the MK66 flash is the tight budget: -Os halves the engine (~67 KB ->
// ~33 KB text) for ~1.5x the query time on the host. Define TC_ENGINE_O2
// to build it with the caller's flags instead (timing comparisons).
#if defined(__arm__) && !defined(TC_ENGINE_O2)
#pragma GCC optimize("Os")
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <new>

#include "check.h"
#include "library.h"

namespace tc {

// Floor modulo / division on cycle and step numbers; 32-bit (hardware
// divide on the M4) when both fit, which is always in practice.
static inline int64_t imod(int64_t a, int64_t n) {
    if (qfits(a) && qfits(n)) {
        int32_t r = (int32_t)a % (int32_t)n;
        return (r != 0 && ((r < 0) != (n < 0))) ? r + n : r;
    }
    return ((a % n) + n) % n;
}
static inline int64_t idivFloor(int64_t a, int64_t n) {
    if (qfits(a) && qfits(n)) {
        int32_t q = (int32_t)a / (int32_t)n;
        return (q * (int32_t)n != (int32_t)a && (((int32_t)a < 0) != (n < 0))) ? q - 1 : q;
    }
    return (a - imod(a, n)) / n;
}

bool subArc(Arc a, Arc b, Arc& out) {
    Q s = qmax(a.s, b.s), e = qmin(a.e, b.e);
    if (s == e && s == a.e && a.s < a.e) return false;  // touching at a's end
    if (s == e && s == b.e && b.s < b.e) return false;
    if (s > e) return false;
    out = Arc{s, e};
    return true;
}

// ---- controls ------------------------------------------------------------------
static const ControlDef kControls[] = {
    {"s", 's'},       {"n", 'n'},       {"note", 'n'},    {"gain", 'f'},    {"pan", 'f'},
    {"cutoff", 'f'},  {"resonance", 'f'}, {"hcutoff", 'f'}, {"bandf", 'f'},  {"delay", 'f'},
    {"room", 'f'},    {"crush", 'f'},   {"shape", 'f'},   {"begin", 'f'},   {"end", 'f'},
    {"speed", 'f'},   {"legato", 'f'},  {"sustain", 'f'}, {"nudge", 'f'},   {"cut", 'i'},
    {"orbit", 'i'},   {"unit", 's'},
    // Tracker mapping (docs/tidal_control_mapping.md)
    {"amp", 'f'},     {"velocity", 'f'}, {"attack", 'f'},  {"release", 'f'}, {"hresonance", 'f'},
    {"distort", 'f'}, {"midichan", 'f'}, {"ccn", 'f'},     {"ccv", 'f'},     {"midibend", 'f'},
    {"voices", 'i'},
    // batch 3 (2026-11-12): global FX (the Tracker has one reverb / delay), DJ
    // filter, MIDI program / aftertouch; coarse for the host (no decimator)
    {"size", 'f'},    {"delaytime", 'f'}, {"delayfeedback", 'f'}, {"djf", 'f'}, {"coarse", 'f'},
    {"progNum", 'f'}, {"miditouch", 'f'},
};
int controlCount() { return (int)(sizeof kControls / sizeof kControls[0]); }
const ControlDef& controlDef(int key) { return kControls[key]; }
int controlKey(const char* name) {
    for (int i = 0; i < controlCount(); i++)
        if (strcmp(kControls[i].name, name) == 0) return i;
    return -1;
}
// library control function -> output key, where they differ
static const char* controlFor(const char* fn) {
    if (!strcmp(fn, "sound")) return "s";
    if (!strcmp(fn, "up")) return "note";
    if (!strcmp(fn, "hpf")) return "hcutoff";
    if (!strcmp(fn, "lpf")) return "cutoff";
    if (!strcmp(fn, "lpq")) return "resonance";
    if (!strcmp(fn, "hpq")) return "hresonance";
    if (!strcmp(fn, "bpf")) return "bandf";
    if (!strcmp(fn, "att")) return "attack";
    if (!strcmp(fn, "rel")) return "release";
    if (!strcmp(fn, "sz")) return "size";
    if (!strcmp(fn, "delayt")) return "delaytime";
    if (!strcmp(fn, "delayfb")) return "delayfeedback";
    return fn;
}

static const char* kEngineStrings[] = {"c"};
enum { kStrC = 0 };

static const char* valStr(const Program& p, uint16_t id) {
    if (id >= kEngineStr) return kEngineStrings[id - kEngineStr];
    return p.str(id);
}

int formatVal(const Program& p, const Val& v, char* buf, int cap) {
    switch (v.t) {
        case VT::Str: return snprintf(buf, cap, "%s", valStr(p, v.str));
        case VT::Num: return snprintf(buf, cap, "%.9g", (double)num(v));
        case VT::Bool: return snprintf(buf, cap, "%s", v.q.n ? "t" : "f");
        default: return snprintf(buf, cap, "?");
    }
}

int formatCtl(const Program& p, const Ctl& c, char* buf, int cap) {
    if (c.tag == 's') return snprintf(buf, cap, "%s", valStr(p, c.str));
    return snprintf(buf, cap, "%.9g", (double)c.f);
}

// ---- values ----------------------------------------------------------------------
static float qfloat(Q q) { return (float)q.n / (float)q.d; }
static Val numQ(Q q) {
    Val v;
    v.t = VT::Num;
    v.exact = true;
    v.q = q;
    return v;
}
static Val numF(float f) {
    Val v;
    v.t = VT::Num;
    v.fl = f;
    return v;
}
static Ctl ctlOf(uint8_t key, char tag, const Val& v) {
    Ctl c;
    c.key = key;
    c.tag = tag;
    c.str = v.t == VT::Str ? v.str : 0;
    c.f = v.t == VT::Str ? 0.0f : v.t == VT::Bool ? (float)v.q.n : num(v);
    return c;
}
static Val strV(uint16_t id, int idx = -1) {
    Val v;
    v.t = VT::Str;
    v.str = id;
    v.idx = (int16_t)idx;
    return v;
}
static Val boolV(bool b) {
    Val v;
    v.t = VT::Bool;
    v.q = Q{b ? 1 : 0, 1};
    return v;
}

enum class NumOp : uint8_t { Add, Sub, Mul, Div, Mod, Pow };
static float fop(NumOp op, float a, float b) {
    switch (op) {
        case NumOp::Add: return a + b;
        case NumOp::Sub: return a - b;
        case NumOp::Mul: return a * b;
        case NumOp::Div: return a / b;
        case NumOp::Mod: return b == 0 ? 0 : a - b * floorf(a / b);  // floored, as Haskell's mod'
        case NumOp::Pow: return powf(a, b);
    }
    return 0;
}

// Exact while both sides are exact and the result fits 32 bits.
static bool numOp(NumOp op, const Val& a, const Val& b, Val& out) {
    if (a.t != VT::Num || b.t != VT::Num) return false;
    if (a.exact && b.exact && op != NumOp::Pow && !((op == NumOp::Div || op == NumOp::Mod) && b.q.n == 0)) {
        bool saved = qOverflowFlag();
        qOverflowFlag() = false;
        Q r = op == NumOp::Add   ? a.q + b.q
              : op == NumOp::Sub ? a.q - b.q
              : op == NumOp::Mul ? a.q * b.q
              : op == NumOp::Div ? a.q / b.q
                                 : a.q - b.q * qint(qfloor(a.q / b.q));
        bool ovf = qOverflowFlag();
        qOverflowFlag() = saved;
        if (!ovf) {
            out = numQ(r);
            return true;
        }
    }
    out = numF(fop(op, num(a), num(b)));
    return true;
}

// ---- library dispatch --------------------------------------------------------------
enum class Fn : uint8_t {
    None, Silence, Fast, Slow, Hurry, Rev, Palindrome, Brak, Iter, Ply, Linger, Segment, Stack, Cat,
    Fastcat, Slowcat, Overlay, Every, Whenmod, Range, Struct, Mask, Euclid, EuclidInv, EuclidFull,
    DegradeBy, SometimesBy, Sometimes, SomeCyclesBy, Rand, Irand, Sine, Cosine, Saw, Tri, Square, Superimpose, Off, Jux, Stut, Echo, Chunk, Chop, Striate,
    LoopAt, Splice, Slice, Scale, Arp, Control,
    // batch 2 (2026-11-11); callLib2
    EveryP, Inside, Outside, Trunc, FastGap, Wedge, ChunkR, Run, Scan, Inv, Sew, Stitch, While, EuclidOff,
    Press, PressBy, SwingBy, Swing, Rot, Bite, Squeeze, Select, PickF, SelectF, Layer, JuxBy, EchoWith,
    PlyWith, Rolled, RolledBy, ToScale, ToScaleP,
    Degrade, UnDegradeBy, SometimesByP, SomeCycles, Shuffle, Scramble, Perlin, Randcat, Choose, ChooseBy,
    CycleChoose, Within, WithinP, Zoom, Compress, WChoose, WRandcat,
    // batch 3 (2026-11-12); callLib3
    Stutter, Ghost, GhostP, IterR, Rangex, Quantise, Isaw, EnvL, EnvLR, EnvEq, EnvEqR, Brand, BrandBy, Append,
    FastAppend, RepeatCycles, FoldEvery, Fix, Unfix, Contrast, Randslice, Chew, QTrigger, Trigger,
    // operators
    OpUnionR, OpUnionL, OpNum, ShiftL, ShiftR,
};
enum class Side : uint8_t { Left, Both, Right };

struct FnInfo {
    Fn fn = Fn::None;
    NumOp nop = NumOp::Add;
    Side side = Side::Left;
    int8_t key = -1;  // Control: output key
    float prob = 0;  // Sometimes: fixed probability
};

static const FnInfo& fnInfo(int id) {
    static FnInfo table[256];
    static bool init = false;
    if (!init) {
        static const struct { const char* n; Fn f; } names[] = {
            {"silence", Fn::Silence},   {"fast", Fn::Fast},           {"slow", Fn::Slow},
            {"hurry", Fn::Hurry},       {"rev", Fn::Rev},             {"palindrome", Fn::Palindrome},
            {"brak", Fn::Brak},         {"iter", Fn::Iter},           {"ply", Fn::Ply},
            {"linger", Fn::Linger},     {"segment", Fn::Segment},     {"stack", Fn::Stack},
            {"cat", Fn::Cat},           {"fastcat", Fn::Fastcat},     {"slowcat", Fn::Slowcat},
            {"overlay", Fn::Overlay},   {"every", Fn::Every},         {"whenmod", Fn::Whenmod},
            {"range", Fn::Range},       {"struct", Fn::Struct},       {"mask", Fn::Mask},
            {"euclid", Fn::Euclid},     {"euclidInv", Fn::EuclidInv}, {"euclidFull", Fn::EuclidFull},
            {"degradeBy", Fn::DegradeBy}, {"sometimesBy", Fn::SometimesBy}, {"sometimes", Fn::Sometimes},
            {"often", Fn::Sometimes},   {"rarely", Fn::Sometimes},    {"almostNever", Fn::Sometimes},
            {"almostAlways", Fn::Sometimes}, {"someCyclesBy", Fn::SomeCyclesBy}, {"rand", Fn::Rand},
            {"irand", Fn::Irand},      {"sine", Fn::Sine},           {"cosine", Fn::Cosine},
            {"saw", Fn::Saw},           {"tri", Fn::Tri},             {"square", Fn::Square},
            {"superimpose", Fn::Superimpose}, {"off", Fn::Off},       {"jux", Fn::Jux},
            {"stut", Fn::Stut},         {"echo", Fn::Echo},           {"chunk", Fn::Chunk},
            {"chop", Fn::Chop},         {"striate", Fn::Striate},     {"loopAt", Fn::LoopAt},
            {"splice", Fn::Splice},     {"slice", Fn::Slice},     {"scale", Fn::Scale},         {"arp", Fn::Arp},
            {"#", Fn::OpUnionR},        {"|>", Fn::OpUnionR},         {"|<", Fn::OpUnionL},
            {">|", Fn::OpUnionR},       {"<|", Fn::OpUnionL},
            {"every'", Fn::EveryP},     {"inside", Fn::Inside},       {"outside", Fn::Outside},
            {"trunc", Fn::Trunc},       {"discretise", Fn::Segment},  {"fastGap", Fn::FastGap},
            {"wedge", Fn::Wedge},       {"chunk'", Fn::ChunkR},       {"run", Fn::Run},
            {"scan", Fn::Scan},         {"inv", Fn::Inv},             {"sew", Fn::Sew},
            {"stitch", Fn::Stitch},     {"while", Fn::While},         {"euclidOff", Fn::EuclidOff},
            {"eoff", Fn::EuclidOff},    {"press", Fn::Press},         {"pressBy", Fn::PressBy},
            {"swingBy", Fn::SwingBy},   {"swing", Fn::Swing},         {"rot", Fn::Rot},
            {"bite", Fn::Bite},         {"squeeze", Fn::Squeeze},     {"select", Fn::Select},
            {"pickF", Fn::PickF},       {"selectF", Fn::SelectF},     {"layer", Fn::Layer},
            {"juxBy", Fn::JuxBy},       {"echoWith", Fn::EchoWith},   {"stutWith", Fn::EchoWith},
            {"plyWith", Fn::PlyWith},   {"rolled", Fn::Rolled},       {"rolledBy", Fn::RolledBy},
            {"toScale", Fn::ToScale},   {"toScale'", Fn::ToScaleP},
            {"degrade", Fn::Degrade},   {"unDegradeBy", Fn::UnDegradeBy}, {"sometimesBy'", Fn::SometimesByP},
            {"someCycles", Fn::SomeCycles}, {"somecycles", Fn::SomeCycles}, {"shuffle", Fn::Shuffle},
            {"scramble", Fn::Scramble}, {"perlin", Fn::Perlin},       {"randcat", Fn::Randcat},
            {"choose", Fn::Choose},     {"chooseBy", Fn::ChooseBy},   {"cycleChoose", Fn::CycleChoose},
            {"within", Fn::Within},     {"within'", Fn::WithinP},     {"zoom", Fn::Zoom},
            {"compress", Fn::Compress}, {"wchoose", Fn::WChoose},     {"wrandcat", Fn::WRandcat},
            {"|>|", Fn::OpUnionR},      {"|<|", Fn::OpUnionL},
            {"stutter", Fn::Stutter},   {"ghost", Fn::Ghost},         {"ghost'", Fn::GhostP},
            {"iter'", Fn::IterR},       {"rangex", Fn::Rangex},       {"quantise", Fn::Quantise},
            {"isaw", Fn::Isaw},         {"envL", Fn::EnvL},           {"envLR", Fn::EnvLR},
            {"envEq", Fn::EnvEq},       {"envEqR", Fn::EnvEqR},       {"brand", Fn::Brand},
            {"brandBy", Fn::BrandBy},   {"append", Fn::Append},       {"slowAppend", Fn::Append},
            {"fastAppend", Fn::FastAppend}, {"repeatCycles", Fn::RepeatCycles}, {"foldEvery", Fn::FoldEvery},
            {"fix", Fn::Fix},           {"unfix", Fn::Unfix},         {"contrast", Fn::Contrast},
            {"randslice", Fn::Randslice}, {"chew", Fn::Chew},         {"qtrigger", Fn::QTrigger},
            {"trigger", Fn::Trigger},
            {"<~", Fn::ShiftL},         {"~>", Fn::ShiftR},
        };
        for (int id = 0; id < libCount() && id < 256; id++) {
            const LibEntry& e = lib(id);
            FnInfo& fi = table[id];
            for (auto& n : names)
                if (!strcmp(n.n, e.name)) fi.fn = n.f;
            static const struct { const char* n; float p; } odds[] = {
                {"sometimes", 0.5f}, {"often", 0.75f}, {"rarely", 0.25f}, {"almostNever", 0.1f}, {"almostAlways", 0.9f}};
            for (auto& o : odds)
                if (!strcmp(o.n, e.name)) fi.prob = o.p;
            if (e.cat == Cat::Control) {
                fi.fn = Fn::Control;
                fi.key = (int8_t)controlKey(controlFor(e.name));
            }
            // numeric operators: |op, |op|, op|, and Haskell + - * /
            const char* n = e.name;
            size_t len = strlen(n);
            // the bars say where the structure comes from: |op left, op| right,
            // |op| and bare Haskell + - * / both; # keeps the left (as |>)
            const bool lb = n[0] == '|', rb = len > 1 && n[len - 1] == '|';
            Side side = Side::Both;
            if (lb && !rb) side = Side::Left;
            if (!lb && rb) side = Side::Right;
            if (e.assoc != Assoc::None && (fi.fn == Fn::OpUnionR || fi.fn == Fn::OpUnionL) && (lb || rb))
                fi.side = side;
            if (e.assoc != Assoc::None && fi.fn == Fn::None) {
                const char* core = n;
                size_t cl = len;
                if (lb) core++, cl--;
                if (rb) cl--;
                if (cl == 1 && strchr("+-*/%", core[0])) {
                    fi.fn = Fn::OpNum;
                    fi.side = side;
                    fi.nop = core[0] == '+' ? NumOp::Add : core[0] == '-' ? NumOp::Sub
                           : core[0] == '*' ? NumOp::Mul : core[0] == '/' ? NumOp::Div : NumOp::Mod;
                } else if (cl == 2 && core[0] == '*' && core[1] == '*') {
                    fi.fn = Fn::OpNum;
                    fi.side = side;
                    fi.nop = NumOp::Pow;
                }
            }
        }
        init = true;
    }
    return table[id];
}

// ---- tables: chords and scales ---------------------------------------------------------
struct NamedSet {
    const char* name;
    int8_t n;
    int8_t v[8];
};
static const NamedSet kChords[] = {
    {"major", 3, {0, 4, 7}},       {"maj", 3, {0, 4, 7}},         {"M", 3, {0, 4, 7}},
    {"aug", 3, {0, 4, 8}},         {"plus", 3, {0, 4, 8}},        {"six", 4, {0, 4, 7, 9}},
    {"minor", 3, {0, 3, 7}},       {"min", 3, {0, 3, 7}},         {"m", 3, {0, 3, 7}},
    {"major7", 4, {0, 4, 7, 11}},  {"maj7", 4, {0, 4, 7, 11}},    {"dom7", 4, {0, 4, 7, 10}},
    {"minor7", 4, {0, 3, 7, 10}},  {"min7", 4, {0, 3, 7, 10}},    {"m7", 4, {0, 3, 7, 10}},
    {"dim", 3, {0, 3, 6}},         {"dim7", 4, {0, 3, 6, 9}},     {"sus2", 3, {0, 2, 7}},
    {"sus4", 3, {0, 5, 7}},        {"one", 1, {0}},               {"five", 2, {0, 7}},
};
static const NamedSet kScales[] = {
    {"major", 7, {0, 2, 4, 5, 7, 9, 11}},     {"ionian", 7, {0, 2, 4, 5, 7, 9, 11}},
    {"minor", 7, {0, 2, 3, 5, 7, 8, 10}},     {"aeolian", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"dorian", 7, {0, 2, 3, 5, 7, 9, 10}},    {"phrygian", 7, {0, 1, 3, 5, 7, 8, 10}},
    {"lydian", 7, {0, 2, 4, 6, 7, 9, 11}},    {"mixolydian", 7, {0, 2, 4, 5, 7, 9, 10}},
    {"locrian", 7, {0, 1, 3, 5, 6, 8, 10}},   {"majPent", 5, {0, 2, 4, 7, 9}},
    {"minPent", 5, {0, 3, 5, 7, 10}},         {"harmonicMinor", 7, {0, 2, 3, 5, 7, 8, 11}},
    {"melodicMinor", 7, {0, 2, 3, 5, 7, 9, 11}}, {"whole", 6, {0, 2, 4, 6, 8, 10}},
    {"wholetone", 6, {0, 2, 4, 6, 8, 10}},
};
static const NamedSet* findSet(const NamedSet* t, int n, const char* name) {
    for (int i = 0; i < n; i++)
        if (!strcmp(t[i].name, name)) return &t[i];
    return nullptr;
}

// Euclidean rhythm: k onsets spread over n steps (Bjorklund's algorithm).
static void bjorklund(int k, int n, bool* out) {
    for (int i = 0; i < n; i++) out[i] = false;
    if (n <= 0 || k <= 0) return;
    if (k >= n) {
        for (int i = 0; i < n; i++) out[i] = true;
        return;
    }
    // groups as bit strings (length <= n <= 64)
    uint64_t a[64], b[64];
    int al[64], bl[64], na = k, nb = n - k;
    for (int i = 0; i < na; i++) a[i] = 1, al[i] = 1;
    for (int i = 0; i < nb; i++) b[i] = 0, bl[i] = 1;
    while (nb > 1) {
        int m = na < nb ? na : nb;
        uint64_t ra[64];
        int rl[64], nr = 0;
        if (na > m) {
            for (int i = m; i < na; i++) ra[nr] = a[i], rl[nr++] = al[i];
        } else {
            for (int i = m; i < nb; i++) ra[nr] = b[i], rl[nr++] = bl[i];
        }
        for (int i = 0; i < m; i++) {
            a[i] = (a[i] << bl[i]) | b[i];
            al[i] += bl[i];
        }
        na = m;
        for (int i = 0; i < nr; i++) b[i] = ra[i], bl[i] = rl[i];
        nb = nr;
    }
    int pos = 0;
    for (int g = 0; g < na + nb; g++) {
        uint64_t bits = g < na ? a[g] : b[g - na];
        int len = g < na ? al[g] : bl[g - na];
        for (int i = len - 1; i >= 0 && pos < n; i--) out[pos++] = (bits >> i) & 1;
    }
}

// Deterministic random number in [0, 1) for a time and seed (splitmix64
// finaliser over the exact rational time).
static float rnd(Q t, uint32_t seed) {
    t = qnorm(t);  // same time, same value, however it was computed
    uint64_t x = (uint64_t)(uint32_t)t.n * 0x9E3779B97F4A7C15ull ^ (uint64_t)(uint32_t)t.d * 0xC2B2AE3D27D4EB4Full ^
                 ((uint64_t)seed + 1) * 0x165667B19E3779F9ull;
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return (float)(x >> 40) * (1.0f / 16777216.0f);
}

// ---- patterns as closures ------------------------------------------------------------
// A Pat is a tree node, or an internal transform of another Pat. They live
// on the C stack for the duration of a query.
struct Pat {
    enum K : uint8_t {
        Node, Apply, Shift, ShiftP, Fast, Rev, Compress, Zoom, Pure, Overlay, SetCtl, MulCtl, Silence,
        Split,    // x queried cycle by cycle (a patterned argument's innerJoin)
        Within,   // x where the onset's cycle position is in [q, q2), else y
        SetCtlP,  // x with control key = q2 + q * (value of pattern y)
        Inv,      // x with its booleans flipped
        Degrade,  // x without the events whose random value is < v (neg: only those)
        EveryF,   // foldEvery: node f applied to x on cycles divisible by q
        Match,    // fix / contrast: x's control events that y matches at their onset (neg: those it doesn't)
        ClipCycles,  // x with each whole cut at cycle boundaries (what `|*| pure` does to structure)
    };
    K k = Silence;
    uint16_t node = 0;      // Node: expression; Apply: function expression
    const Pat* x = nullptr;
    const Pat* y = nullptr;  // Overlay: second; ShiftP: the time pattern
    Q q{0, 1}, q2{1, 1};     // Shift / Fast amount; Compress / Zoom span [q, q2)
    bool neg = false;        // ShiftP: rotate left
    Val v;                   // Pure / SetCtl / MulCtl value
    uint8_t key = 0;         // SetCtl / MulCtl
    char tag = 'f';
};
static Pat pNode(uint16_t n) {
    Pat p;
    p.k = Pat::Node;
    p.node = n;
    return p;
}
static Pat pWrap(Pat::K k, const Pat* x) {
    Pat p;
    p.k = k;
    p.x = x;
    return p;
}

// ---- evaluator ---------------------------------------------------------------------
// Raw storage, so nothing runs at static-init time (the firmware may place
// it in NOLOAD SDRAM); events are constructed in push().
// Internal event: control maps live out of line in gCtl (copy on write,
// bump-allocated, reset per query), so moving and copying events is cheap
// (~52 B instead of the 212 B public Event).
struct Ev {
    Arc whole;  // valid if hasWhole
    Arc part;
    bool hasWhole = true;
    bool isMap = false;
    uint8_t nctl = 0;    // isMap: entries at gCtl[ctlAt]
#if TC_EVENT_SRC
    uint8_t nsrc = 0;    // source atoms (engine.h)
#endif
    uint16_t ctlAt = 0;
#if TC_EVENT_SRC
    uint16_t src[kMaxSrc];
#endif
    Val v;               // !isMap
};
#if TC_EVENT_SRC
static inline void addSrc(Ev& o, uint16_t id) {
    for (int k = 0; k < o.nsrc; k++)
        if (o.src[k] == id) return;
    if (o.nsrc < kMaxSrc) o.src[o.nsrc++] = id;
}
static inline void joinSrc(Ev& o, const Ev& l, const Ev& r) {
    o.nsrc = 0;
    for (int k = 0; k < l.nsrc; k++) addSrc(o, l.src[k]);
    for (int k = 0; k < r.nsrc; k++) addSrc(o, r.src[k]);
}
#define TC_JOIN_SRC(o, l, r) joinSrc(o, l, r)
#define TC_TAG_SRC(from, id)                                              \
    for (int i_ = (from); i_ < top; i_++) {                               \
        gArena[i_].nsrc = 1;                                              \
        gArena[i_].src[0] = (id);                                         \
    }
#else
#define TC_JOIN_SRC(o, l, r) ((void)0)
#define TC_TAG_SRC(from, id) ((void)0)
#endif
TC_ENGINE_ARENA_ATTR static unsigned char gArenaRaw[sizeof(Ev) * kEventArena] __attribute__((aligned(8)));
static Ev* gArena = reinterpret_cast<Ev*>(gArenaRaw);
TC_ENGINE_ARENA_ATTR static Ctl gCtlPool[kCtlPool];
static Ctl* gCtl = gCtlPool;
static int gCtlCap = kCtlPool;
static int gCtlTop = 0, gCtlHighWater = 0;
// withParam's values, moved out of the arena so results land in place
struct ParamVal {
    Val v;
    Arc part;
    bool isMap;
};
// raw storage: with TC_ENGINE_ARENA_ATTR in SDRAM (NOLOAD) there must be
// no static constructor (it would run before SDRAM is set up); entries
// are assigned before they are read
TC_ENGINE_ARENA_ATTR static unsigned char gParamsRaw[sizeof(ParamVal) * kParamStack] __attribute__((aligned(8)));
static ParamVal* gParams = reinterpret_cast<ParamVal*>(gParamsRaw);
static int gParamCap = kParamStack;
static int gParamTop = 0;
static int gArenaCap = kEventArena;
static int gHighWater = 0;
// Lowest stack address seen in the recursive evaluator (TC_TRACK_STACK).
static uintptr_t gMinSp = UINTPTR_MAX;
#ifdef TC_TRACK_STACK
#define TC_NOTE_SP()                                   \
    do {                                               \
        volatile char here_ = 0;                       \
        uintptr_t sp_ = (uintptr_t)&here_;             \
        if (sp_ < gMinSp) gMinSp = sp_;                \
    } while (0)
#else
#define TC_NOTE_SP()
#endif
uintptr_t engineMinStack() { return gMinSp; }

bool isStepAtom(Kind k) {
    return k == Kind::MWord || k == Kind::MNum || k == Kind::MNote || k == Kind::MChord || k == Kind::MBool ||
           k == Kind::MRange;
}

static void atomWalk(const Program& p, uint16_t n, uint16_t* out, int cap, int& cnt) {
    for (; n; n = p.at(n).next) {
        if (isStepAtom(p.at(n).kind)) {
            if (cnt < cap) out[cnt] = n;
            cnt++;
        }
        atomWalk(p, p.at(n).first, out, cap, cnt);
    }
}

int atomOrder(const Program& p, uint16_t root, uint16_t* out, int cap) {
    if (!root) return 0;
    int cnt = 0;
    if (isStepAtom(p.at(root).kind)) {
        if (cnt < cap) out[cnt] = root;
        cnt++;
    }
    atomWalk(p, p.at(root).first, out, cap, cnt);
    return cnt < cap ? cnt : cap;
}
int engineHighWater() { return gHighWater; }
void engineResetStats() {
    gHighWater = 0;
    gCtlHighWater = 0;
}
void engineResetMinStack() { gMinSp = UINTPTR_MAX; }
void* engineDefaultArena() { return gArenaRaw; }
void engineUseArena(void* storage, int bytes) {
    gArena = storage ? reinterpret_cast<Ev*>(storage) : reinterpret_cast<Ev*>(gArenaRaw);
    gArenaCap = storage ? bytes / (int)sizeof(Ev) : kEventArena;
}
int engineMemoryBytes(int events, int ctls, int params) {
    return events * (int)sizeof(Ev) + ctls * (int)sizeof(Ctl) + params * (int)sizeof(ParamVal) + 16;
}
void engineUseMemory(void* storage, int events, int ctls, int params) {
    if (!storage) {
        engineUseArena(nullptr, 0);
        gCtl = gCtlPool;
        gCtlCap = kCtlPool;
        gParams = reinterpret_cast<ParamVal*>(gParamsRaw);
        gParamCap = kParamStack;
        return;
    }
    uintptr_t p = ((uintptr_t)storage + 7) & ~(uintptr_t)7;
    engineUseArena((void*)p, events * (int)sizeof(Ev));
    p += events * sizeof(Ev);
    gCtl = reinterpret_cast<Ctl*>(p);
    gCtlCap = ctls;
    p += ctls * sizeof(Ctl);
    gParams = reinterpret_cast<ParamVal*>(p);
    gParamCap = params;
}
int engineEventBytes() { return (int)sizeof(Ev); }
int engineCtlHighWater() { return gCtlHighWater; }

class Eval {
public:
    Eval(const Program& p, Error& e) : P(p), err(e) {}
    int top = 0;

    void query(const Pat& p, Arc a);

private:
    const Program& P;
    Error& err;
    int depth = 0;

    bool bad() const { return err.failed; }
    void fail(const char* msg, uint16_t node = 0) {
        if (!err.failed) {
            err.set(-1, "%s", msg);
            err.node = node;
        }
    }
    Ev* push() {
        if (top >= gArenaCap) {
            fail("too many events in one query");
            return nullptr;
        }
        Ev* e = new (&gArena[top++]) Ev();
        if (top > gHighWater) gHighWater = top;
        return e;
    }
    // control maps (gCtl): read, allocate, copy-on-write with room to add
    static const Ctl* ctls(const Ev& e) { return &gCtl[e.ctlAt]; }
    int ctlAlloc(int n) {
        if (gCtlTop + n > gCtlCap) {
            fail("too many controls in one query");
            return -1;
        }
        int at = gCtlTop;
        gCtlTop += n;
        if (gCtlTop > gCtlHighWater) gCtlHighWater = gCtlTop;
        return at;
    }
    Ctl* ctlWrite(Ev& e, int extra) {
        int at = ctlAlloc(e.nctl + extra);
        if (at < 0) return nullptr;
        for (int k = 0; k < e.nctl; k++) gCtl[at + k] = gCtl[e.ctlAt + k];
        e.ctlAt = (uint16_t)at;
        return &gCtl[at];
    }
    // drop [from, to), keep everything after it
    void drop(int from, int to) {
        int n = top - to;
        if (n > 0 && from != to) memmove(&gArena[from], &gArena[to], sizeof(Ev) * n);
        top = from + (n > 0 ? n : 0);
    }

    template <class F>
    void forCycles(Arc a, F f) {
        if (a.s == a.e) {
            f(a, qfloor(a.s));
            return;
        }
        for (int64_t c = qfloor(a.s); qint(c) < a.e && !bad(); c++) {
            Arc sub{qmax(a.s, qint(c)), qmin(a.e, qint(c + 1))};
            if (sub.s < sub.e) f(sub, c);
        }
    }
    static void mapEvent(Ev& e, Q mul, Q add) {  // t -> t * mul + add
        e.part.s = e.part.s * mul + add;
        e.part.e = e.part.e * mul + add;
        if (e.hasWhole) {
            e.whole.s = e.whole.s * mul + add;
            e.whole.e = e.whole.e * mul + add;
        }
    }
    static void shiftEvent(Ev& e, Q t) {
        e.part.s = e.part.s + t;
        e.part.e = e.part.e + t;
        if (e.hasWhole) {
            e.whole.s = e.whole.s + t;
            e.whole.e = e.whole.e + t;
        }
    }

    // param pattern: call f(value, sub-arc) inside each of its events
    // The parameter's events move to gParams first, so f's results land at
    // m directly (no memmove of every result over the parameter events).
    // A literal (2, 0.25) or a string of one plain value ("2") is a pure
    // pattern in Tidal: its functions use the value as it is, without
    // splitting the query at cycles (arithmetic like (1/3) is not pure).
    bool pureValue(const Pat& p, Val& v) const {
        if (p.k == Pat::Pure) {
            v = p.v;
            return true;
        }
        if (p.k != Pat::Node) return false;
        const Node& nd = P.at(p.node);
        if (nd.kind == Kind::Num) {
            v = numQ(qmake((int32_t)nd.a, (int32_t)nd.b));
            return true;
        }
        if (nd.kind != Kind::Mini || !nd.first) return false;
        const Node& sq = P.at(nd.first);
        if (sq.kind != Kind::MSeq || (sq.flags & kDots) || !sq.first || P.at(sq.first).next) return false;
        const Node& at = P.at(sq.first);
        switch (at.kind) {
            case Kind::MNum: v = numQ(qmake((int32_t)at.a, (int32_t)at.b)); return true;
            case Kind::MNote: v = numQ(qint((int32_t)at.a)); return true;
            case Kind::MBool: v = boolV(at.a != 0); return true;
            default: return false;
        }
    }

    template <class F>
    void withParam(const Pat& param, Arc a, F f) {
        {
            Val pv;
            if (pureValue(param, pv)) return f(pv, a);
        }
        int m = top;
        query(param, a);
        int k = top - m, base = gParamTop;
        if (base + k > gParamCap) return fail("patterned arguments nested too deeply");
        for (int j = 0; j < k; j++) new (&gParams[base + j]) ParamVal{gArena[m + j].v, gArena[m + j].part, gArena[m + j].isMap};
        top = m;
        gParamTop = base + k;
        for (int j = 0; j < k && !bad(); j++) {
            const ParamVal& pv = gParams[base + j];
            if (pv.isMap) {
                fail("a control pattern can't be a parameter");
                break;
            }
            f(pv.v, pv.part);
        }
        gParamTop = base;
    }
    Q timeOf(const Val& v) {
        if (v.t != VT::Num) {
            fail("expected a number");
            return Q{0, 1};
        }
        if (v.exact) return v.q;
        return qmake((int64_t)lroundf(v.fl * 65536.0f), 65536);
    }
    int64_t intOf(const Val& v) {
        if (v.t != VT::Num) {
            fail("expected a whole number");
            return 0;
        }
        return v.exact ? qfloor(v.q) : (int64_t)floorf(v.fl);
    }

    // basic transforms
    void qPure(const Val& v, Arc a);
    void qFast(const Pat& x, Q r, Arc a);
    void qShift(const Pat& x, Q t, Arc a);
    void qRev(const Pat& x, Arc a);
    void qCompress(const Pat& x, Q b, Q e, Arc a);
    void qZoom(const Pat& x, Q s, Q e, Arc a);
    void qCat(const Pat* pats, int n, Arc a);
    void qCtl(const Pat& x, uint8_t key, char tag, const Val& v, bool mul, Arc a);
    void qSignal(Fn f, Arc a);

    // tree
    void qNode(uint16_t n, Arc a);
    void qSeq(uint16_t n, Arc a);
    Q seqWeight(uint16_t n);
    void stepInfo(uint16_t c, uint16_t& target, Q& w, int& count);
    void qAtom(uint16_t n, Arc a);
    void qAtomValues(uint16_t n, Arc a);
    void qEuclid(const Pat& x, int64_t k, int64_t n, int64_t r, bool inv, Arc a);

    // functions
    void queryApply(uint16_t fn, const Pat& x, Arc a);
    void callLib(int id, const Pat* args, int nargs, const Pat* extra, uint16_t at, Arc a);
    __attribute__((noinline)) void qApp(uint16_t head, const Pat* extra, uint16_t at, Arc a);
    // Cases with big Pat arrays live outside callLib so its (recursive)
    // frame stays small: the M4 stack is tight.
    __attribute__((noinline)) void qIter(const Pat& x, int64_t k, Arc a, bool back = false);
    __attribute__((noinline)) void qList(Fn fn, uint16_t list, Arc a);
    __attribute__((noinline)) void qEchoLayers(const Pat& x, int64_t count, Q t, float fb, uint8_t gain, Arc a);
    void binop(int id, const Pat& l, const Pat& r, Arc a);
    template <class C>
    void appLeft(const Pat& l, const Pat& r, Arc a, C combine);
    template <class C>
    void appBoth(const Pat& l, const Pat& r, Arc a, C combine);
    template <class C>
    void appRight(const Pat& l, const Pat& r, Arc a, C combine);
    bool combineVals(Fn f, NumOp op, const Ev& l, const Ev& r, Ev& out);
    void sampleAt(const Pat& p, Q t, Val& out, bool& found);
    void qPly(const Pat& x, Q n, bool chop, Arc a);
    void qDegrade(const Pat& x, float p, bool invert, uint32_t seed, Arc a);
    uint32_t miniSeed(uint16_t n);
    void qArp(const Pat& x, const Val& mode, Arc a);
    void qSplice(const Pat& bits, const Pat& idx, const Pat& x, Arc a, bool speed = true);
    int listPats(uint16_t list, Pat* out, int cap);
    int listLen(uint16_t list);
    Pat listAt(uint16_t list, int i) const;
    // batch 2 (2026-11-11)
    __attribute__((noinline)) void callLib2(Fn fn, const Pat* const* all, int n, uint16_t at, Arc a);
    void qWithin(const Pat& fx, const Pat& x, Q lo, Q hi, Arc a);
    void qSqueeze(const Pat& inner, Arc whole, Arc part);
    __attribute__((noinline)) void qChunk(const Pat& x, uint16_t f, int64_t k, bool backward, Arc a);
    void pushRun(int64_t k, Arc a, int64_t c);
    void qSew(const Pat& b, const Pat& x, const Pat& y, Arc a);
    void qPress(const Pat& x, Q r, Arc a);
    __attribute__((noinline)) void qRot(const Pat& x, int64_t k, Arc a);
    void qBite(const Pat& x, int64_t n, const Pat& idx, Arc a);
    __attribute__((noinline)) void qSqueezeList(const Pat& idx, uint16_t list, Arc a);
    __attribute__((noinline)) void qSelect(Fn fn, const Pat& sel, uint16_t list, const Pat* x, Arc a);
    __attribute__((noinline)) void qEchoWith(const Pat& x, int64_t count, Q t, uint16_t f, Arc a);
    __attribute__((noinline)) void qPlyWith(const Pat& x, int64_t count, uint16_t f, Arc a);
    void arpeggiate(int m, bool down);
    __attribute__((noinline)) void qRolled(const Pat& x, Q t, Arc a);
    __attribute__((noinline)) void qToScale(uint16_t list, const Pat& idx, int64_t octave, Arc a);
    __attribute__((noinline)) void qChoose(Fn fn, const Pat* sel, uint16_t list, Arc a);
    bool pairArg(const Pat& p, Val& a, Val& b, uint16_t at);
    __attribute__((noinline)) void qWeighted(Fn fn, uint16_t list, Arc a);
    // batch 3 (2026-11-12)
    __attribute__((noinline)) void callLib3(Fn fn, const Pat* const* all, int n, uint16_t at, Arc a);
    __attribute__((noinline)) void qGhost(const Pat& x, Q t, Arc a);
    __attribute__((noinline)) void qMatch(const Pat& x, const Pat& cond, bool matched, Arc a);
    __attribute__((noinline)) void qFoldEvery(uint16_t list, uint16_t f, const Pat& x, Arc a);
    __attribute__((noinline)) void qChew(const Pat& x, int64_t n, const Pat& idx, Arc a);
    __attribute__((noinline)) void qMapNum(Fn fn, const Pat& lo, const Pat& hi, const Pat& x, Arc a);
    __attribute__((noinline)) void qRandslice(const Pat& x, int64_t n, Arc a);
    bool ctlsMatch(const Ev& cond, const Ev& e) const;
};

void Eval::query(const Pat& p, Arc a) {
    TC_NOTE_SP();
    if (bad()) return;
    if (++depth > kMaxEvalDepth) {
        fail("pattern nested too deeply");
        depth--;
        return;
    }
    switch (p.k) {
        case Pat::Node: qNode(p.node, a); break;
        case Pat::Apply: queryApply(p.node, *p.x, a); break;
        case Pat::Shift: qShift(*p.x, p.q, a); break;
        case Pat::ShiftP: {
            const Pat& x = *p.x;
            bool neg = p.neg;
            withParam(*p.y, a, [&](const Val& v, Arc sub) {
                Q t = timeOf(v);
                qShift(x, neg ? -t : t, sub);
            });
            break;
        }
        case Pat::Fast: qFast(*p.x, p.q, a); break;
        case Pat::Rev: qRev(*p.x, a); break;
        case Pat::Compress: qCompress(*p.x, p.q, p.q2, a); break;
        case Pat::Zoom: qZoom(*p.x, p.q, p.q2, a); break;
        case Pat::Pure: qPure(p.v, a); break;
        case Pat::Overlay:
            query(*p.x, a);
            query(*p.y, a);
            break;
        case Pat::SetCtl: qCtl(*p.x, p.key, p.tag, p.v, false, a); break;
        case Pat::MulCtl: qCtl(*p.x, p.key, p.tag, p.v, true, a); break;
        case Pat::Silence: break;
        case Pat::Split: forCycles(a, [&](Arc sub, int64_t) { query(*p.x, sub); }); break;
        case Pat::Within: qWithin(*p.x, *p.y, p.q, p.q2, a); break;
        case Pat::SetCtlP: {
            const Pat& x = *p.x;
            const uint8_t key = p.key;
            const float mul = qfloat(p.q), add = qfloat(p.q2);
            withParam(*p.y, a, [&](const Val& v, Arc sub) { qCtl(x, key, 'f', numF(add + mul * num(v)), false, sub); });
            break;
        }
        case Pat::Degrade: qDegrade(*p.x, num(p.v), p.neg, 0, a); break;
        case Pat::EveryF: {
            const int64_t k = qfloor(p.q);
            forCycles(a, [&](Arc sub, int64_t c) {
                if (k > 0 && imod(c, k) == 0) queryApply(p.node, *p.x, sub);
                else query(*p.x, sub);
            });
            break;
        }
        case Pat::Match: qMatch(*p.x, *p.y, !p.neg, a); break;
        case Pat::ClipCycles:
            forCycles(a, [&](Arc sub, int64_t c) {
                const int m = top;
                query(*p.x, sub);
                for (int i = m; i < top; i++)
                    if (gArena[i].hasWhole) {
                        gArena[i].whole.s = qmax(gArena[i].whole.s, qint(c));
                        gArena[i].whole.e = qmin(gArena[i].whole.e, qint(c + 1));
                    }
            });
            break;
        case Pat::Inv: {
            const int m = top;
            query(*p.x, a);
            for (int i = m; i < top; i++)
                if (!gArena[i].isMap && gArena[i].v.t == VT::Bool) gArena[i].v.q.n = gArena[i].v.q.n ? 0 : 1;
            break;
        }
    }
    depth--;
}

void Eval::qPure(const Val& v, Arc a) {
    forCycles(a, [&](Arc sub, int64_t c) {
        Ev* e = push();
        if (!e) return;
        e->hasWhole = true;
        e->whole = Arc{qint(c), qint(c + 1)};
        e->part = sub;
        e->isMap = false;
        e->nctl = 0;
        e->v = v;
    });
}

void Eval::qFast(const Pat& x, Q r, Arc a) {
    if (r.n == 0) return;
    if (r.n < 0) {
        Pat f;
        f.k = Pat::Fast;
        f.x = &x;
        f.q = -r;
        qRev(f, a);
        return;
    }
    int m = top;
    query(x, Arc{a.s * r, a.e * r});
    Q inv = Q{1, 1} / r;
    for (int i = m; i < top; i++) mapEvent(gArena[i], inv, Q{0, 1});
}

void Eval::qShift(const Pat& x, Q t, Arc a) {  // rotR t
    int m = top;
    query(x, Arc{a.s - t, a.e - t});
    for (int i = m; i < top; i++) shiftEvent(gArena[i], t);
}

void Eval::qRev(const Pat& x, Arc a) {
    forCycles(a, [&](Arc sub, int64_t c) {
        Q mirror = qint(2 * c + 1);  // t -> 2c + 1 - t
        int m = top;
        query(x, Arc{mirror - sub.e, mirror - sub.s});
        for (int i = m; i < top; i++) {
            Ev& e = gArena[i];
            Arc p = e.part;
            e.part = Arc{mirror - p.e, mirror - p.s};
            if (e.hasWhole) {
                Arc w = e.whole;
                e.whole = Arc{mirror - w.e, mirror - w.s};
            }
        }
    });
}

// Squeeze each cycle of x into [c + b, c + e) of cycle c.
void Eval::qCompress(const Pat& x, Q b, Q e, Arc a) {
    if (!(b < e)) return;
    Q len = e - b, inv = Q{1, 1} / len;
    forCycles(a, [&](Arc sub, int64_t c) {
        Q C = qint(c);
        Arc span{C + b, C + e}, q;
        if (!subArc(sub, span, q)) return;
        if (q.s == q.e && q.s == span.e) return;
        int m = top;
        query(x, Arc{C + (q.s - span.s) * inv, C + (q.e - span.s) * inv});
        for (int i = m; i < top; i++) {
            Ev& ev = gArena[i];
            ev.part.s = C + b + (ev.part.s - C) * len;
            ev.part.e = C + b + (ev.part.e - C) * len;
            if (ev.hasWhole) {
                Q cw = qsam(ev.whole.s);
                ev.whole.s = cw + b + (ev.whole.s - cw) * len;
                ev.whole.e = cw + b + (ev.whole.e - cw) * len;
            }
        }
    });
}

// Play [s, e) of every cycle of x stretched over the whole cycle.
void Eval::qZoom(const Pat& x, Q s, Q e, Arc a) {
    Q d = e - s;
    if (!(d.n > 0)) return;
    forCycles(a, [&](Arc sub, int64_t c) {
        Q C = qint(c);
        int m = top;
        query(x, Arc{C + s + (sub.s - C) * d, C + s + (sub.e - C) * d});
        for (int i = m; i < top; i++) {
            Ev& ev = gArena[i];
            auto back = [&](Arc& r) {
                Q cs = qsam(r.s);
                r.s = cs + (r.s - cs - s) / d;
                r.e = cs + (r.e - cs - s) / d;
            };
            back(ev.part);
            if (ev.hasWhole) back(ev.whole);
        }
    });
}

// cat: cycle c plays pattern c mod n at its own cycle c div n.
void Eval::qCat(const Pat* pats, int n, Arc a) {
    if (n <= 0) return;
    forCycles(a, [&](Arc sub, int64_t c) {
        int64_t i = imod(c, n);
        int64_t k = idivFloor(c, n);
        Q off = qint(c - k);
        int m = top;
        query(pats[i], Arc{sub.s - off, sub.e - off});
        for (int j = m; j < top; j++) shiftEvent(gArena[j], off);
    });
}

// x with control `key` set (or multiplied) by a constant, like
// `x # gain 1` / `x |* gain 0.5`: the constant is a pure pattern, so parts
// split at cycle boundaries.
void Eval::qCtl(const Pat& x, uint8_t key, char tag, const Val& v, bool mul, Arc a) {
    int m = top;
    query(x, a);
    int e = top;
    for (int i = m; i < e && !bad(); i++) {
        Arc w = gArena[i].hasWhole ? gArena[i].whole : gArena[i].part;
        forCycles(w, [&](Arc cyc, int64_t) {
            Arc part;
            if (!subArc(gArena[i].part, cyc, part)) return;
            Ev* o = push();
            if (!o) return;
            *o = gArena[i];
            o->part = part;
            if (!o->isMap) {
                fail("controls need a control pattern");
                return;
            }
            int at = -1;
            const Ctl* rc = ctls(*o);
            for (int k = 0; k < o->nctl; k++)
                if (rc[k].key == key) at = k;
            if (at >= 0 && mul && rc[at].tag == 's') return;  // nothing to multiply
            if (at < 0 && o->nctl >= kMaxCtl) return fail("too many controls on one event");
            Ctl* w = ctlWrite(*o, at < 0 ? 1 : 0);
            if (!w) return;
            if (at >= 0 && mul) {
                w[at].f *= num(v);
            } else if (at >= 0) {
                w[at] = ctlOf(key, tag, v);
            } else {
                w[o->nctl++] = ctlOf(key, tag, v);
            }
        });
    }
    drop(m, e);
}

void Eval::qSignal(Fn f, Arc a) {
    // position in the cycle, exact first so long sets don't lose precision
    float pos = qfloat(a.s - qsam(a.s)), v = 0;
    const float kTwoPi = 6.28318530718f;
    switch (f) {
        case Fn::Sine: v = (sinf(kTwoPi * pos) + 1) / 2; break;
        case Fn::Cosine: v = (sinf(kTwoPi * (pos - 0.25f)) + 1) / 2; break;
        case Fn::Saw: v = pos; break;
        case Fn::Tri: v = pos < 0.5f ? 2 * pos : 2 - 2 * pos; break;
        case Fn::Square: v = pos < 0.5f ? 0 : 1; break;
        case Fn::Isaw: v = 1 - pos; break;
        // the envelopes run once, over the first cycle (absolute time)
        case Fn::EnvL:
        case Fn::EnvLR:
        case Fn::EnvEq:
        case Fn::EnvEqR: {
            float t = qfloat(a.s);
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            const float u = 1 - t, kHalfPi = 1.57079632679f;
            const float w = f == Fn::EnvEq ? sinf(kHalfPi * u) : cosf(kHalfPi * u);
            v = f == Fn::EnvL ? t : f == Fn::EnvLR ? 1 - t : sqrtf(w > 0 ? w : 0);  // (cos rounds just below 0 at 1)
            break;
        }
        default: break;
    }
    Ev* e = push();
    if (!e) return;
    e->hasWhole = false;
    e->part = a;
    e->isMap = false;
    e->nctl = 0;
    e->v = numF(v);
}

// ---- mini-notation -------------------------------------------------------------
void Eval::stepInfo(uint16_t c, uint16_t& target, Q& w, int& count) {
    const Node& n = P.at(c);
    target = c;
    w = Q{1, 1};
    count = 1;
    if (n.kind == Kind::MReplicate) {
        target = n.first;
        count = (int)n.a;
    } else if (n.kind == Kind::MWeight) {
        target = n.first;
        if (n.flags & kUnderscore) {
            w = qint(n.a);
        } else {
            uint16_t arg = P.at(n.first).next;
            w = arg ? Q{(int32_t)P.at(arg).a, (int32_t)P.at(arg).b} : Q{1, 1};
        }
    }
}

Q Eval::seqWeight(uint16_t n) {
    if (P.at(n).kind != Kind::MSeq) return Q{1, 1};
    if (P.at(n).flags & kDots) return qint(P.childCount(n));
    Q W{0, 1};
    for (uint16_t c = P.at(n).first; c; c = P.at(c).next) {
        uint16_t t;
        Q w;
        int k;
        stepInfo(c, t, w, k);
        W = W + w * qint(k);
    }
    return W;
}

void Eval::qSeq(uint16_t n, Arc a) {
    Q W = seqWeight(n);
    if (W.n <= 0) return;
    bool dots = P.at(n).flags & kDots;
    Q cum{0, 1};
    for (uint16_t c = P.at(n).first; c && !bad(); c = P.at(c).next) {
        uint16_t t = c;
        Q w{1, 1};
        int k = 1;
        if (!dots) stepInfo(c, t, w, k);
        Pat x = pNode(t);
        for (int r = 0; r < k && !bad(); r++) {
            qCompress(x, cum / W, (cum + w) / W, a);
            cum = cum + w;
        }
    }
}

void Eval::qAtom(uint16_t n, Arc a) {
    const int m = top;
    qAtomValues(n, a);
    TC_TAG_SRC(m, n);
}

void Eval::qAtomValues(uint16_t n, Arc a) {
    const Node& nd = P.at(n);
    switch (nd.kind) {
        case Kind::MRest: return;
        case Kind::MWord: qPure(strV((uint16_t)nd.a, nd.b ? (int)nd.b - 1 : -1), a); return;
        case Kind::MNum: qPure(numQ(qmake((int32_t)nd.a, (int32_t)nd.b)), a); return;
        case Kind::MNote: qPure(numQ(qint((int32_t)nd.a)), a); return;
        case Kind::MBool: qPure(boolV(nd.a != 0), a); return;
        case Kind::MChord: {
            const char* w = P.str((int)nd.a);
            const char* q = strchr(w, '\'');
            int root = 0;
            if (!q || !parseNoteName(w, (int)(q - w), root)) return fail("bad chord", n);
            char name[16];
            int len = 0;
            for (const char* s = q + 1; *s && *s != '\'' && len < 15; s++) name[len++] = *s;
            name[len] = 0;
            const NamedSet* ch = findSet(kChords, (int)(sizeof kChords / sizeof kChords[0]), name);
            if (!ch) return fail("chord name not supported yet", n);
            if (strchr(q + 1, '\'')) return fail("chord modifiers not supported yet", n);
            for (int i = 0; i < ch->n; i++) qPure(numQ(qint(root + ch->v[i])), a);
            return;
        }
        default: return;
    }
}

void Eval::qEuclid(const Pat& x, int64_t k, int64_t n, int64_t r, bool inv, Arc a) {
    if (n <= 0 || n > 64) return n > 64 ? fail("euclid: at most 64 steps") : void();
    bool neg = k < 0;
    bool bits[64];
    bjorklund((int)(neg ? -k : k), (int)n, bits);
    if (neg) inv = !inv;
    Q rot = qlazy(imod(r, n), n);  // rotate left by r steps
    int m = top;
    Arc q{a.s + rot, a.e + rot};
    for (int i = 0; i < n && !bad(); i++)
        if (bits[i] != inv) qCompress(x, qlazy(i, n), qlazy(i + 1, n), q);
    for (int i = m; i < top; i++) shiftEvent(gArena[i], -rot);
}

void Eval::qNode(uint16_t n, Arc a) {
    TC_NOTE_SP();
    const Node& nd = P.at(n);
    switch (nd.kind) {
        case Kind::Num: qPure(numQ(qmake((int32_t)nd.a, (int32_t)nd.b)), a); return;
        case Kind::Mini: return nd.first ? qNode(nd.first, a) : void();
        case Kind::MSeq: return qSeq(n, a);
        case Kind::MSub:
            for (uint16_t c = nd.first; c && !bad(); c = P.at(c).next) qNode(c, a);
            return;
        case Kind::MAlt:
            for (uint16_t c = nd.first; c && !bad(); c = P.at(c).next) {
                Q W = seqWeight(c);
                if (W.n > 0) qFast(pNode(c), Q{1, 1} / W, a);
            }
            return;
        case Kind::MPoly: {
            uint16_t steps = (nd.flags & kHasSteps) ? P.child(n, P.childCount(n) - 1) : 0;
            auto layers = [&](Q base, Arc sub) {
                for (uint16_t c = nd.first; c && c != steps && !bad(); c = P.at(c).next) {
                    Q W = seqWeight(c);
                    if (W.n > 0) qFast(pNode(c), base / W, sub);
                }
            };
            if (steps) {
                withParam(pNode(steps), a, [&](const Val& v, Arc sub) { layers(timeOf(v), sub); });
            } else if (nd.first) {
                layers(seqWeight(nd.first), a);
            }
            return;
        }
        case Kind::MChoice: {  // one alternative per cycle
            int k = P.childCount(n);
            if (k <= 0) return;
            uint32_t seed = miniSeed(n);
            forCycles(a, [&](Arc sub, int64_t c) {
                int i = (int)(rnd(qint(c), seed) * k);
                qNode(P.child(n, i < k ? i : k - 1), sub);
            });
            return;
        }
        case Kind::MDegrade: {  // x? drops events with probability 0.5 (or x?p)
            uint16_t arg = P.at(nd.first).next;
            float p = arg ? qfloat(qmake((int32_t)P.at(arg).a, (int32_t)P.at(arg).b)) : 0.5f;
            return qDegrade(pNode(nd.first), p, false, miniSeed(n), a);
        }
        case Kind::MFast:
        case Kind::MSlow: {
            Pat x = pNode(nd.first);
            bool slow = nd.kind == Kind::MSlow;
            withParam(pNode(P.at(nd.first).next), a, [&](const Val& v, Arc sub) {
                Q r = timeOf(v);
                if (slow) {
                    if (r.n == 0) return;
                    r = Q{1, 1} / r;
                }
                qFast(x, r, sub);
            });
            return;
        }
        case Kind::MReplicate: {  // only reached outside a sequence
            Pat x = pNode(nd.first);
            for (uint32_t i = 0; i < nd.a; i++) qCompress(x, qlazy(i, nd.a), qlazy(i + 1, nd.a), a);
            return;
        }
        case Kind::MWeight: return qNode(nd.first, a);
        case Kind::MEuclid: {
            Pat x = pNode(nd.first);
            // "t(3,8)" on a Bool pattern: the other steps are f (Tidal's euclidBool)
            const bool boolPat = nd.type == tyPat(Base::Bool);
            const Pat notX = pWrap(Pat::Inv, &x);
            uint16_t kn = P.child(n, 1), nn = P.child(n, 2), rn = P.child(n, 3);
            auto both = [&](int64_t k, int64_t steps, int64_t r, Arc s) {
                qEuclid(x, k, steps, r, false, s);
                if (boolPat) qEuclid(notX, k, steps, r, true, s);
            };
            withParam(pNode(kn), a, [&](const Val& kv, Arc s1) {
                int64_t k = intOf(kv);
                withParam(pNode(nn), s1, [&](const Val& nv, Arc s2) {
                    int64_t steps = intOf(nv);
                    if (!rn) return both(k, steps, 0, s2);
                    withParam(pNode(rn), s2, [&](const Val& rv, Arc s3) { both(k, steps, intOf(rv), s3); });
                });
            });
            return;
        }
        case Kind::MRange: {
            const Node& lo = P.at(nd.first);
            const Node& hi = P.at(lo.next);
            auto val = [](const Node& x) {
                return x.kind == Kind::MNote ? (int64_t)(int32_t)x.a : (int64_t)((int32_t)x.a / (int32_t)x.b);
            };
            int64_t l = val(lo), h = val(hi), cnt = (h >= l ? h - l : l - h) + 1;
            if (cnt > 256) return fail("range too long", n);
            const int m0 = top;
            for (int64_t i = 0; i < cnt && !bad(); i++) {
                Pat v;
                v.k = Pat::Pure;
                v.v = numQ(qint(h >= l ? l + i : l - i));
                qCompress(v, qlazy(i, cnt), qlazy(i + 1, cnt), a);
            }
            TC_TAG_SRC(m0, n);
            return;
        }
        case Kind::MRest:
        case Kind::MWord:
        case Kind::MNum:
        case Kind::MNote:
        case Kind::MChord:
        case Kind::MBool: return qAtom(n, a);
        case Kind::Fn: return callLib((int)nd.a, nullptr, 0, nullptr, n, a);
        case Kind::App: return qApp(nd.first, nullptr, n, a);
        case Kind::Op: return binop((int)nd.a, pNode(nd.first), pNode(P.at(nd.first).next), a);
        case Kind::Hole: return fail("the pattern has a ?? hole", n);
        default: return fail("not a pattern", n);
    }
}

// ---- functions ---------------------------------------------------------------------
// A library function applied to argument nodes (+ the piped-in pattern).
// The argument array is sized to the call (alloca): this frame is on the
// recursion path, and most functions take 1-3 arguments.
void Eval::qApp(uint16_t head, const Pat* extra, uint16_t at, Arc a) {
    if (!head || P.at(head).kind != Kind::Fn) return fail("can't apply this", at);
    int na = 0;
    for (uint16_t c = P.at(head).next; c && na < 8; c = P.at(c).next) na++;
    Pat* args = static_cast<Pat*>(__builtin_alloca(sizeof(Pat) * (na ? na : 1)));
    na = 0;
    for (uint16_t c = P.at(head).next; c && na < 8; c = P.at(c).next) new (&args[na++]) Pat(pNode(c));
    callLib((int)P.at(head).a, args, na, extra, at, a);
}

void Eval::queryApply(uint16_t fn, const Pat& x, Arc a) {
    const Node& f = P.at(fn);
    switch (f.kind) {
        case Kind::Fn: return callLib((int)f.a, nullptr, 0, &x, fn, a);
        case Kind::App: return qApp(f.first, &x, fn, a);
        case Kind::SectionL: return binop((int)f.a, pNode(f.first), x, a);
        case Kind::SectionR: return binop((int)f.a, x, pNode(f.first), a);
        case Kind::Compose: {
            Pat gx;
            gx.k = Pat::Apply;
            gx.node = P.at(f.first).next;
            gx.x = &x;
            return queryApply(f.first, gx, a);
        }
        default: return fail("not a function", fn);
    }
}

// Element count of a list argument (0 and an error if it isn't a list) and
// element i as a pattern: no Pat arrays on the (recursive) stack.
int Eval::listLen(uint16_t list) {
    if (!list || P.at(list).kind != Kind::List) {
        fail("expected a list [a, b]", list);
        return 0;
    }
    return P.childCount(list);
}
Pat Eval::listAt(uint16_t list, int i) const { return pNode(P.child(list, i)); }

int Eval::listPats(uint16_t list, Pat* out, int cap) {
    if (P.at(list).kind != Kind::List) {
        fail("expected a list [a, b]", list);
        return 0;
    }
    int n = 0;
    for (uint16_t c = P.at(list).first; c && n < cap; c = P.at(c).next) out[n++] = pNode(c);
    return n;
}

void Eval::sampleAt(const Pat& p, Q t, Val& out, bool& found) {
    int m = top;
    query(p, Arc{t, t});
    found = top > m;
    if (found) out = gArena[m].v;
    drop(m, top);
}

// Merge the right event's values into the left's, per operator.
bool Eval::combineVals(Fn f, NumOp op, const Ev& l, const Ev& r, Ev& out) {
    if (l.isMap != r.isMap) {
        fail("can't combine a control pattern with a plain pattern");
        return false;
    }
    if (!l.isMap) {
        if (f == Fn::OpUnionR) out.v = r.v;
        else if (f == Fn::OpUnionL) out.v = l.v;
        else if (!numOp(op, l.v, r.v, out.v)) {
            fail("arithmetic needs numbers");
            return false;
        }
        return true;
    }
    // out may be l or r: read their maps by index before rewriting out
    const int ln = l.nctl, rn = r.nctl, la = l.ctlAt, ra = r.ctlAt;
    int at0 = ctlAlloc(ln + rn);
    if (at0 < 0) return false;
    Ctl* w = &gCtl[at0];
    int n = ln;
    for (int i = 0; i < ln; i++) w[i] = gCtl[la + i];
    for (int j = 0; j < rn; j++) {
        const Ctl& rj = gCtl[ra + j];
        int at = -1;
        for (int i = 0; i < n; i++)
            if (w[i].key == rj.key) at = i;
        if (at < 0) {
            if (n >= kMaxCtl) return fail("too many controls on one event"), false;
            w[n++] = rj;
        } else if (f == Fn::OpUnionR) {
            w[at] = rj;
        } else if (f == Fn::OpNum) {
            if (w[at].tag == 's' || rj.tag == 's') return fail("arithmetic needs numbers"), false;
            w[at].f = fop(op, w[at].f, rj.f);
        }
    }
    out.isMap = true;
    out.ctlAt = (uint16_t)at0;
    out.nctl = (uint8_t)n;
    return true;
}

// Structure from the left: right queried over each left whole (or part).
template <class C>
void Eval::appLeft(const Pat& l, const Pat& r, Arc a, C combine) {
    int m = top;
    query(l, a);
    int le = top;
    for (int i = m; i < le && !bad(); i++) {
        Arc qa = gArena[i].hasWhole ? gArena[i].whole : gArena[i].part;
        int r0 = top;
        query(r, qa);
        int r1 = top;
        for (int j = r0; j < r1 && !bad(); j++) {
            Arc part;
            if (!subArc(gArena[i].part, gArena[j].part, part)) continue;
            Ev* o = push();
            if (!o) break;
            o->whole = gArena[i].whole;
            o->hasWhole = gArena[i].hasWhole;
            o->part = part;
            o->isMap = false;
            o->nctl = 0;
            TC_JOIN_SRC(*o, gArena[i], gArena[j]);
            if (!combine(gArena[i], gArena[j], *o)) top--;
        }
        drop(r0, r1);
    }
    drop(m, le);
}

template <class C>
void Eval::appRight(const Pat& l, const Pat& r, Arc a, C combine) {
    int m = top;
    query(r, a);
    int re = top;
    for (int j = m; j < re && !bad(); j++) {
        Arc qa = gArena[j].hasWhole ? gArena[j].whole : gArena[j].part;
        int l0 = top;
        query(l, qa);
        int l1 = top;
        for (int i = l0; i < l1 && !bad(); i++) {
            Arc part;
            if (!subArc(gArena[i].part, gArena[j].part, part)) continue;
            Ev* o = push();
            if (!o) break;
            o->whole = gArena[j].whole;
            o->hasWhole = gArena[j].hasWhole;
            o->part = part;
            o->isMap = false;
            o->nctl = 0;
            TC_JOIN_SRC(*o, gArena[i], gArena[j]);
            if (!combine(gArena[i], gArena[j], *o)) top--;
        }
        drop(l0, l1);
    }
    drop(m, re);
}

template <class C>
void Eval::appBoth(const Pat& l, const Pat& r, Arc a, C combine) {
    int m = top;
    query(l, a);
    int le = top;
    query(r, a);
    int re = top;
    for (int i = m; i < le && !bad(); i++)
        for (int j = le; j < re && !bad(); j++) {
            Arc part, whole{};
            bool hasWhole = gArena[i].hasWhole && gArena[j].hasWhole;
            if (hasWhole && !subArc(gArena[i].whole, gArena[j].whole, whole)) continue;
            if (!subArc(gArena[i].part, gArena[j].part, part)) continue;
            Ev* o = push();
            if (!o) break;
            o->whole = whole;
            o->hasWhole = hasWhole;
            o->part = part;
            o->isMap = false;
            o->nctl = 0;
            TC_JOIN_SRC(*o, gArena[i], gArena[j]);
            if (!combine(gArena[i], gArena[j], *o)) top--;
        }
    drop(m, re);
}

void Eval::binop(int id, const Pat& l, const Pat& r, Arc a) {
    const FnInfo& fi = fnInfo(id);
    switch (fi.fn) {
        case Fn::ShiftL:
        case Fn::ShiftR: {
            withParam(l, a, [&](const Val& v, Arc sub) {
                Q t = timeOf(v);
                qShift(r, fi.fn == Fn::ShiftL ? -t : t, sub);
            });
            return;
        }
        case Fn::OpUnionR:
        case Fn::OpUnionL:
        case Fn::OpNum: {
            Fn f = fi.fn;
            NumOp op = fi.nop;
            auto comb = [&](const Ev& le, const Ev& re, Ev& o) { return combineVals(f, op, le, re, o); };
            if (fi.side == Side::Left) appLeft(l, r, a, comb);
            else if (fi.side == Side::Right) appRight(l, r, a, comb);
            else appBoth(l, r, a, comb);
            return;
        }
        default: return fail("operator not implemented yet");
    }
}

// Each event split into n equal pieces of its whole (ply), or the same
// with begin/end set to the matching slice of the sample (chop).
void Eval::qPly(const Pat& x, Q n, bool chop, Arc a) {
    if (!(n.n > 0)) return;
    int m = top;
    query(x, a);
    int e = top;
    int keyB = controlKey("begin"), keyE = controlKey("end");
    for (int i = m; i < e && !bad(); i++) {
        if (!gArena[i].hasWhole) continue;
        Arc w = gArena[i].whole;
        Q len = w.e - w.s;
        for (int64_t k = 0; qint(k) < n && !bad(); k++) {
            Arc piece{w.s + len * (qint(k) / n), w.s + len * (qint(k + 1) / n)}, part;
            if (!subArc(gArena[i].part, piece, part)) continue;
            if (part.s == part.e && part.s == piece.e) continue;
            Ev* o = push();
            if (!o) return;
            *o = gArena[i];
            o->whole = piece;
            o->part = part;
            if (chop) {
                if (!o->isMap) return fail("chop needs a control pattern");
                // Tidal 1.10.3 slices [begin, end] only when both are set;
                // with one of them alone it slices the whole sample
                float b0 = 0, e0 = 1;
                bool setB = false, setE = false;
                Ctl* w = ctlWrite(*o, 2);
                if (!w) return;
                for (int c = 0; c < o->nctl; c++) {
                    if (w[c].key == keyB) b0 = w[c].f, setB = true;
                    if (w[c].key == keyE) e0 = w[c].f, setE = true;
                }
                if (!(setB && setE)) b0 = 0, e0 = 1;
                float nn = qfloat(n), bb = b0 + (e0 - b0) * (float)k / nn, ee = b0 + (e0 - b0) * (float)(k + 1) / nn;
                bool hb = false, he = false;
                for (int c = 0; c < o->nctl; c++) {
                    if (w[c].key == keyB) w[c].f = bb, hb = true;
                    if (w[c].key == keyE) w[c].f = ee, he = true;
                }
                if (!hb && o->nctl < kMaxCtl) w[o->nctl++] = ctlOf((uint8_t)keyB, 'f', numF(bb));
                if (!he && o->nctl < kMaxCtl) w[o->nctl++] = ctlOf((uint8_t)keyE, 'f', numF(ee));
            }
        }
    }
    drop(m, e);
}

// arp: notes sharing a whole (a chord) are spread across it in turn.
void Eval::qArp(const Pat& x, const Val& mode, Arc a) {
    if (mode.t != VT::Str) return fail("arp mode must be a name");
    const char* name = valStr(P, mode.str);
    bool down = !strcmp(name, "down");
    if (!down && strcmp(name, "up")) return fail("arp mode not supported yet (up, down)");
    const int m = top;
    query(x, a);
    arpeggiate(m, down);
}

// Events from m up: those with the same whole share it in turn (in their
// order, or reversed with `down`): Tidal's arpeggiate.
void Eval::arpeggiate(int m, bool down) {
    int e = top, n = e - m;
    if (n > 256) return fail("too many notes for arp");
    int order[256];
    for (int i = 0; i < n; i++) order[i] = m + i;
    // stable sort by whole
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--) {
            const Ev &p = gArena[order[j - 1]], &q = gArena[order[j]];
            int c = qcmp(p.whole.s, q.whole.s);
            if (c == 0) c = qcmp(p.whole.e, q.whole.e);
            if (c <= 0) break;
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    for (int g = 0; g < n && !bad();) {
        int h = g;
        while (h < n && gArena[order[h]].whole.s == gArena[order[g]].whole.s &&
               gArena[order[h]].whole.e == gArena[order[g]].whole.e)
            h++;
        int d = h - g;
        for (int j = 0; j < d; j++) {
            const Ev& src = gArena[order[down ? h - 1 - j : g + j]];
            if (!src.hasWhole) continue;
            Q dur = (src.whole.e - src.whole.s) / qint(d);
            Arc w{gArena[order[g]].whole.s + dur * qint(j), Q{0, 1}}, part;
            w.e = w.s + dur;
            if (!subArc(w, src.part, part)) continue;
            Ev* o = push();
            if (!o) return;
            *o = src;
            o->whole = w;
            o->part = part;
        }
        g = h;
    }
    drop(m, e);
}

// splice: structure from the index pattern; each event plays slice
// (i mod n) / n of the sample, sped up to fit the event, with unit "c".
// slice (speed = false): the same begin / end, no speed or unit.
void Eval::qSplice(const Pat& bits, const Pat& idx, const Pat& x, Arc a, bool withSpeed) {
    int keyB = controlKey("begin"), keyE = controlKey("end"), keyS = controlKey("speed"), keyU = controlKey("unit");
    int m = top;
    query(idx, a);
    int ie = top;
    for (int i = m; i < ie && !bad(); i++) {
        if (!gArena[i].hasWhole) continue;
        int b0 = top;
        query(bits, gArena[i].whole);
        int b1 = top;
        for (int j = b0; j < b1 && !bad(); j++) {
            Arc part1;
            if (!subArc(gArena[i].part, gArena[j].part, part1)) continue;
            int64_t n = intOf(gArena[j].v), k = intOf(gArena[i].v);
            if (n <= 0) continue;
            float begin = (float)(((k % n) + n) % n) / (float)n, end = begin + 1.0f / (float)n;
            int x0 = top;
            query(x, gArena[i].whole);
            int x1 = top;
            for (int t = x0; t < x1 && !bad(); t++) {
                Arc part2;
                if (!subArc(part1, gArena[t].part, part2)) continue;
                if (!gArena[t].isMap) return fail(withSpeed ? "splice needs a control pattern" : "slice needs a control pattern");
                Q wd = gArena[i].whole.e - gArena[i].whole.s;
                float speed = (1.0f / (float)n) / qfloat(wd);
                // unit "c" is a pure pattern: split at cycle boundaries
                forCycles(gArena[i].whole, [&](Arc cyc, int64_t) {
                    Arc part;
                    if (!subArc(part2, cyc, part)) return;
                    Ev* o = push();
                    if (!o) return;
                    *o = gArena[t];
                    o->whole = gArena[i].whole;
                    o->hasWhole = true;
                    o->part = part;
                    Ctl* w = ctlWrite(*o, 4);
                    if (!w) return;
                    auto set = [&](int key, char tag, const Val& v, bool mulSpeed) {
                        for (int c = 0; c < o->nctl; c++)
                            if (w[c].key == key) {
                                if (mulSpeed) w[c].f *= num(v);
                                else w[c] = ctlOf((uint8_t)key, tag, v);
                                return;
                            }
                        if (o->nctl < kMaxCtl) w[o->nctl++] = ctlOf((uint8_t)key, tag, v);
                    };
                    // begin/end come from the left of `# x`, so x's own win
                    bool hasB = false, hasE = false;
                    for (int c = 0; c < o->nctl; c++) {
                        if (w[c].key == keyB) hasB = true;
                        if (w[c].key == keyE) hasE = true;
                    }
                    if (!hasB) set(keyB, 'f', numF(begin), false);
                    if (!hasE) set(keyE, 'f', numF(end), false);
                    if (withSpeed) {
                        set(keyS, 'f', numF(speed), true);
                        set(keyU, 's', strV(kEngineStr + kStrC), false);
                    }
                });
            }
            drop(x0, x1);
        }
        drop(b0, b1);
    }
    drop(m, ie);
}

// Keep events whose random value (at their whole start) is >= p, or < p
// when inverted (the complement, for sometimesBy).
void Eval::qDegrade(const Pat& x, float p, bool invert, uint32_t seed, Arc a) {
    int m = top;
    query(x, a);
    int w = m;
    for (int i = m; i < top; i++) {
        const Ev& e = gArena[i];
        float r = rnd(e.hasWhole ? e.whole.s : e.part.s, seed);
        if ((r >= p) != invert) gArena[w++] = gArena[i];
    }
    top = w;
}

// Seed of a ? / | node: its position among the ? and | of its mini pattern,
// in source order (so editing elsewhere doesn't reshuffle).
uint32_t Eval::miniSeed(uint16_t n) {
    uint16_t root = n;
    for (uint16_t up = P.parentOf(root); up; up = P.parentOf(root)) {
        root = up;
        if (P.at(root).kind == Kind::Mini) break;
    }
    uint32_t count = 0;
    bool found = false;
    // iterative pre-order walk
    uint16_t stack[kMaxDepth * 4];
    int sp = 0;
    stack[sp++] = root;
    while (sp && !found) {
        uint16_t x = stack[--sp];
        if (x == n) {
            found = true;
            break;
        }
        Kind k = P.at(x).kind;
        if (k == Kind::MDegrade || k == Kind::MChoice) count++;
        // push children in reverse so the first child is visited first
        uint16_t kids[kMaxChildren];
        int nk = 0;
        for (uint16_t c = P.at(x).first; c && nk < kMaxChildren; c = P.at(c).next) kids[nk++] = c;
        for (int i = nk - 1; i >= 0 && sp < kMaxDepth * 4; i--) stack[sp++] = kids[i];
    }
    return count;
}

void Eval::qIter(const Pat& x, int64_t k, Arc a, bool back) {
    Pat rots[64];
    for (int64_t i = 0; i < k; i++) {
        rots[i].k = Pat::Shift;
        rots[i].x = &x;
        rots[i].q = back ? qlazy(i, k) : -qlazy(i, k);  // iter: rotL, iter': rotR
    }
    qCat(rots, (int)k, a);
}

void Eval::qList(Fn fn, uint16_t list, Arc a) {
    Pat items[64];
    int k = listPats(list, items, 64);
    if (fn == Fn::Stack) {
        for (int i = 0; i < k; i++) query(items[i], a);
    } else if (fn == Fn::Fastcat) {
        for (int i = 0; i < k; i++) qCompress(items[i], qlazy(i, k), qlazy(i + 1, k), a);
    } else {
        qCat(items, k, a);
    }
}

// echo count time feedback: each echo shifted by time, gain * fb
void Eval::qEchoLayers(const Pat& x, int64_t count, Q t, float fb, uint8_t gain, Arc a) {
    Pat shifted[32], mul[32], layer[32];
    layer[0] = x;
    for (int64_t e = 1; e < count; e++) {
        shifted[e].k = Pat::Shift;
        shifted[e].x = &layer[e - 1];
        shifted[e].q = t;
        mul[e].k = Pat::MulCtl;
        mul[e].x = &shifted[e];
        mul[e].key = gain;
        mul[e].v = numF(fb);
        layer[e].k = Pat::Overlay;
        layer[e].x = &mul[e];
        layer[e].y = &x;
    }
    query(layer[count > 1 ? count - 1 : 0], a);
}


// ---- batch 2 (2026-11-11): every', inside, sew, squeeze ... -----------------------
// Semantics from Tidal's documentation, checked against GHCi (golden/cases.txt,
// "batch 2"); patterned arguments split queries like Tidal's innerJoin.

// within / playWhen: fx's events whose onset's cycle position is in [lo, hi),
// and x's whose isn't.
void Eval::qWithin(const Pat& fx, const Pat& x, Q lo, Q hi, Arc a) {
    auto in = [&](const Ev& e) {
        Q t = e.hasWhole ? e.whole.s : e.part.s;
        Q pos = t - qsam(t);
        return pos >= lo && pos < hi;
    };
    int m = top;
    query(fx, a);
    int w = m;
    for (int j = m; j < top; j++)
        if (in(gArena[j])) gArena[w++] = gArena[j];
    top = w;
    int m2 = top;
    query(x, a);
    w = m2;
    for (int j = m2; j < top; j++)
        if (!in(gArena[j])) gArena[w++] = gArena[j];
    top = w;
}

// squeezeJoin: the outer event's own cycle of `inner` played inside its whole
// (inner time u in [c, c+1) at whole.s + (u - c) * |whole|), wholes cut to
// the outer whole, parts to the outer part.
void Eval::qSqueeze(const Pat& inner, Arc whole, Arc part) {
    const Q d = whole.e - whole.s;
    if (!(d.n > 0)) return;
    const Q c = qsam(whole.s);
    auto in = [&](Q t) { return c + (t - whole.s) / d; };
    auto out = [&](Q u) { return whole.s + (u - c) * d; };
    const int m = top;
    query(inner, Arc{in(part.s), in(part.e)});
    int w = m;
    for (int i = m; i < top; i++) {
        Ev e = gArena[i];
        Arc p{out(e.part.s), out(e.part.e)}, cut;
        if (!subArc(p, part, cut)) continue;
        e.part = cut;
        if (e.hasWhole) {
            Arc wh{out(e.whole.s), out(e.whole.e)}, wc;
            if (!subArc(wh, whole, wc)) continue;
            e.whole = wc;
        }
        gArena[w++] = e;
    }
    top = w;
}

// chunk / chunk': cycle c is cat's cycle c div k of "within chunk i, f x"
// (chunk' counts the chunks backwards).
void Eval::qChunk(const Pat& x, uint16_t f, int64_t k, bool backward, Arc a) {
    if (k <= 0) return query(x, a);
    forCycles(a, [&](Arc s2, int64_t c) {
        const int64_t j = imod(c, k), i = backward ? k - 1 - j : j;
        const Q off = qint(c - (c - j) / k);
        const Arc loc{s2.s - off, s2.e - off};
        Pat fx;
        fx.k = Pat::Apply;
        fx.node = f;
        fx.x = &x;
        const int m = top;
        qWithin(fx, x, qlazy(i, k), qlazy(i + 1, k), loc);
        for (int t = m; t < top; t++) shiftEvent(gArena[t], off);
    });
}

// run k: 0 .. k-1 in cycle c (inside arc a)
void Eval::pushRun(int64_t k, Arc a, int64_t c) {
    if (k <= 0) return;
    if (k > 1024) return fail("run: at most 1024 steps");
    const Q C = qint(c);
    int64_t j = qfloor((a.s - C) * qint(k));
    if (j < 0) j = 0;
    for (; j < k && !bad(); j++) {
        Arc step{C + qlazy(j, k), C + qlazy(j + 1, k)}, part;
        if (step.s > a.e) break;
        if (!subArc(step, a, part)) continue;
        Ev* e = push();
        if (!e) return;
        e->whole = step;
        e->part = part;
        e->v = numQ(qint(j));
    }
}

static bool truthy(const Val& v) { return v.t == VT::Bool ? v.q.n != 0 : (v.t == VT::Num && num(v) != 0); }

// sew: x where the boolean is true, y where false, both cut to its events
void Eval::qSew(const Pat& b, const Pat& x, const Pat& y, Arc a) {
    const int m = top;
    query(b, a);
    const int be = top;
    for (int i = m; i < be && !bad(); i++) {
        const Arc part = gArena[i].part;
        query(truthy(gArena[i].v) ? x : y, part);
    }
    drop(m, be);
}

// pressBy r: each event starts r of the way through its whole
void Eval::qPress(const Pat& x, Q r, Arc a) {
    const int m = top;
    query(x, a);
    int w = m;
    for (int i = m; i < top; i++) {
        Ev e = gArena[i];
        if (e.hasWhole) {
            Arc nw{e.whole.s + (e.whole.e - e.whole.s) * r, e.whole.e}, np;
            if (!subArc(nw, e.part, np)) continue;
            e.whole = nw;
            e.part = np;
        }
        gArena[w++] = e;
    }
    top = w;
}

// rot k: per cycle, each event takes the value of the event k later
void Eval::qRot(const Pat& x, int64_t k, Arc a) {
    forCycles(a, [&](Arc s2, int64_t c) {
        const int m = top;
        query(x, Arc{qint(c), qint(c + 1)});
        const int e = top, n = e - m;
        if (n == 0) return;
        if (n > 256) return fail("rot: too many events in a cycle");
        int16_t order[256];
        for (int i = 0; i < n; i++) order[i] = (int16_t)(m + i);
        auto start = [&](int i) { const Ev& v = gArena[i]; return v.hasWhole ? v.whole.s : v.part.s; };
        for (int i = 1; i < n; i++)  // stable, by onset
            for (int j = i; j > 0 && start(order[j - 1]) > start(order[j]); j--) {
                int16_t t = order[j];
                order[j] = order[j - 1];
                order[j - 1] = t;
            }
        for (int i = 0; i < n && !bad(); i++) {
            const Ev& src = gArena[order[i]];
            Arc part;
            if (!subArc(src.part, s2, part)) continue;
            const Ev& from = gArena[order[imod(i + k, n)]];
            Ev* o = push();
            if (!o) return;
            *o = src;
            o->part = part;
            o->isMap = from.isMap;
            o->v = from.v;
            o->nctl = from.nctl;
            o->ctlAt = from.ctlAt;  // shared: writers copy first
        }
        drop(m, e);
    });
}

// bite n idx x: each idx event plays slice (i mod n) of x, squeezed in
void Eval::qBite(const Pat& x, int64_t n, const Pat& idx, Arc a) {
    if (n <= 0) return;
    const int m = top;
    query(idx, a);
    const int ie = top;
    for (int i = m; i < ie && !bad(); i++) {
        if (!gArena[i].hasWhole) continue;
        const int64_t k = imod(intOf(gArena[i].v), n);
        Pat z;
        z.k = Pat::Zoom;
        z.x = &x;
        z.q = qlazy(k, n);
        z.q2 = qlazy(k + 1, n);
        qSqueeze(z, gArena[i].whole, gArena[i].part);
    }
    drop(m, ie);
}

// squeeze idx [p0, p1 ..]: each idx event plays p (i mod count), squeezed in
void Eval::qSqueezeList(const Pat& idx, uint16_t list, Arc a) {
    const int n = listLen(list);
    if (n == 0) return;
    const int m = top;
    query(idx, a);
    const int ie = top;
    for (int i = m; i < ie && !bad(); i++)
        if (gArena[i].hasWhole) qSqueeze(listAt(list, (int)imod(intOf(gArena[i].v), n)), gArena[i].whole, gArena[i].part);
    drop(m, ie);
}

// select / pickF / selectF / layer: a list of patterns or functions
void Eval::qSelect(Fn fn, const Pat& sel, uint16_t list, const Pat* x, Arc a) {
    const int n = listLen(list);
    if (n == 0) return;
    if (fn == Fn::Layer) {
        for (int i = 0; i < n && !bad(); i++) queryApply(P.child(list, i), *x, a);
        return;
    }
    withParam(sel, a, [&](const Val& v, Arc sub) {
        int64_t i;
        if (fn == Fn::PickF) {
            i = imod(intOf(v), n);
        } else {  // round (half to even, as Haskell) of f * (count - 1)
            const float f = num(v) * (float)(n - 1);
            const float fl = floorf(f), fr = f - fl;
            i = (int64_t)fl + (fr > 0.5f || (fr == 0.5f && ((int64_t)fl & 1)) ? 1 : 0);
            if (i < 0) i = 0;
            if (i >= n) i = n - 1;
        }
        if (fn == Fn::Select) query(listAt(list, (int)i), sub);
        else queryApply(P.child(list, (int)i), *x, sub);
    });
}

// echoWith count t f x: x + f (t ~> echoWith (count - 1) t f x)
constexpr int kMaxLayers = 8;  // nesting: each layer is a level of recursion
void Eval::qEchoWith(const Pat& x, int64_t count, Q t, uint16_t f, Arc a) {
    if (count <= 1) return query(x, a);
    if (count > kMaxLayers) return fail("echoWith / stutWith: at most 8");
    Pat lv[kMaxLayers], sh[kMaxLayers], ap[kMaxLayers];
    lv[0] = x;
    for (int k = 1; k < count; k++) {
        sh[k].k = Pat::Shift;
        sh[k].x = &lv[k - 1];
        sh[k].q = t;
        ap[k].k = Pat::Apply;
        ap[k].node = f;
        ap[k].x = &sh[k];
        lv[k].k = Pat::Overlay;
        lv[k].x = &x;
        lv[k].y = &ap[k];
    }
    query(lv[count - 1], a);
}

// plyWith count f x: x, f x, f (f x) ... sharing each event in turn
void Eval::qPlyWith(const Pat& x, int64_t count, uint16_t f, Arc a) {
    if (count > kMaxLayers) return fail("plyWith: at most 8");
    Pat lv[kMaxLayers], ap[kMaxLayers];
    lv[0] = x;
    for (int k = 1; k < count; k++) {
        ap[k].k = Pat::Apply;
        ap[k].node = f;
        ap[k].x = &lv[k - 1];
        lv[k].k = Pat::Overlay;
        lv[k].x = &x;
        lv[k].y = &ap[k];
    }
    const int m = top;
    query(lv[count > 1 ? count - 1 : 0], a);
    arpeggiate(m, false);
}

// rolledBy t: events sharing a whole start one after another over t of it
// (t < 0: from the last one)
void Eval::qRolled(const Pat& x, Q t, Arc a) {
    const int m = top;
    query(x, a);
    const int e = top, n = e - m;
    if (n > 256) return fail("rolled: too many notes");
    if (t.n == 0) return;
    const bool back = t.n < 0;
    const Q at = back ? -t : t;
    int16_t order[256];
    for (int i = 0; i < n; i++) order[i] = (int16_t)(m + i);
    auto key = [&](int i, int j) {  // stable sort by whole
        int c = qcmp(gArena[i].whole.s, gArena[j].whole.s);
        return c ? c : qcmp(gArena[i].whole.e, gArena[j].whole.e);
    };
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && key(order[j - 1], order[j]) > 0; j--) {
            int16_t s = order[j];
            order[j] = order[j - 1];
            order[j - 1] = s;
        }
    for (int g = 0; g < n;) {
        int h = g + 1;
        while (h < n && key(order[g], order[h]) == 0) h++;
        const int cnt = h - g;
        for (int j = 0; j < cnt; j++) {
            Ev& ev = gArena[order[g + j]];
            if (!ev.hasWhole) continue;
            const int k = back ? cnt - 1 - j : j;
            const Q shift = (ev.whole.e - ev.whole.s) * at * qlazy(k, cnt);
            Arc nw{ev.whole.s + shift, ev.whole.e}, np;
            if (!subArc(nw, ev.part, np)) {
                ev.part = Arc{Q{1, 1}, Q{0, 1}};  // dropped below
                continue;
            }
            ev.whole = nw;
            ev.part = np;
        }
        g = h;
    }
    int w = m;
    for (int i = m; i < e; i++)
        if (!(gArena[i].part.s > gArena[i].part.e)) gArena[w++] = gArena[i];
    top = w;
}

// toScale' octave [degrees] idx
void Eval::qToScale(uint16_t list, const Pat& idx, int64_t octave, Arc a) {
    const int n = listLen(list);
    if (n == 0) return;
    if (n > 32) return fail("toScale: at most 32 degrees");
    Val deg[32];
    for (int i = 0; i < n; i++) {
        bool found;
        sampleAt(listAt(list, i), Q{0, 1}, deg[i], found);
        if (!found) return fail("toScale: a scale degree must be a number");
    }
    const int m = top;
    query(idx, a);
    for (int i = m; i < top && !bad(); i++) {
        Ev& e = gArena[i];
        if (e.isMap) return fail("toScale: a pattern of numbers");
        const int64_t k = intOf(e.v);
        numOp(NumOp::Add, deg[imod(k, n)], numQ(qint(idivFloor(k, n) * octave)), e.v);
    }
}


// randcat / choose / chooseBy / cycleChoose (Tidal: [a]; literals are
// patterns here, read at time 0)
void Eval::qChoose(Fn fn, const Pat* sel, uint16_t list, Arc a) {
    const int k = listLen(list);
    if (k == 0) return;
    if (fn == Fn::Randcat) {  // a random pattern of the list each cycle
        forCycles(a, [&](Arc s2, int64_t c) { query(listAt(list, (int)(rnd(qint(c), 5) * (float)k) % k), s2); });
        return;
    }
    auto pick = [&](float r, Val& out) {
        const int i = (int)imod((int64_t)floorf(r * (float)k), k);  // wraps, as Tidal's !!!
        bool found;
        sampleAt(listAt(list, i), Q{0, 1}, out, found);
        return found;
    };
    if (fn == Fn::CycleChoose) {  // one value per cycle
        forCycles(a, [&](Arc s2, int64_t c) {
            Val v;
            if (!pick(rnd(qint(c), 0), v)) return;
            Ev* e = push();
            if (!e) return;
            e->whole = Arc{qint(c), qint(c + 1)};
            e->part = s2;
            e->v = v;
        });
        return;
    }
    auto signal = [&](float r, Arc s) {  // continuous, like rand
        Val v;
        if (!pick(r, v)) return;
        Ev* e = push();
        if (!e) return;
        e->hasWhole = false;
        e->part = s;
        e->v = v;
    };
    if (fn == Fn::Choose) return signal(rnd(a.s, 0), a);
    withParam(*sel, a, [&](const Val& v, Arc sub) { signal(num(v), sub); });
}


// A pair argument (0, 0.5): both values, read at time 0 (Tidal: plain
// values; literals are patterns here).
bool Eval::pairArg(const Pat& p, Val& a, Val& b, uint16_t at) {
    if (p.k != Pat::Node || P.at(p.node).kind != Kind::Tuple) {
        fail("expected a pair (a, b)", at);
        return false;
    }
    const uint16_t f = P.at(p.node).first;
    bool fa, fb;
    sampleAt(pNode(f), Q{0, 1}, a, fa);
    sampleAt(pNode(P.at(f).next), Q{0, 1}, b, fb);
    if (!fa || !fb) fail("a pair needs two values", at);
    return fa && fb;
}

// wchoose [(value, weight)] (continuous, like rand) / wrandcat
// [(pattern, weight)] (one per cycle): weighted random picks
void Eval::qWeighted(Fn fn, uint16_t list, Arc a) {
    const int k = listLen(list);
    if (k == 0) return;
    if (k > 32) return fail("at most 32 choices");
    float w[32], total = 0;
    for (int i = 0; i < k; i++) {
        Val v0, v1;
        if (!pairArg(listAt(list, i), v0, v1, list)) return;
        w[i] = num(v1) > 0 ? num(v1) : 0;
        total += w[i];
    }
    if (!(total > 0)) return;
    auto pick = [&](float r) {
        float x = r * total;
        for (int i = 0; i < k; i++) {
            if (x < w[i]) return i;
            x -= w[i];
        }
        return k - 1;
    };
    auto item = [&](int i) { return pNode(P.at(P.child(list, i)).first); };
    if (fn == Fn::WRandcat) {
        forCycles(a, [&](Arc s2, int64_t c) { query(item(pick(rnd(qint(c), 5))), s2); });
        return;
    }
    Val v;
    bool found;
    sampleAt(item(pick(rnd(a.s, 0))), Q{0, 1}, v, found);
    if (!found) return;
    Ev* e = push();
    if (!e) return;
    e->hasWhole = false;
    e->part = a;
    e->v = v;
}

void Eval::callLib2(Fn fn, const Pat* const* all, int n, uint16_t at, Arc a) {
    auto need = [&](int k) {
        if (n < k) fail("function is missing arguments", at);
        return n >= k;
    };
    auto fnArg = [&](int i) -> uint16_t {
        if (all[i]->k != Pat::Node) {
            fail("expected a function", at);
            return 0;
        }
        return all[i]->node;
    };
    auto listArg = [&](int i) -> uint16_t { return all[i]->k == Pat::Node ? all[i]->node : 0; };
    switch (fn) {
        case Fn::EveryP: {  // every' n o f: f when cycle mod n == o
            if (!need(4)) return;
            const uint16_t f = fnArg(2);
            const Pat& x = *all[3];
            withParam(*all[0], a, [&](const Val& nv, Arc s1) {
                const int64_t k = intOf(nv);
                withParam(*all[1], s1, [&](const Val& ov, Arc s2) {
                    const int64_t o = intOf(ov);
                    forCycles(s2, [&](Arc s3, int64_t c) {
                        if (k > 0 && imod(c, k) == o) queryApply(f, x, s3);
                        else query(x, s3);
                    });
                });
            });
            return;
        }
        case Fn::Inside:
        case Fn::Outside: {  // inside n f x = fast n (f (slow n x))
            if (!need(3)) return;
            const uint16_t f = fnArg(1);
            const Pat& x = *all[2];
            const bool out = fn == Fn::Outside;
            withParam(*all[0], a, [&](const Val& v, Arc sub) {
                Q r = timeOf(v);
                if (r.n == 0) return;
                if (out) r = Q{1, 1} / r;
                Pat slow;
                slow.k = Pat::Fast;
                slow.x = &x;
                slow.q = Q{1, 1} / r;
                const Pat split = pWrap(Pat::Split, &slow);
                Pat fx;
                fx.k = Pat::Apply;
                fx.node = f;
                fx.x = &split;
                qFast(fx, r, sub);
            });
            return;
        }
        case Fn::Trunc: {
            if (!need(2)) return;
            const Pat& x = *all[1];
            withParam(*all[0], a, [&](const Val& v, Arc sub) {
                const Q r = timeOf(v);
                forCycles(sub, [&](Arc s2, int64_t c) {
                    Arc q;
                    if (subArc(s2, Arc{qint(c), qint(c) + r}, q)) query(x, q);
                });
            });
            return;
        }
        case Fn::FastGap: {
            if (!need(2)) return;
            const Pat& x = *all[1];
            withParam(*all[0], a, [&](const Val& v, Arc sub) {
                const Q r = timeOf(v);
                if (r.n > 0) qCompress(x, Q{0, 1}, qmin(Q{1, 1}, Q{1, 1} / r), sub);
            });
            return;
        }
        case Fn::Wedge: {
            if (!need(3)) return;
            withParam(*all[0], a, [&](const Val& v, Arc sub) {
                const Q r = timeOf(v);
                if (r <= Q{0, 1}) return query(*all[2], sub);
                if (r >= Q{1, 1}) return query(*all[1], sub);
                qCompress(*all[1], Q{0, 1}, r, sub);
                qCompress(*all[2], r, Q{1, 1}, sub);
            });
            return;
        }
        case Fn::ChunkR: {
            if (!need(3)) return;
            const uint16_t f = fnArg(1);
            const Pat& x = *all[2];
            withParam(*all[0], a, [&](const Val& v, Arc sub) { qChunk(x, f, intOf(v), true, sub); });
            return;
        }
        case Fn::Run: {
            if (!need(1)) return;
            withParam(*all[0], a, [&](const Val& v, Arc sub) {
                // [0 .. n-1]: Haskell's enumFromTo goes on to n - 1/2
                const int64_t k = v.exact ? qfloor(v.q + Q{1, 2}) : (int64_t)floorf(num(v) + 0.5f);
                forCycles(sub, [&](Arc s2, int64_t c) { pushRun(k, s2, c); });
            });
            return;
        }
        case Fn::Scan: {
            if (!need(1)) return;
            withParam(*all[0], a, [&](const Val& v, Arc sub) {
                const int64_t k = intOf(v);
                if (k <= 0) return;
                forCycles(sub, [&](Arc s2, int64_t c) { pushRun(imod(c, k) + 1, s2, c); });
            });
            return;
        }
        case Fn::Inv: {
            if (!need(1)) return;
            const Pat iv = pWrap(Pat::Inv, all[0]);
            return query(iv, a);
        }
        case Fn::Sew:
            if (need(3)) qSew(*all[0], *all[1], *all[2], a);
            return;
        case Fn::While: {
            if (!need(3)) return;
            Pat fx;
            fx.k = Pat::Apply;
            fx.node = fnArg(1);
            fx.x = all[2];
            return qSew(*all[0], fx, *all[2], a);
        }
        case Fn::Stitch: {
            if (!need(3)) return;
            for (int want = 1; want >= 0; want--)
                appLeft(*all[0], want ? *all[1] : *all[2], a, [&](const Ev& l, const Ev& r, Ev& o) {
                    if (truthy(l.v) != (want == 1)) return false;
                    o.isMap = r.isMap;
                    o.v = r.v;
                    o.nctl = r.nctl;
                    o.ctlAt = r.ctlAt;  // shared: writers copy first
                    return true;
                });
            return;
        }
        case Fn::EuclidOff: {
            if (!need(4)) return;
            const Pat& x = *all[3];
            withParam(*all[0], a, [&](const Val& kv, Arc s1) {
                withParam(*all[1], s1, [&](const Val& nv, Arc s2) {
                    withParam(*all[2], s2,
                              [&](const Val& rv, Arc s3) { qEuclid(x, intOf(kv), intOf(nv), intOf(rv), false, s3); });
                });
            });
            return;
        }
        case Fn::Press:
            if (need(1)) qPress(*all[0], Q{1, 2}, a);
            return;
        case Fn::PressBy: {
            if (!need(2)) return;
            const Pat& x = *all[1];
            withParam(*all[0], a, [&](const Val& v, Arc sub) { qPress(x, timeOf(v), sub); });
            return;
        }
        case Fn::SwingBy:
        case Fn::Swing: {  // swingBy x n = inside n (within (0.5, 1) (x ~>))
            const bool by = fn == Fn::SwingBy;
            if (!need(by ? 3 : 2)) return;
            Pat third;
            third.k = Pat::Pure;
            third.v = numQ(Q{1, 3});
            const Pat& amount = by ? *all[0] : third;
            const Pat& x = *all[by ? 2 : 1];
            withParam(*all[by ? 1 : 0], a, [&](const Val& v, Arc sub) {
                const Q r = timeOf(v);
                if (r.n == 0) return;
                Pat slow;
                slow.k = Pat::Fast;
                slow.x = &x;
                slow.q = Q{1, 1} / r;
                const Pat split = pWrap(Pat::Split, &slow);
                Pat sh;
                sh.k = Pat::ShiftP;
                sh.x = &split;
                sh.y = &amount;
                Pat w;
                w.k = Pat::Within;
                w.x = &sh;
                w.y = &split;
                w.q = Q{1, 2};
                w.q2 = Q{1, 1};
                qFast(w, r, sub);
            });
            return;
        }
        case Fn::Rot: {
            if (!need(2)) return;
            const Pat& x = *all[1];
            withParam(*all[0], a, [&](const Val& v, Arc sub) { qRot(x, intOf(v), sub); });
            return;
        }
        case Fn::Bite: {
            if (!need(3)) return;
            withParam(*all[0], a, [&](const Val& v, Arc sub) { qBite(*all[2], intOf(v), *all[1], sub); });
            return;
        }
        case Fn::Squeeze:
            if (need(2)) qSqueezeList(*all[0], listArg(1), a);
            return;
        case Fn::Select:
            if (need(2)) qSelect(fn, *all[0], listArg(1), nullptr, a);
            return;
        case Fn::PickF:
        case Fn::SelectF:
            if (need(3)) qSelect(fn, *all[0], listArg(1), all[2], a);
            return;
        case Fn::Layer:
            if (need(2)) qSelect(fn, *all[1], listArg(0), all[1], a);
            return;
        case Fn::JuxBy: {  // x # pan (0.5 - n/2), f (x # pan (0.5 + n/2))
            if (!need(3)) return;
            const uint16_t f = fnArg(1);
            Pat l;
            l.k = Pat::SetCtlP;
            l.x = all[2];
            l.y = all[0];
            l.key = (uint8_t)controlKey("pan");
            l.q = Q{-1, 2};
            l.q2 = Q{1, 2};
            Pat r = l;
            r.q = Q{1, 2};
            query(l, a);
            return queryApply(f, r, a);
        }
        case Fn::EchoWith: {
            if (!need(4)) return;
            const uint16_t f = fnArg(2);
            const Pat& x = *all[3];
            withParam(*all[0], a, [&](const Val& cv, Arc s1) {
                withParam(*all[1], s1, [&](const Val& tv, Arc s2) { qEchoWith(x, intOf(cv), timeOf(tv), f, s2); });
            });
            return;
        }
        case Fn::PlyWith: {
            if (!need(3)) return;
            const uint16_t f = fnArg(1);
            const Pat& x = *all[2];
            withParam(*all[0], a, [&](const Val& v, Arc sub) {
                // compound n = x + f (compound (n - 1)) while n > 1: ceil(n) layers
                const int64_t k = v.exact ? -qfloor(-v.q) : (int64_t)ceilf(num(v));
                qPlyWith(x, k < 1 ? 1 : k, f, sub);
            });
            return;
        }
        case Fn::Rolled:
            if (need(1)) qRolled(*all[0], Q{1, 4}, a);
            return;
        case Fn::RolledBy: {
            if (!need(2)) return;
            const Pat& x = *all[1];
            withParam(*all[0], a, [&](const Val& v, Arc sub) { qRolled(x, timeOf(v), sub); });
            return;
        }
        case Fn::ToScale:
            if (need(2)) qToScale(listArg(0), *all[1], 12, a);
            return;
        case Fn::ToScaleP: {
            if (!need(3)) return;
            Val o;
            bool found;
            sampleAt(*all[0], Q{0, 1}, o, found);
            if (!found) return fail("toScale': the octave must be a number", at);
            qToScale(listArg(1), *all[2], intOf(o), a);
            return;
        }
        case Fn::Degrade:
            if (need(1)) qDegrade(*all[0], 0.5f, false, 0, a);
            return;
        case Fn::UnDegradeBy: {
            if (!need(2)) return;
            const Pat& x = *all[1];
            withParam(*all[0], a, [&](const Val& v, Arc sub) { qDegrade(x, num(v), true, 0, sub); });
            return;
        }
        case Fn::SometimesByP: {
            if (!need(3)) return;
            Pat fx;
            fx.k = Pat::Apply;
            fx.node = fnArg(1);
            fx.x = all[2];
            const Pat& x = *all[2];
            withParam(*all[0], a, [&](const Val& v, Arc sub) {
                qDegrade(x, num(v), false, 0, sub);
                qDegrade(fx, num(v), true, 0, sub);
            });
            return;
        }
        case Fn::SomeCycles: {
            if (!need(2)) return;
            const uint16_t f = fnArg(0);
            forCycles(a, [&](Arc s2, int64_t c) {
                if (rnd(qint(c), 0) < 0.5f) queryApply(f, *all[1], s2);
                else query(*all[1], s2);
            });
            return;
        }
        case Fn::Shuffle:
        case Fn::Scramble: {
            if (!need(2)) return;
            const bool perm = fn == Fn::Shuffle;
            const Pat& x = *all[1];
            withParam(*all[0], a, [&](const Val& v, Arc sub) {
                const int64_t k = intOf(v);
                if (k <= 0) return;
                if (k > 64) return fail("shuffle / scramble: at most 64 parts", at);
                forCycles(sub, [&](Arc s2, int64_t c) {
                    uint8_t order[64];  // shuffle: a permutation per cycle (Fisher-Yates)
                    for (int i = 0; i < k; i++) order[i] = (uint8_t)i;
                    if (perm)
                        for (int i = (int)k - 1; i > 0; i--) {
                            const int j = (int)(rnd(qint(c) + qlazy(i, k), 7) * (float)(i + 1));
                            const uint8_t t = order[i];
                            order[i] = order[j > i ? i : j];
                            order[j > i ? i : j] = t;
                        }
                    const Q C = qint(c);
                    for (int i = 0; i < k && !bad(); i++) {
                        Arc slot{C + qlazy(i, k), C + qlazy(i + 1, k)}, part;
                        if (!subArc(slot, s2, part)) continue;
                        const int64_t pick = perm ? order[i] : (int64_t)(rnd(slot.s, 7) * (float)k);
                        Pat z;
                        z.k = Pat::Zoom;
                        z.x = &x;
                        z.q = qlazy(pick, k);
                        z.q2 = qlazy(pick + 1, k);
                        qSqueeze(z, slot, part);
                    }
                });
            });
            return;
        }
        case Fn::Perlin: {  // smooth noise: random values at whole cycles, eased between
            const int64_t i = qfloor(a.s);
            const float f = qfloat(a.s - qint(i)), r0 = rnd(qint(i), 3), r1 = rnd(qint(i + 1), 3);
            const float ease = f * f * f * (f * (f * 6 - 15) + 10);
            Ev* e = push();
            if (!e) return;
            e->hasWhole = false;
            e->part = a;
            e->v = numF(r0 + ease * (r1 - r0));
            return;
        }
        case Fn::Within:
        case Fn::WithinP:
        case Fn::Zoom:
        case Fn::Compress: {
            const bool fnTaking = fn == Fn::Within || fn == Fn::WithinP;
            if (!need(fnTaking ? 3 : 2)) return;
            Val v0, v1;
            if (!pairArg(*all[0], v0, v1, at)) return;
            const Q s0 = timeOf(v0), e0 = timeOf(v1);
            const Pat& x = *all[fnTaking ? 2 : 1];
            if (fn == Fn::Zoom) return qZoom(x, s0, e0, a);
            if (fn == Fn::Compress) {  // silence outside 0 <= s <= e <= 1
                if (s0 < Q{0, 1} || e0 > Q{1, 1} || s0 > e0) return;
                return qCompress(x, s0, e0, a);
            }
            Pat fx;
            fx.k = Pat::Apply;
            fx.node = fnArg(1);
            if (fn == Fn::Within) {  // f where the onset's cycle position is in [s, e)
                fx.x = &x;
                return qWithin(fx, x, s0, e0, a);
            }
            // within': f sees only that part, stretched to a cycle
            Pat z;
            z.k = Pat::Zoom;
            z.x = &x;
            z.q = s0;
            z.q2 = e0;
            fx.x = &z;
            Pat c;
            c.k = Pat::Compress;
            c.x = &fx;
            c.q = s0;
            c.q2 = e0;
            return qWithin(c, x, s0, e0, a);
        }
        case Fn::WChoose:
        case Fn::WRandcat:
            if (need(1)) qWeighted(fn, listArg(0), a);
            return;
        case Fn::Randcat:
        case Fn::Choose:
        case Fn::CycleChoose:
            if (need(1)) qChoose(fn, nullptr, listArg(0), a);
            return;
        case Fn::ChooseBy:
            if (need(2)) qChoose(fn, all[0], listArg(1), a);
            return;
        default: return fail("not implemented", at);
    }
}

void Eval::callLib(int id, const Pat* args, int nargs, const Pat* extra, uint16_t at, Arc a) {
    TC_NOTE_SP();
    const FnInfo& fi = fnInfo(id);
    // the arguments are read in place (no 9 x Pat copy in this recursive frame)
    struct ArgView {
        const Pat* p[9];
        const Pat& operator[](int i) const { return *p[i]; }
    } all;
    static const Pat kNone;  // unused slots read as silence, as before
    for (const Pat*& q : all.p) q = &kNone;
    int n = 0;
    for (int i = 0; i < nargs && n < 8; i++) all.p[n++] = &args[i];
    if (extra) all.p[n++] = extra;
    auto need = [&](int k) {
        if (n < k) fail("function is missing arguments", at);
        return n >= k;
    };
    auto fnArg = [&](int i) -> uint16_t {
        if (all[i].k != Pat::Node) {
            fail("expected a function", at);
            return 0;
        }
        return all[i].node;
    };
    switch (fi.fn) {
        case Fn::Silence: return;
        case Fn::Control: {
            if (!need(1) || fi.key < 0) return fi.key < 0 ? fail("control not supported", at) : void();
            int m = top;
            query(all[0], a);
            uint8_t key = (uint8_t)fi.key;
            char tag = controlDef(key).tag;
            int keyN = controlKey("n");
            for (int i = m; i < top; i++) {
                Ev& e = gArena[i];
                if (e.isMap) return fail("control of a control pattern", at);
                Val v = e.v;
                bool withN = v.t == VT::Str && v.idx >= 0 && controlDef(key).tag == 's';
                int c0 = ctlAlloc(withN ? 2 : 1);
                if (c0 < 0) return;
                e.isMap = true;
                e.ctlAt = (uint16_t)c0;
                e.nctl = 0;
                gCtl[c0 + e.nctl++] = ctlOf(key, tag, v);
                if (withN) gCtl[c0 + e.nctl++] = ctlOf((uint8_t)keyN, 'f', numQ(qint(v.idx)));
            }
            return;
        }
        case Fn::Fast:
        case Fn::Slow:
        case Fn::Hurry: {
            if (!need(2)) return;
            Fn f = fi.fn;
            const Pat& x = all[1];
            withParam(all[0], a, [&](const Val& v, Arc sub) {
                Q r = timeOf(v);
                if (f == Fn::Slow) {
                    if (r.n == 0) return;
                    r = Q{1, 1} / r;
                }
                if (f != Fn::Hurry) return qFast(x, r, sub);
                Pat fx;
                fx.k = Pat::Fast;
                fx.x = &x;
                fx.q = r;
                qCtl(fx, (uint8_t)controlKey("speed"), 'f', numQ(r), true, sub);
            });
            return;
        }
        case Fn::Rev:
            if (need(1)) qRev(all[0], a);
            return;
        case Fn::Palindrome: {
            if (!need(1)) return;
            Pat pr[2] = {all[0], pWrap(Pat::Rev, &all[0])};
            return qCat(pr, 2, a);
        }
        case Fn::Brak: {
            if (!need(1)) return;
            Pat half;
            half.k = Pat::Compress;
            half.x = &all[0];
            half.q = Q{0, 1};
            half.q2 = Q{1, 2};
            Pat sh;
            sh.k = Pat::Shift;
            sh.x = &half;
            sh.q = Q{1, 4};
            forCycles(a, [&](Arc sub, int64_t c) { query((c % 2 != 0) ? sh : all[0], sub); });
            return;
        }
        case Fn::Iter: {
            if (!need(2)) return;
            const Pat& x = all[1];
            withParam(all[0], a, [&](const Val& v, Arc sub) {
                int64_t k = intOf(v);
                if (k == 0) return query(x, sub);
                if (k < 0 || k > 64) return fail("iter: 1..64 supported", at);
                qIter(x, k, sub);
            });
            return;
        }
        case Fn::Ply:
        case Fn::Chop: {
            if (!need(2)) return;
            bool chop = fi.fn == Fn::Chop;
            const Pat& x = all[1];
            withParam(all[0], a, [&](const Val& v, Arc sub) { qPly(x, timeOf(v), chop, sub); });
            return;
        }
        case Fn::Linger: {
            if (!need(2)) return;
            const Pat& x = all[1];
            withParam(all[0], a, [&](const Val& v, Arc sub) {
                Q r = timeOf(v);
                if (r.n == 0) return;
                Pat z;
                z.k = Pat::Zoom;
                z.x = &x;
                if (r.n > 0) {
                    z.q = Q{0, 1};
                    z.q2 = r;
                } else {
                    z.q = Q{1, 1} + r;
                    z.q2 = Q{1, 1};
                }
                qFast(z, Q{1, 1} / (r.n > 0 ? r : -r), sub);
            });
            return;
        }
        case Fn::Segment: {
            if (!need(2)) return;
            const Pat& x = all[1];
            withParam(all[0], a, [&](const Val& v, Arc sub) {
                Pat unit;
                unit.k = Pat::Pure;
                unit.v = boolV(true);
                Pat grid;
                grid.k = Pat::Fast;
                grid.x = &unit;
                grid.q = timeOf(v);
                appLeft(grid, x, sub, [&](const Ev&, const Ev& r, Ev& o) {
                    o.isMap = r.isMap;
                    o.v = r.v;
                    o.nctl = r.nctl;
                    o.ctlAt = r.ctlAt;  // shared: writers copy first
                    return true;
                });
            });
            return;
        }
        case Fn::Stack:
        case Fn::Fastcat:
        case Fn::Cat:
        case Fn::Slowcat: {
            if (!need(1) || all[0].k != Pat::Node) return;
            qList(fi.fn, all[0].node, a);
            return;
        }
        case Fn::Overlay:
            if (need(2)) query(all[0], a), query(all[1], a);
            return;
        case Fn::Every: {
            if (!need(3)) return;
            uint16_t f = fnArg(1);
            const Pat& x = all[2];
            withParam(all[0], a, [&](const Val& v, Arc sub) {
                int64_t k = intOf(v);
                forCycles(sub, [&](Arc s2, int64_t c) {
                    if (k > 0 && c % k == 0) queryApply(f, x, s2);
                    else query(x, s2);
                });
            });
            return;
        }
        case Fn::Whenmod: {
            if (!need(4)) return;
            uint16_t f = fnArg(2);
            const Pat& x = all[3];
            withParam(all[0], a, [&](const Val& av, Arc s1) {
                Q m = timeOf(av);
                withParam(all[1], s1, [&](const Val& bv, Arc s2) {
                    Q b = timeOf(bv);
                    forCycles(s2, [&](Arc s3, int64_t) {
                        Q t = s3.s;
                        bool on = m.n != 0 && (t - m * qsam(t / m)) >= b;
                        if (on) queryApply(f, x, s3);
                        else query(x, s3);
                    });
                });
            });
            return;
        }
        case Fn::Range: {
            if (!need(3)) return;
            int m = top;
            query(all[2], a);
            for (int i = m; i < top && !bad(); i++) {
                Val lo, hi;
                bool f1, f2;
                Q t = gArena[i].part.s;
                int saveTop = top;
                sampleAt(all[0], t, lo, f1);
                sampleAt(all[1], t, hi, f2);
                top = saveTop;
                if (!f1 || !f2) continue;
                Val d, s, r;
                numOp(NumOp::Sub, hi, lo, d);
                numOp(NumOp::Mul, d, gArena[i].v, s);
                numOp(NumOp::Add, s, lo, r);
                gArena[i].v = r;
            }
            return;
        }
        case Fn::Struct:
        case Fn::Mask: {
            if (!need(2)) return;
            bool st = fi.fn == Fn::Struct;
            auto truthy = [](const Val& v) { return v.t == VT::Bool ? v.q.n != 0 : (v.t == VT::Num && num(v) != 0); };
            auto comb = [&](const Ev& l, const Ev& r, Ev& o) {
                const Ev& keepFrom = st ? r : l;
                if (!truthy(st ? l.v : r.v)) return false;
                o.isMap = keepFrom.isMap;
                o.v = keepFrom.v;
                o.nctl = keepFrom.nctl;
                o.ctlAt = keepFrom.ctlAt;  // shared: writers copy first
                return true;
            };
            if (st) appLeft(all[0], all[1], a, comb);
            else appLeft(all[1], all[0], a, comb);
            return;
        }
        case Fn::Euclid:
        case Fn::EuclidInv:
        case Fn::EuclidFull: {
            bool full = fi.fn == Fn::EuclidFull;
            if (!need(full ? 4 : 3)) return;
            bool inv = fi.fn == Fn::EuclidInv;
            withParam(all[0], a, [&](const Val& kv, Arc s1) {
                int64_t k = intOf(kv);
                withParam(all[1], s1, [&](const Val& nv, Arc s2) {
                    int64_t steps = intOf(nv);
                    qEuclid(all[2], k, steps, 0, inv, s2);
                    if (full) qEuclid(all[3], k, steps, 0, true, s2);
                });
            });
            return;
        }
        case Fn::DegradeBy: {
            if (!need(2)) return;
            const Pat& x = all[1];
            withParam(all[0], a, [&](const Val& v, Arc sub) { qDegrade(x, num(v), false, 0, sub); });
            return;
        }
        case Fn::SometimesBy:
        case Fn::Sometimes: {
            // degradeBy p x, plus f applied to the events degradeBy dropped
            // (sometimesBy' picks after f instead: callLib2)
            int pi = fi.fn == Fn::SometimesBy ? 1 : 0;
            if (!need(pi + 2)) return;
            uint16_t f = fnArg(pi);
            const Pat& x = all[pi + 1];
            auto run = [&](float p, Arc sub) {
                Pat rest;
                rest.k = Pat::Degrade;
                rest.x = &x;
                rest.v = numF(p);
                rest.neg = true;
                Pat fx;
                fx.k = Pat::Apply;
                fx.node = f;
                fx.x = &rest;
                qDegrade(x, p, false, 0, sub);
                query(fx, sub);
            };
            if (fi.fn == Fn::Sometimes) return run(fi.prob, a);
            withParam(all[0], a, [&](const Val& v, Arc sub) { run(num(v), sub); });
            return;
        }
        case Fn::SomeCyclesBy: {
            if (!need(3)) return;
            uint16_t f = fnArg(1);
            const Pat& x = all[2];
            withParam(all[0], a, [&](const Val& v, Arc sub) {
                forCycles(sub, [&](Arc s2, int64_t c) {
                    if (rnd(qint(c), 0) < num(v)) queryApply(f, x, s2);
                    else query(x, s2);
                });
            });
            return;
        }
        case Fn::Rand: {  // continuous, like a signal
            Ev* e = push();
            if (!e) return;
            e->hasWhole = false;
            e->part = a;
            e->isMap = false;
            e->nctl = 0;
            e->v = numF(rnd(a.s, 0));
            return;
        }
        case Fn::Irand: {
            if (!need(1)) return;
            withParam(all[0], a, [&](const Val& v, Arc sub) {
                Ev* e = push();
                if (!e) return;
                e->hasWhole = false;
                e->part = sub;
                e->isMap = false;
                e->nctl = 0;
                e->v = numQ(qint((int64_t)floorf(rnd(sub.s, 0) * (float)intOf(v))));
            });
            return;
        }
        case Fn::Sine:
        case Fn::Cosine:
        case Fn::Saw:
        case Fn::Tri:
        case Fn::Square: return qSignal(fi.fn, a);
        case Fn::Superimpose: {
            if (!need(2)) return;
            uint16_t f = fnArg(0);
            query(all[1], a);
            return queryApply(f, all[1], a);
        }
        case Fn::Off: {
            if (!need(3)) return;
            uint16_t f = fnArg(1);
            Pat sh;
            sh.k = Pat::ShiftP;
            sh.x = &all[2];
            sh.y = &all[0];
            query(all[2], a);
            return queryApply(f, sh, a);
        }
        case Fn::Jux: {
            if (!need(2)) return;
            uint16_t f = fnArg(0);
            uint8_t pan = (uint8_t)controlKey("pan");
            qCtl(all[1], pan, 'f', numQ(qint(0)), false, a);
            Pat right;
            right.k = Pat::SetCtl;
            right.x = &all[1];
            right.key = pan;
            right.v = numQ(qint(1));
            return queryApply(f, right, a);
        }
        case Fn::Stut:
        case Fn::Echo: {
            if (!need(4)) return;
            bool stut = fi.fn == Fn::Stut;
            const Pat& x = all[3];
            uint8_t gain = (uint8_t)controlKey("gain");
            withParam(all[0], a, [&](const Val& cv, Arc s1) {
                int64_t count = intOf(cv);
                withParam(all[1], s1, [&](const Val& v1, Arc s2) {
                    withParam(all[2], s2, [&](const Val& v2, Arc s3) {
                        if (count > 32) return fail("at most 32 echoes", at);
                        if (stut) {
                            // stut count feedback time: echo x, gain fb + (1-fb)(count-x)/count
                            float fb = num(v1);
                            Q t = timeOf(v2);
                            query(x, s3);
                            for (int64_t e = 1; e < count && !bad(); e++) {
                                Pat mul;
                                mul.k = Pat::MulCtl;
                                mul.x = &x;
                                mul.key = gain;
                                mul.v = numF(fb + (1 - fb) * ((float)(count - e) / (float)count));
                                qShift(mul, t * qint(e), s3);
                            }
                        } else {
                            // echo count time feedback: each echo shifted by time, gain * fb
                            qEchoLayers(x, count, timeOf(v1), num(v2), gain, s3);
                        }
                    });
                });
            });
            return;
        }
        case Fn::Chunk: {
            if (!need(3)) return;
            uint16_t f = fnArg(1);
            const Pat& x = all[2];
            withParam(all[0], a, [&](const Val& v, Arc sub) { qChunk(x, f, intOf(v), false, sub); });
            return;
        }
        case Fn::Striate: {
            if (!need(2)) return;
            const Pat& x = all[1];
            withParam(all[0], a, [&](const Val& v, Arc sub) {
                int64_t k = intOf(v);
                if (k <= 0 || k > 64) return;
                for (int64_t i = 0; i < k && !bad(); i++) {
                    Pat b, e;
                    b.k = Pat::SetCtl;
                    b.x = &x;
                    b.key = (uint8_t)controlKey("begin");
                    b.v = numQ(qlazy(i, k));
                    e.k = Pat::SetCtl;
                    e.x = &b;
                    e.key = (uint8_t)controlKey("end");
                    e.v = numQ(qlazy(i + 1, k));
                    qCompress(e, qlazy(i, k), qlazy(i + 1, k), sub);
                }
            });
            return;
        }
        case Fn::LoopAt: {
            if (!need(2)) return;
            const Pat& x = all[1];
            withParam(all[0], a, [&](const Val& v, Arc sub) {
                Q r = timeOf(v);
                if (r.n == 0) return;
                Pat slow, spd;
                slow.k = Pat::Fast;
                slow.x = &x;
                slow.q = Q{1, 1} / r;
                spd.k = Pat::MulCtl;
                spd.x = &slow;
                spd.key = (uint8_t)controlKey("speed");
                spd.v = numQ(Q{1, 1} / r);
                qCtl(spd, (uint8_t)controlKey("unit"), 's', strV(kEngineStr + kStrC), false, sub);
            });
            return;
        }
        case Fn::Slice:
            if (need(3)) qSplice(all[0], all[1], all[2], a, false);
            break;
        case Fn::Splice:
            if (need(3)) qSplice(all[0], all[1], all[2], a);
            return;
        case Fn::Scale: {
            if (!need(2)) return;
            const Program& p = P;
            appLeft(all[1], all[0], a, [&](const Ev& l, const Ev& r, Ev& o) {
                if (r.v.t != VT::Str) return fail("scale name must be a name", at), false;
                const char* name = valStr(p, r.v.str);
                const NamedSet* s = findSet(kScales, (int)(sizeof kScales / sizeof kScales[0]), name);
                if (!s) return fail("scale not supported yet", at), false;
                int64_t x = intOf(l.v), len = s->n, i = imod(x, len), oct = idivFloor(x, len);
                o.v = numQ(qint(s->v[i] + 12 * oct));
                return true;
            });
            return;
        }
        case Fn::Arp: {
            if (!need(2)) return;
            const Pat& x = all[1];
            withParam(all[0], a, [&](const Val& v, Arc sub) { qArp(x, v, sub); });
            return;
        }
        case Fn::OpUnionR:
        case Fn::OpUnionL:
        case Fn::OpNum:
        case Fn::ShiftL:
        case Fn::ShiftR:
            if (need(2)) binop(id, all[0], all[1], a);
            return;
        default: {
            if (fi.fn >= Fn::EveryP && fi.fn <= Fn::WRandcat) return callLib2(fi.fn, all.p, n, at, a);
            if (fi.fn >= Fn::Stutter && fi.fn <= Fn::Trigger) return callLib3(fi.fn, all.p, n, at, a);
            char m[64];
            snprintf(m, sizeof m, "%s is not implemented in the engine yet", lib(id).name);
            return fail(m, at);
        }
    }
}

static Q gTrigger{0, 1};
void setQueryTrigger(Q cycle) { gTrigger = cycle; }

// ---- batch 3 (2026-11-12): stutter, ghost, fix, chew ... ---------------------------
// Semantics from Tidal's documentation, checked against GHCi (golden/cases.txt,
// "batch 3").
void Eval::callLib3(Fn fn, const Pat* const* all, int n, uint16_t at, Arc a) {
    auto need = [&](int k) {
        if (n < k) fail("function is missing arguments", at);
        return n >= k;
    };
    auto fnArg = [&](int i) -> uint16_t {
        if (all[i]->k != Pat::Node) {
            fail("expected a function", at);
            return 0;
        }
        return all[i]->node;
    };
    switch (fn) {
        case Fn::Stutter: {  // stutter n t x = echoWith n t id x: n copies, each t later
            if (!need(3)) return;
            const Pat& x = *all[2];
            withParam(*all[0], a, [&](const Val& cv, Arc s1) {
                withParam(*all[1], s1, [&](const Val& tv, Arc s2) {
                    const int64_t count = intOf(cv);
                    if (count > 32) return fail("stutter: at most 32", at);
                    const Q t = timeOf(tv);
                    for (int64_t k = 0; k < count && !bad(); k++) {
                        Pat sh;
                        sh.k = Pat::Shift;
                        sh.x = &x;
                        sh.q = t * qint(k);
                        query(sh, s2);
                    }
                });
            });
            return;
        }
        case Fn::Ghost:
            if (need(1)) qGhost(*all[0], Q{1, 8}, a);
            return;
        case Fn::GhostP: {
            if (!need(2)) return;
            const Pat& x = *all[1];
            withParam(*all[0], a, [&](const Val& v, Arc sub) { qGhost(x, timeOf(v), sub); });
            return;
        }
        case Fn::IterR: {
            if (!need(2)) return;
            const Pat& x = *all[1];
            withParam(*all[0], a, [&](const Val& v, Arc sub) {
                const int64_t k = intOf(v);
                if (k == 0) return query(x, sub);
                if (k < 0 || k > 64) return fail("iter': 1..64 supported", at);
                qIter(x, k, sub, true);
            });
            return;
        }
        case Fn::Rangex:
            if (need(3)) qMapNum(fn, *all[0], *all[1], *all[2], a);
            return;
        case Fn::Quantise:
            if (need(2)) qMapNum(fn, *all[0], *all[0], *all[1], a);
            return;
        case Fn::Isaw:
        case Fn::EnvL:
        case Fn::EnvLR:
        case Fn::EnvEq:
        case Fn::EnvEqR: return qSignal(fn, a);
        case Fn::Brand:
        case Fn::BrandBy: {  // rand < probability, continuous like rand (same random stream)
            auto put = [&](float prob, Arc sub) {
                Ev* e = push();
                if (!e) return;
                e->hasWhole = false;
                e->part = sub;
                e->isMap = false;
                e->nctl = 0;
                e->v = boolV(rnd(sub.s, 0) < prob);
            };
            if (fn == Fn::Brand) return put(0.5f, a);
            if (!need(1)) return;
            withParam(*all[0], a, [&](const Val& v, Arc sub) { put(num(v), sub); });
            return;
        }
        case Fn::Append: {  // cat [a, b]
            if (!need(2)) return;
            const Pat two[2] = {*all[0], *all[1]};
            qCat(two, 2, a);
            return;
        }
        case Fn::FastAppend:  // fastcat [a, b]
            if (!need(2)) return;
            qCompress(*all[0], Q{0, 1}, Q{1, 2}, a);
            qCompress(*all[1], Q{1, 2}, Q{1, 1}, a);
            return;
        case Fn::RepeatCycles: {  // each cycle of x played n times
            if (!need(2)) return;
            const Pat& x = *all[1];
            withParam(*all[0], a, [&](const Val& v, Arc sub) {
                const int64_t k = intOf(v);
                if (k < 1) return;
                forCycles(sub, [&](Arc s2, int64_t c) {
                    Pat sh;
                    sh.k = Pat::Shift;
                    sh.x = &x;
                    sh.q = qint(c - idivFloor(c, k));
                    query(sh, s2);
                });
            });
            return;
        }
        case Fn::FoldEvery:
            if (need(3) && all[0]->k == Pat::Node) qFoldEvery(all[0]->node, fnArg(1), *all[2], a);
            return;
        case Fn::Fix:
        case Fn::Unfix:
        case Fn::Contrast: {  // contrast f g cond x = f (x where cond matches) + g (the rest)
            const bool both = fn == Fn::Contrast;
            if (!need(both ? 4 : 3)) return;
            const uint16_t f = fnArg(0), g = both ? fnArg(1) : 0;
            const Pat& cond = *all[both ? 2 : 1];
            const Pat& x = *all[both ? 3 : 2];
            Pat yes, no;
            yes.k = no.k = Pat::Match;
            yes.x = no.x = &x;
            yes.y = no.y = &cond;
            no.neg = true;
            if (fn == Fn::Unfix) query(yes, a);
            else queryApply(f, yes, a);
            if (fn == Fn::Fix) query(no, a);
            else queryApply(fn == Fn::Unfix ? f : g, no, a);
            return;
        }
        case Fn::Randslice: {
            if (!need(2)) return;
            const Pat& x = *all[1];
            withParam(*all[0], a, [&](const Val& v, Arc sub) { qRandslice(x, intOf(v), sub); });
            return;
        }
        case Fn::Chew: {
            if (!need(3)) return;
            withParam(*all[0], a, [&](const Val& v, Arc sub) { qChew(*all[2], intOf(v), *all[1], sub); });
            return;
        }
        case Fn::QTrigger:
        case Fn::Trigger: {  // the pattern's cycle 0 where the block took effect (qtrigger: next whole cycle)
            if (!need(1)) return;
            Q t = gTrigger;
            if (fn == Fn::QTrigger) t = qint(-qfloor(-t));
            Pat sh;
            sh.k = Pat::Shift;
            sh.x = all[0];
            sh.q = t;
            query(sh, a);
            return;
        }
        default: return fail("not implemented", at);
    }
}

// ghost' t x: x, plus quieter, shorter, higher copies 1.5t and 2.5t later
// (gain * 0.7, end 0.2, speed * 1.25), and one of the first 2.5t after that
void Eval::qGhost(const Pat& x, Q t, Arc a) {
    const uint8_t gain = (uint8_t)controlKey("gain"), end = (uint8_t)controlKey("end"),
                  speed = (uint8_t)controlKey("speed");
    // f p = p |*| gain 0.7 |> end 0.2 |*| speed 1.25 (|*| with a pure value
    // cuts each event at cycle boundaries: GHCi, golden ghost)
    auto fOf = [&](const Pat& p, Pat* f3) {  // f3: four Pats, the last is f p
        f3[0].k = Pat::ClipCycles;
        f3[0].x = &p;
        f3[1].k = Pat::MulCtl;
        f3[1].x = &f3[0];
        f3[1].key = speed;
        f3[1].v = numF(1.25f);
        f3[2].k = Pat::SetCtl;
        f3[2].x = &f3[1];
        f3[2].key = end;
        f3[2].v = numF(0.2f);
        f3[3].k = Pat::MulCtl;
        f3[3].x = &f3[2];
        f3[3].key = gain;
        f3[3].v = numF(0.7f);
    };
    const Q t1 = t * Q{3, 2}, t2 = t * Q{5, 2};
    Pat fx[4], h, g, fh[4], gh;
    fOf(x, fx);
    h.k = g.k = Pat::Shift;  // h x = t1 ~> f x, g x = t2 ~> f x
    h.x = g.x = &fx[3];
    h.q = t1;
    g.q = t2;
    fOf(h, fh);
    gh.k = Pat::Shift;  // g (h x)
    gh.x = &fh[3];
    gh.q = t2;
    query(x, a);
    query(h, a);
    query(g, a);
    query(gh, a);
}

// Every control of cond is in e with the same value (Tidal: a submap).
bool Eval::ctlsMatch(const Ev& cond, const Ev& e) const {
    const Ctl* c = ctls(cond);
    const Ctl* x = ctls(e);
    for (int i = 0; i < cond.nctl; i++) {
        bool hit = false;
        for (int j = 0; j < e.nctl && !hit; j++) {
            if (x[j].key != c[i].key) continue;
            if ((x[j].tag == 's') != (c[i].tag == 's')) break;
            hit = c[i].tag == 's' ? !strcmp(valStr(P, x[j].str), valStr(P, c[i].str)) : fabsf(x[j].f - c[i].f) < 1e-6f;
        }
        if (!hit) return false;
    }
    return true;
}

// x's events that cond matches at their onset (matched) or doesn't (!matched)
void Eval::qMatch(const Pat& x, const Pat& cond, bool matched, Arc a) {
    const int m = top;
    query(x, a);
    const int me = top;
    int w = m;
    for (int i = m; i < me && !bad(); i++) {
        bool hit = false;
        if (gArena[i].isMap) {
            const Q t = gArena[i].hasWhole ? gArena[i].whole.s : gArena[i].part.s;
            const int c0 = top;
            query(cond, Arc{t, t});
            for (int j = c0; j < top && !hit; j++)
                if (gArena[j].isMap && ctlsMatch(gArena[j], gArena[i])) hit = true;
            top = c0;
        }
        if (hit == matched) gArena[w++] = gArena[i];
    }
    top = w;
}

// foldEvery [n1, n2 ..] f x = every n1 f (every n2 f (.. x))
void Eval::qFoldEvery(uint16_t list, uint16_t f, const Pat& x, Arc a) {
    const int k = listLen(list);
    if (k > kMaxLayers) return fail("foldEvery: at most 8 numbers");
    Pat ev[kMaxLayers];
    const Pat* cur = &x;
    for (int i = k - 1; i >= 0; i--) {
        Val v;
        bool found;
        sampleAt(listAt(list, i), Q{0, 1}, v, found);
        if (!found) return fail("foldEvery: the list needs numbers");
        ev[i].k = Pat::EveryF;
        ev[i].x = cur;
        ev[i].node = f;
        ev[i].q = qint(intOf(v));
        cur = &ev[i];
    }
    query(*cur, a);
}

// rangex lo hi x: exp (range (log lo) (log hi) x); quantise q x: round (x * q) / q
// (Haskell's round: halves to even)
void Eval::qMapNum(Fn fn, const Pat& lo, const Pat& hi, const Pat& x, Arc a) {
    const int m = top;
    query(x, a);
    for (int i = m; i < top && !bad(); i++) {
        Ev& e = gArena[i];
        if (e.isMap || e.v.t != VT::Num) return fail(fn == Fn::Rangex ? "rangex needs numbers" : "quantise needs numbers");
        const Q t = e.hasWhole ? e.whole.s : e.part.s;
        const int saveTop = top;
        Val l, h;
        bool f1, f2 = true;
        sampleAt(lo, t, l, f1);
        if (fn == Fn::Rangex) sampleAt(hi, t, h, f2);
        top = saveTop;
        if (!f1 || !f2) continue;
        if (fn == Fn::Rangex) {
            const float a0 = logf(num(l)), a1 = logf(num(h));
            e.v = numF(expf(a0 + num(e.v) * (a1 - a0)));
        } else {
            const float q = num(l);
            if (q == 0) continue;
            const float r = rintf(num(e.v) * q);  // to nearest, halves to even
            e.v = q == 1 ? numQ(qint((int64_t)r)) : numF(r / q);
        }
    }
}

// randslice n x: each event plays a random one of n slices (begin / end, which win)
void Eval::qRandslice(const Pat& x, int64_t n, Arc a) {
    if (n <= 0) return;
    const uint8_t kb = (uint8_t)controlKey("begin"), ke = (uint8_t)controlKey("end");
    const int m = top;
    query(x, a);
    for (int i = m; i < top && !bad(); i++) {
        Ev& e = gArena[i];
        if (!e.isMap) return fail("randslice needs a control pattern");
        const Q t = e.hasWhole ? e.whole.s : e.part.s;
        int64_t k = (int64_t)(rnd(t, 7) * (float)n);
        if (k >= n) k = n - 1;
        const float b = (float)k / (float)n, en = (float)(k + 1) / (float)n;
        Ctl* w = ctlWrite(e, 2);
        if (!w) return;
        bool hb = false, he = false;
        for (int c = 0; c < e.nctl; c++) {
            if (w[c].key == kb) w[c] = ctlOf(kb, 'f', numF(b)), hb = true;
            if (w[c].key == ke) w[c] = ctlOf(ke, 'f', numF(en)), he = true;
        }
        if (!hb && e.nctl < kMaxCtl) w[e.nctl++] = ctlOf(kb, 'f', numF(b));
        if (!he && e.nctl < kMaxCtl) w[e.nctl++] = ctlOf(ke, 'f', numF(en));
    }
}

// chew: bite, with each piece's speed changed by how much it was squeezed
// (a slice of 1/n played in an event of length d: speed * (1/n) / d)
void Eval::qChew(const Pat& x, int64_t n, const Pat& idx, Arc a) {
    if (n <= 0) return;
    const uint8_t speed = (uint8_t)controlKey("speed");
    const int m = top;
    query(idx, a);
    const int ie = top;
    for (int i = m; i < ie && !bad(); i++) {
        if (!gArena[i].hasWhole) continue;
        const int64_t k = imod(intOf(gArena[i].v), n);
        const Q d = gArena[i].whole.e - gArena[i].whole.s;
        if (d.n <= 0) continue;
        Pat z, sp;
        z.k = Pat::Zoom;
        z.x = &x;
        z.q = qlazy(k, n);
        z.q2 = qlazy(k + 1, n);
        sp.k = Pat::MulCtl;
        sp.x = &z;
        sp.key = speed;
        sp.v = numF((1.0f / (float)n) / qfloat(d));
        qSqueeze(sp, gArena[i].whole, gArena[i].part);
    }
    drop(m, ie);
}

int queryPattern(const Program& p, uint16_t expr, Arc arc, Event* out, int cap, Error& err) {
    qOverflowFlag() = false;
    gCtlTop = 0;
    gParamTop = 0;
    Eval ev(p, err);
    ev.query(pNode(expr), arc);
    if (!err.failed && qOverflow()) err.set(-1, "time overflow (32-bit rationals)");
    if (err.failed) return -1;
    int n = ev.top < cap ? ev.top : cap;
    for (int i = 0; i < n; i++) {
        // expand the slim internal event; times are lazily normalised inside
        const Ev& in = gArena[i];
        Event& e = out[i];
        e.hasWhole = in.hasWhole;
        e.part = Arc{qnorm(in.part.s), qnorm(in.part.e)};
        if (in.hasWhole) e.whole = Arc{qnorm(in.whole.s), qnorm(in.whole.e)};
        e.isMap = in.isMap;
        e.v = in.v;
        if (e.v.t == VT::Num && e.v.exact) e.v.q = qnorm(e.v.q);
        e.nctl = in.isMap ? in.nctl : 0;
        for (int k = 0; k < e.nctl; k++) e.ctl[k] = gCtl[in.ctlAt + k];
#if TC_EVENT_SRC
        e.nsrc = in.nsrc;
        for (int k = 0; k < in.nsrc; k++) e.src[k] = in.src[k];
#endif
    }
    if (ev.top > cap) {
        err.set(-1, "more events than the output buffer holds");
        return -1;
    }
    return n;
}

}  // namespace tc
