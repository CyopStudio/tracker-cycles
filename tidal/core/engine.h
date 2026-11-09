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

// 12 bytes. Numbers stay exact rationals while they can (time arguments
// need that); otherwise float, which is also what OSC sends.
struct Val {
    VT t = VT::None;
    bool exact = false;  // Num: q holds the exact value, else fl
    int16_t idx = -1;    // Str: sample index ("bd:3"), -1 = none
    union {
        Q q;             // Num (exact) / Bool (0 or 1 in q.n)
        float fl;        // Num (inexact)
        uint16_t str;    // Str: Program string id, or kEngineStr + i
    };
    Val() : q{0, 1} {}
};
inline float num(const Val& v) {
    return v.t != VT::Num ? 0.0f : v.exact ? (float)v.q.n / (float)v.q.d : v.fl;
}

// Output controls (SuperDirt names). Key = index into this table.
struct ControlDef {
    const char* name;  // key sent to SuperDirt, e.g. "hcutoff"
    char tag;          // value type: s string, f double, n note, i int
};
int controlCount();
const ControlDef& controlDef(int key);
int controlKey(const char* name);  // -1 if unknown

// Events of control patterns carry a small map; others a single value.
// A control value is a float or a string id (8 bytes per entry).
constexpr int kMaxCtl = 20;
struct Ctl {
    uint8_t key;
    char tag;      // s f n i (usually controlDef(key).tag; "n" from "bd:3" is f)
    uint16_t str;  // tag 's'
    float f;       // other tags
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
// Events alive during one query (all intermediate results included) live
// in an arena of kEventArena slim internal events (engineEventBytes(), ~52
// B; their control maps sit in a pool of kCtlPool entries, 8 B each).
// `make bench` peaks at 384 events for a one-cycle query of its heaviest
// pattern (97 for quarter-cycle windows); a query that needs more fails
// with "too many events", and the scheduler should retry with a smaller
// window. TC_ENGINE_ARENA_ATTR places the arena and pools, e.g. in SDRAM
// (__NOINIT(EXTERNAL_RAM)); they are always written before being read.
#ifndef TC_ENGINE_ARENA
#define TC_ENGINE_ARENA 1024
#endif
#ifndef TC_ENGINE_CTL_POOL
#define TC_ENGINE_CTL_POOL 8192
#endif
#ifndef TC_ENGINE_ARENA_ATTR
#define TC_ENGINE_ARENA_ATTR
#endif
constexpr int kEventArena = TC_ENGINE_ARENA;
constexpr int kCtlPool = TC_ENGINE_CTL_POOL;
constexpr int kParamStack = 256;  // parameter values of nested patterned arguments
constexpr int kMaxEvalDepth = 64;

// Query expression `expr` of a checked program over `arc`. Returns the
// number of events written to `out` (at most `cap`), or -1 with `err` set
// (unsupported function, arena full, time overflow, ...).
int queryPattern(const Program& p, uint16_t expr, Arc arc, Event* out, int cap, Error& err);

// Render a value for tests / debugging ("bd", "3.5", "t").
int formatVal(const Program& p, const Val& v, char* buf, int cap);
int formatCtl(const Program& p, const Ctl& c, char* buf, int cap);

// Highest arena use since the last reset (to size kEventArena). Returned
// events (Event, 212 B) are self-contained copies.
int engineHighWater();
void engineResetStats();
// Query into `storage` (bytes) instead of the built-in arena; nullptr
// switches back. For measuring memory placement (bench).
void engineUseArena(void* storage, int bytes);
void* engineDefaultArena();
int engineEventBytes();    // size of one internal (arena) event
int engineCtlHighWater();  // control pool use since engineResetStats()
// Lowest stack address reached by the evaluator since engineResetMinStack()
// (only tracked with -DTC_TRACK_STACK; else UINTPTR_MAX).
uintptr_t engineMinStack();
void engineResetMinStack();

}  // namespace tc
