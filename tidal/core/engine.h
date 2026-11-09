// Query engine (plan step 2): a checked code tree -> events over a time
// span, like Tidal's queryArc. Portable, no heap: events live in a fixed
// arena, closures for higher-order functions live on the C stack.
//
// Semantics are implemented from Tidal's documented behaviour and checked
// against golden/expected.txt (captured from real Tidal 1.10.3); no Tidal
// source is translated (CLAUDE.md "Licensing").
#pragma once
#include <stdint.h>

#include "qtime.h"
#include "tree.h"

namespace tc {

// ---- values ------------------------------------------------------------------
constexpr uint16_t kEngineStr = 0xFF00;  // strings owned by the engine ("c")
enum class VT : uint8_t { None, Str, Num, Bool };

struct Val {
    VT t = VT::None;
    bool exact = false;  // Num: q holds the exact value
    int16_t idx = -1;    // Str: sample index ("bd:3"), -1 = none
    uint16_t str = 0;    // Str: Program string id, or kEngineStr + i
    Q q{0, 1};           // Num (exact) / Bool (0 or 1 in q.n)
    double f = 0;        // Num: always set
};

// Output controls (SuperDirt names). Key = index into this table.
struct ControlDef {
    const char* name;  // key sent to SuperDirt, e.g. "hcutoff"
    char tag;          // value type: s string, f double, n note, i int
};
int controlCount();
const ControlDef& controlDef(int key);
int controlKey(const char* name);  // -1 if unknown

// Events of control patterns carry a small map; others a single value.
// TODO(device): kMaxCtl x Ctl per event is large (~400 bytes per event);
// move maps to a shared pool before the MK66 build.
constexpr int kMaxCtl = 20;
struct Ctl {
    uint8_t key;
    char tag;  // s f n i (usually controlDef(key).tag; "n" from "bd:3" is f)
    Val v;
};

struct Event {
    Arc whole;          // valid if hasWhole
    Arc part;
    bool hasWhole = true;
    bool isMap = false;
    Val v;              // !isMap
    uint8_t nctl = 0;   // isMap
    Ctl ctl[kMaxCtl];
};

// ---- engine ------------------------------------------------------------------
constexpr int kEventArena = 8192;  // events alive during one query
constexpr int kMaxEvalDepth = 64;

// Query expression `expr` of a checked program over `arc`. Returns the
// number of events written to `out` (at most `cap`), or -1 with `err` set
// (unsupported function, arena full, time overflow, ...).
int queryPattern(const Program& p, uint16_t expr, Arc arc, Event* out, int cap, Error& err);

// Render a value for tests / debugging ("bd", "3.5", "t").
int formatVal(const Program& p, const Val& v, char* buf, int cap);

}  // namespace tc
