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
    // ---- harmony ------------------------------------------------------------
    FN("scale", "Frac a => Pat String -> Pat Int -> Pat a", Harmony, Full, "\"major\""),
    FN("arp", "Pat String -> Pat a -> Pat a", Harmony, Full, "\"up\""),
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
const LibEntry& lib(int id) { return kLib[id]; }

int libFind(const char* name, int len) {
    for (int i = 0; i < libCount(); i++)
        if ((int)strlen(kLib[i].name) == len && strncmp(kLib[i].name, name, len) == 0) return i;
    return -1;
}

}  // namespace tc
