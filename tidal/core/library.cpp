// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
#include "library.h"

#include <string.h>

namespace tc {

// Signatures checked against :t in GHCi, Tidal 1.10.3 (golden/types.txt,
// 2026-11-09). Simplifications: Integer is Int (stut, echo count), the
// Real constraint of saw/tri is dropped, |op| forms are typed on Pat.
#define FN(n, s, c, d, def) {n, s, Cat::c, Device::d, def, 0, Assoc::None, false}
#define CTL(n, t) {n, "Pat " t " -> Pat Control", Cat::Control, Device::Full, nullptr, 0, Assoc::None, false}
#define OP(n, s, p, a, chk) {n, s, Cat::Operator, Device::Full, nullptr, p, Assoc::a, chk}

static const LibEntry kLib[] = {
    // ---- core / time --------------------------------------------------------
    FN("silence", "Pat a", Core, Full, nullptr),
    FN("fast", "Pat Time -> Pat a -> Pat a", Time, Full, "2"),
    FN("slow", "Pat Time -> Pat a -> Pat a", Time, Full, "2"),
    FN("hurry", "Pat Time -> Pat Control -> Pat Control", Time, Approx, "2"),
    FN("rev", "Pat a -> Pat a", Time, Full, nullptr),
    FN("palindrome", "Pat a -> Pat a", Time, Full, nullptr),
    FN("brak", "Pat a -> Pat a", Time, Full, nullptr),
    FN("iter", "Pat Int -> Pat a -> Pat a", Time, Full, "4"),
    FN("ply", "Pat Time -> Pat a -> Pat a", Time, Full, "2"),
    FN("linger", "Pat Time -> Pat a -> Pat a", Time, Full, "0.25"),
    FN("segment", "Pat Time -> Pat a -> Pat a", Time, Full, "4"),
    FN("stack", "[Pat a] -> Pat a", Core, Full, nullptr),
    FN("cat", "[Pat a] -> Pat a", Core, Full, nullptr),
    FN("fastcat", "[Pat a] -> Pat a", Core, Full, nullptr),
    FN("slowcat", "[Pat a] -> Pat a", Core, Full, nullptr),
    FN("overlay", "Pat a -> Pat a -> Pat a", Core, Full, nullptr),
    FN("every", "Pat Int -> (Pat a -> Pat a) -> Pat a -> Pat a", Time, Full, "4 rev"),
    FN("whenmod", "Pat Time -> Pat Time -> (Pat a -> Pat a) -> Pat a -> Pat a", Time, Full, "4 2 rev"),
    FN("range", "Num a => Pat a -> Pat a -> Pat a -> Pat a", Signal, Full, "1 5"),
    // ---- structure ----------------------------------------------------------
    FN("struct", "Pat Bool -> Pat a -> Pat a", Structure, Full, "\"t f t t\""),
    FN("mask", "Pat Bool -> Pat a -> Pat a", Structure, Full, "\"1 0 1 1\""),
    FN("euclid", "Pat Int -> Pat Int -> Pat a -> Pat a", Structure, Full, "3 8"),
    FN("euclidInv", "Pat Int -> Pat Int -> Pat a -> Pat a", Structure, Full, "3 8"),
    FN("euclidFull", "Pat Int -> Pat Int -> Pat a -> Pat a -> Pat a", Structure, Full, "3 8"),
    // ---- random -------------------------------------------------------------
    FN("degradeBy", "Pat Double -> Pat a -> Pat a", Random, Full, "0.5"),
    FN("sometimesBy", "Pat Double -> (Pat a -> Pat a) -> Pat a -> Pat a", Random, Full, "0.5 rev"),
    FN("sometimes", "(Pat a -> Pat a) -> Pat a -> Pat a", Random, Full, "rev"),
    FN("often", "(Pat a -> Pat a) -> Pat a -> Pat a", Random, Full, "rev"),
    FN("rarely", "(Pat a -> Pat a) -> Pat a -> Pat a", Random, Full, "rev"),
    FN("almostNever", "(Pat a -> Pat a) -> Pat a -> Pat a", Random, Full, "rev"),
    FN("almostAlways", "(Pat a -> Pat a) -> Pat a -> Pat a", Random, Full, "rev"),
    FN("someCyclesBy", "Pat Double -> (Pat a -> Pat a) -> Pat a -> Pat a", Random, Full, "0.5 rev"),
    FN("rand", "Frac a => Pat a", Random, Full, nullptr),
    FN("irand", "Num a => Pat Int -> Pat a", Random, Full, "8"),
    // ---- continuous signals -------------------------------------------------
    FN("sine", "Frac a => Pat a", Signal, Full, nullptr),
    FN("cosine", "Frac a => Pat a", Signal, Full, nullptr),
    FN("saw", "Frac a => Pat a", Signal, Full, nullptr),
    FN("tri", "Frac a => Pat a", Signal, Full, nullptr),
    FN("square", "Frac a => Pat a", Signal, Full, nullptr),
    // ---- layering -----------------------------------------------------------
    FN("superimpose", "(Pat a -> Pat a) -> Pat a -> Pat a", Layer, Full, "(fast 2)"),
    FN("off", "Pat Time -> (Pat a -> Pat a) -> Pat a -> Pat a", Layer, Full, "0.25 (# crush 4)"),
    FN("jux", "(Pat Control -> Pat Control) -> Pat Control -> Pat Control", Layer, Approx, "rev"),
    FN("stut", "Pat Int -> Pat Double -> Pat Time -> Pat Control -> Pat Control", Layer, Full, "3 0.5 0.125"),
    FN("echo", "Pat Int -> Pat Time -> Pat Double -> Pat Control -> Pat Control", Layer, Full, "3 0.2 0.5"),
    FN("chunk", "Pat Int -> (Pat a -> Pat a) -> Pat a -> Pat a", Layer, Full, "4 (fast 2)"),
    // ---- samples ------------------------------------------------------------
    FN("chop", "Pat Int -> Pat Control -> Pat Control", Sample, HostOnly, "4"),
    FN("striate", "Pat Int -> Pat Control -> Pat Control", Sample, HostOnly, "4"),
    // loopAt/splice set speed so the sample fits the cycle / event (unit "c");
    // the device can only approximate that rate with tune + fine-tune.
    // Signatures verified with :t in GHCi (Tidal 1.10.3), 2026-11-09.
    FN("loopAt", "Pat Time -> Pat Control -> Pat Control", Sample, Approx, "2"),
    FN("splice", "Pat Int -> Pat Int -> Pat Control -> Pat Control", Sample, Approx, "4 \"0 2\""),
    // slice (RC13 user ask): begin / end only, which the Tracker plays exactly
    FN("slice", "Pat Int -> Pat Int -> Pat Control -> Pat Control", Sample, Full, "4 \"0 2\""),
    // ---- harmony ------------------------------------------------------------
    FN("scale", "Frac a => Pat String -> Pat Int -> Pat a", Harmony, Full, "\"major\""),
    FN("arp", "Pat String -> Pat a -> Pat a", Harmony, Full, "\"up\""),
    // ---- batch 2 (2026-11-11): signatures from :t in GHCi, Tidal 1.10.3 -----
    // (Integral / Ord / Enum constraints dropped, Ratio Integer as Time)
    FN("every'", "Pat Int -> Pat Int -> (Pat a -> Pat a) -> Pat a -> Pat a", Time, Full, "3 0 rev"),
    FN("inside", "Pat Time -> (Pat b -> Pat a) -> Pat b -> Pat a", Time, Full, "2 rev"),
    FN("outside", "Pat Time -> (Pat b -> Pat a) -> Pat b -> Pat a", Time, Full, "2 rev"),
    FN("trunc", "Pat Time -> Pat a -> Pat a", Time, Full, "0.75"),
    FN("discretise", "Pat Time -> Pat a -> Pat a", Time, Full, "4"),
    FN("fastGap", "Pat Time -> Pat a -> Pat a", Time, Full, "2"),
    FN("swingBy", "Pat Time -> Pat Time -> Pat a -> Pat a", Time, Full, "(1/3) 4"),
    FN("swing", "Pat Time -> Pat a -> Pat a", Time, Full, "4"),
    FN("press", "Pat a -> Pat a", Time, Full, nullptr),
    FN("pressBy", "Pat Time -> Pat a -> Pat a", Time, Full, "0.25"),
    FN("rot", "Pat Int -> Pat a -> Pat a", Structure, Full, "1"),
    FN("inv", "Pat Bool -> Pat Bool", Structure, Full, nullptr),
    FN("sew", "Pat Bool -> Pat a -> Pat a -> Pat a", Structure, Full, nullptr),
    FN("stitch", "Pat Bool -> Pat a -> Pat a -> Pat a", Structure, Full, nullptr),
    FN("while", "Pat Bool -> (Pat a -> Pat a) -> Pat a -> Pat a", Structure, Full, "\"1 0\" (fast 2)"),
    FN("euclidOff", "Pat Int -> Pat Int -> Pat Int -> Pat a -> Pat a", Structure, Full, "3 8 2"),
    FN("eoff", "Pat Int -> Pat Int -> Pat Int -> Pat a -> Pat a", Structure, Full, "3 8 2"),
    FN("wedge", "Pat Time -> Pat a -> Pat a -> Pat a", Structure, Full, nullptr),
    FN("bite", "Pat Int -> Pat Int -> Pat a -> Pat a", Structure, Full, "4 \"0 2*2\""),
    FN("squeeze", "Pat Int -> [Pat a] -> Pat a", Core, Full, nullptr),
    FN("select", "Pat Double -> [Pat a] -> Pat a", Core, Full, nullptr),
    FN("run", "Num a => Pat a -> Pat a", Signal, Full, "4"),
    FN("scan", "Num a => Pat a -> Pat a", Signal, Full, "4"),
    FN("chunk'", "Pat Int -> (Pat a -> Pat a) -> Pat a -> Pat a", Layer, Full, "4 (fast 2)"),
    FN("echoWith", "Pat Int -> Pat Time -> (Pat a -> Pat a) -> Pat a -> Pat a", Layer, Full, "3 (1/8) (|* speed 2)"),
    FN("stutWith", "Pat Int -> Pat Time -> (Pat a -> Pat a) -> Pat a -> Pat a", Layer, Full, "3 (1/8) (|* speed 2)"),
    // Tidal: (Ord t, Num t) => Pattern t, which GHC defaults; Double here
    FN("plyWith", "Pat Double -> (Pat a -> Pat a) -> Pat a -> Pat a", Layer, Full, "3 (|* gain 0.8)"),
    FN("juxBy", "Pat Double -> (Pat Control -> Pat Control) -> Pat Control -> Pat Control", Layer, Approx, "0.5 rev"),
    FN("layer", "[Pat a -> Pat b] -> Pat a -> Pat b", Layer, Full, "[rev, fast 2]"),
    FN("pickF", "Pat Int -> [Pat a -> Pat a] -> Pat a -> Pat a", Layer, Full, "\"<0 1>\" [rev, fast 2]"),
    FN("selectF", "Pat Double -> [Pat a -> Pat a] -> Pat a -> Pat a", Layer, Full, "\"<0 0.5>\" [rev, fast 2]"),
    FN("rolled", "Pat a -> Pat a", Harmony, Full, nullptr),
    FN("rolledBy", "Pat Time -> Pat a -> Pat a", Harmony, Full, "0.25"),
    // Tidal: [a] and Int; numeric literals are patterns here, so the list's
    // and the octave's values are read at time 0
    FN("toScale", "Num a => [Pat a] -> Pat Int -> Pat a", Harmony, Full, nullptr),
    // pairs (Tidal: (Time, Time), plain values; read at time 0 here)
    FN("within", "(Pat Time, Pat Time) -> (Pat a -> Pat a) -> Pat a -> Pat a", Time, Full, "(0, 0.5) (fast 2)"),
    FN("within'", "(Pat Time, Pat Time) -> (Pat a -> Pat a) -> Pat a -> Pat a", Time, Full, "(0, 0.5) (fast 2)"),
    FN("zoom", "(Pat Time, Pat Time) -> Pat a -> Pat a", Time, Full, "(0.25, 0.75)"),
    FN("compress", "(Pat Time, Pat Time) -> Pat a -> Pat a", Time, Full, "(0.25, 0.75)"),
    FN("wchoose", "[(Pat a, Pat Double)] -> Pat a", Random, Full, nullptr),
    FN("wrandcat", "[(Pat a, Pat Double)] -> Pat a", Random, Full, nullptr),
    FN("degrade", "Pat a -> Pat a", Random, Full, nullptr),
    FN("unDegradeBy", "Pat Double -> Pat a -> Pat a", Random, Full, "0.5"),
    FN("sometimesBy'", "Pat Double -> (Pat a -> Pat a) -> Pat a -> Pat a", Random, Full, "0.5 rev"),
    FN("someCycles", "(Pat a -> Pat a) -> Pat a -> Pat a", Random, Full, "rev"),
    FN("somecycles", "(Pat a -> Pat a) -> Pat a -> Pat a", Random, Full, "rev"),
    FN("shuffle", "Pat Int -> Pat a -> Pat a", Random, Full, "4"),
    FN("scramble", "Pat Int -> Pat a -> Pat a", Random, Full, "4"),
    FN("perlin", "Frac a => Pat a", Random, Full, nullptr),
    FN("randcat", "[Pat a] -> Pat a", Random, Full, nullptr),
    // Tidal: [a]; literals are patterns here, read at time 0
    FN("choose", "[Pat a] -> Pat a", Random, Full, nullptr),
    FN("chooseBy", "Pat Double -> [Pat a] -> Pat a", Random, Full, nullptr),
    FN("cycleChoose", "[Pat a] -> Pat a", Random, Full, nullptr),
    FN("toScale'", "Num a => Pat Int -> [Pat a] -> Pat Int -> Pat a", Harmony, Full, nullptr),
    // ---- batch 3 (2026-11-12): signatures from :t in GHCi, Tidal 1.10.3 -----
    // (plain Int / Time / [Int] arguments as patterns read at time 0, as above)
    FN("stutter", "Pat Int -> Pat Time -> Pat a -> Pat a", Layer, Full, "4 (1/16)"),
    FN("ghost", "Pat Control -> Pat Control", Layer, Full, nullptr),
    FN("ghost'", "Pat Time -> Pat Control -> Pat Control", Layer, Full, "(1/16)"),
    FN("iter'", "Pat Int -> Pat a -> Pat a", Time, Full, "4"),
    FN("rangex", "Frac a => Pat a -> Pat a -> Pat a -> Pat a", Signal, Full, "200 4000"),
    FN("quantise", "Frac a => Pat a -> Pat a -> Pat a", Signal, Full, "1"),
    FN("isaw", "Frac a => Pat a", Signal, Full, nullptr),
    FN("envL", "Pat Double", Signal, Full, nullptr),
    FN("envLR", "Pat Double", Signal, Full, nullptr),
    FN("envEq", "Pat Double", Signal, Full, nullptr),
    FN("envEqR", "Pat Double", Signal, Full, nullptr),
    FN("brand", "Pat Bool", Random, Full, nullptr),
    FN("brandBy", "Pat Double -> Pat Bool", Random, Full, "0.3"),
    FN("append", "Pat a -> Pat a -> Pat a", Structure, Full, nullptr),
    FN("fastAppend", "Pat a -> Pat a -> Pat a", Structure, Full, nullptr),
    FN("slowAppend", "Pat a -> Pat a -> Pat a", Structure, Full, nullptr),
    FN("repeatCycles", "Pat Int -> Pat a -> Pat a", Time, Full, "2"),
    FN("foldEvery", "[Pat Int] -> (Pat a -> Pat a) -> Pat a -> Pat a", Time, Full, "[3, 5] (fast 2)"),
    FN("fix", "(Pat Control -> Pat Control) -> Pat Control -> Pat Control -> Pat Control", Structure, Full,
       "(# crush 4) (n 1)"),
    FN("unfix", "(Pat Control -> Pat Control) -> Pat Control -> Pat Control -> Pat Control", Structure, Full,
       "(# crush 4) (n 1)"),
    FN("contrast",
       "(Pat Control -> Pat Control) -> (Pat Control -> Pat Control) -> Pat Control -> Pat Control -> Pat Control",
       Structure, Full, "(# crush 4) (# speed 2) (n 1)"),
    FN("randslice", "Pat Int -> Pat Control -> Pat Control", Sample, Full, "4"),
    FN("chew", "Pat Int -> Pat Int -> Pat Control -> Pat Control", Sample, Approx, "4 \"0 1 [2 3]\""),
    // restart from the pattern's cycle 0 when the block takes effect (qtrigger:
    // at the next cycle boundary); the scheduler knows when (setQueryTrigger)
    FN("qtrigger", "Pat a -> Pat a", Time, Full, nullptr),
    FN("trigger", "Pat a -> Pat a", Time, Full, nullptr),
    // ---- controls -----------------------------------------------------------
    CTL("s", "String"),
    CTL("sound", "String"),
    CTL("n", "Note"),
    CTL("note", "Note"),
    CTL("up", "Note"),
    CTL("gain", "Double"),
    CTL("pan", "Double"),
    CTL("cutoff", "Double"),
    CTL("resonance", "Double"),
    CTL("hpf", "Double"),
    CTL("bandf", "Double"),
    CTL("delay", "Double"),
    CTL("room", "Double"),
    CTL("crush", "Double"),
    CTL("shape", "Double"),
    CTL("begin", "Double"),
    CTL("speed", "Double"),
    CTL("legato", "Double"),
    CTL("sustain", "Double"),
    CTL("nudge", "Double"),
    CTL("cut", "Int"),
    CTL("orbit", "Int"),
    CTL("end", "Double"),
    CTL("unit", "String"),
    // Tracker mapping (docs/tidal_control_mapping.md); aliases as in Tidal
    CTL("amp", "Double"),
    CTL("velocity", "Double"),
    CTL("attack", "Double"),
    CTL("release", "Double"),
    CTL("hresonance", "Double"),
    CTL("distort", "Double"),
    CTL("midichan", "Double"),
    CTL("ccn", "Double"),
    CTL("ccv", "Double"),
    CTL("midibend", "Double"),
    CTL("lpf", "Double"),
    CTL("lpq", "Double"),
    CTL("hpq", "Double"),
    CTL("bpf", "Double"),
    CTL("att", "Double"),
    CTL("rel", "Double"),
    // Tracker Cycles' own (not in Tidal: `let voices = pI "voices"`, see
    // docs/tidal_voices.md): notes a sample track lets ring, 1-3 (default 1)
    CTL("voices", "Int"),
    // batch 3 (2026-11-12); sz / delayt / delayfb are Tidal's aliases
    CTL("size", "Double"),
    CTL("sz", "Double"),
    CTL("delaytime", "Double"),
    CTL("delayt", "Double"),
    CTL("delayfeedback", "Double"),
    CTL("delayfb", "Double"),
    CTL("djf", "Double"),
    CTL("coarse", "Double"),
    CTL("progNum", "Double"),
    CTL("miditouch", "Double"),
    // ---- operators (fixities from GHCi :info, Tidal 1.10.3) ------------------
    OP("#", "Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("|+|", "Num a => Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("|+", "Num a => Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("+|", "Num a => Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("|-|", "Num a => Pat a -> Pat a -> Pat a", 9, Left, false),
    OP("|-", "Num a => Pat a -> Pat a -> Pat a", 9, Left, false),
    OP("-|", "Num a => Pat a -> Pat a -> Pat a", 9, Left, false),
    OP("|*|", "Num a => Pat a -> Pat a -> Pat a", 9, Left, false),
    OP("|*", "Num a => Pat a -> Pat a -> Pat a", 9, Left, false),
    OP("*|", "Num a => Pat a -> Pat a -> Pat a", 9, Left, false),
    OP("|/|", "Frac a => Pat a -> Pat a -> Pat a", 9, Left, false),
    OP("|/", "Frac a => Pat a -> Pat a -> Pat a", 9, Left, false),
    OP("/|", "Frac a => Pat a -> Pat a -> Pat a", 9, Left, false),
    OP("|%|", "Num a => Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("|%", "Num a => Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("%|", "Num a => Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("|**|", "Frac a => Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("|**", "Frac a => Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("**|", "Frac a => Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("|<", "Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("|>", "Pat a -> Pat a -> Pat a", 9, Left, true),
    // structure from the right / both (values as |< / |>)
    OP(">|", "Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("<|", "Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("|>|", "Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("|<|", "Pat a -> Pat a -> Pat a", 9, Left, true),
    OP("<~", "Pat Time -> Pat a -> Pat a", 9, Left, true),
    OP("~>", "Pat Time -> Pat a -> Pat a", 9, Left, true),
    // Haskell arithmetic on patterns (Num (Pattern a))
    OP("+", "Num a => Pat a -> Pat a -> Pat a", 6, Left, true),
    OP("-", "Num a => Pat a -> Pat a -> Pat a", 6, Left, true),
    OP("*", "Num a => Pat a -> Pat a -> Pat a", 7, Left, true),
    OP("/", "Frac a => Pat a -> Pat a -> Pat a", 7, Left, true),
};

#undef FN
#undef CTL
#undef OP

int libCount() { return (int)(sizeof kLib / sizeof kLib[0]); }

bool libIsAlias(int id) {
    static const char* const kAliases[] = {"somecycles", "eoff", "stutWith", "discretise"};
    for (const char* a : kAliases)
        if (!strcmp(lib(id).name, a)) return true;
    return false;
}
const LibEntry& lib(int id) { return kLib[id]; }

int libFind(const char* name, int len) {
    for (int i = 0; i < libCount(); i++)
        if ((int)strlen(kLib[i].name) == len && strncmp(kLib[i].name, name, len) == 0) return i;
    return -1;
}

}  // namespace tc
