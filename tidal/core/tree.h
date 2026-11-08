// Tracker Cycles code tree (see docs/tidal_code_tree.md).
// Portable, no heap allocation: everything lives in a fixed Program pool.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace tc {

// ---- Limits (spec 6.2) ---------------------------------------------------
constexpr int kMaxNodes = 4096;       // per program, index 0 = null
constexpr int kMaxBlockNodes = 512;
constexpr int kMaxDepth = 32;
constexpr int kMaxChildren = 64;
constexpr int kMaxSeqSteps = 64;
constexpr int kMaxStrings = 256;
constexpr int kMaxStringLen = 15;     // chars, excluding terminator
constexpr int kMaxTypes = 255;
constexpr int kSlots = 16;            // d1..d16
constexpr int kSampleSlots = 8;       // d1..d8; d9..d16 are MIDI 1..8
constexpr int kMaxCommentLen = 127;

// ---- Node kinds (spec 4 and 5) ---------------------------------------------
enum class Kind : uint8_t {
    None = 0,
    // program
    Block,      // child: expression (or none). a = slot (0..15)
    // expressions
    Num,        // a = numerator (int32), b = denominator; flags = decimals
    Mini,       // child: mini root (MSeq / MSub)
    Fn,         // a = library id
    App,        // children: fn, args...; flags kDollar
    Op,         // children: lhs, rhs; a = operator library id
    SectionL,   // child: lhs; a = operator id   e.g. (0.25 ~>)
    SectionR,   // child: rhs; a = operator id   e.g. (# speed 2)
    Compose,    // children: f, g
    List,       // children: elements
    Hole,       // editor placeholder; a = expected type id (optional)
    // mini-notation containers
    MSeq,       // children: steps; flags kDots -> children are MSeq groups
    MSub,       // [a b, c d]  children: MSeq layers
    MAlt,       // <a b, c>    children: MSeq layers
    MPoly,      // {a b, c}%n  children: MSeq layers, then optional steps arg
                //             (flags kHasSteps -> last child is the steps arg)
    MChoice,    // [a | b]     children: MSeq alternatives
    // mini-notation modifiers: first child = target step, rest = args
    MFast,      // x*f
    MSlow,      // x/f
    MReplicate, // x!n   flags kBare -> rendered as "x ! !" (a = count)
    MWeight,    // x@w   flags kUnderscore -> "x _ _" (a = weight)
    MDegrade,   // x?    optional arg: probability
    MEuclid,    // x(k,n[,r]) args are MSeq
    MRange,     // lo .. hi   children: lo atom, hi atom
    // mini-notation atoms
    MRest,      // ~
    MWord,      // a = string id; b = sample index + 1 (0 = none)
    MNum,       // like Num
    MNote,      // a = semitone from c5 (int32); flags = original spelling id
    MChord,     // a = string id of the full "root'name'mods" text
    MBool,      // a = 0/1; flags kDigitStyle -> rendered as 1/0
    Count
};

// Node flags
constexpr uint16_t kDollar = 1 << 0;      // App: last arg after $
constexpr uint16_t kDots = 1 << 1;        // MSeq: '.'-separated groups
constexpr uint16_t kBare = 1 << 2;        // MReplicate: "x ! !"
constexpr uint16_t kUnderscore = 1 << 3;  // MWeight: "x _ _"
constexpr uint16_t kHasSteps = 1 << 4;    // MPoly: last child is %steps
constexpr uint16_t kDigitStyle = 1 << 5;  // MBool: 1/0 instead of t/f
constexpr uint16_t kNegParen = 1 << 6;    // Num: written as (-x)
constexpr uint16_t kDecimalsMask = 0x0F00; // Num/MNum: fractional digits
constexpr int kDecimalsShift = 8;

struct Node {
    Kind kind;
    uint8_t type;   // type-table index (0 = none / error)
    uint16_t flags;
    uint16_t first; // first child (0 = none)
    uint16_t next;  // next sibling (0 = none)
    uint32_t a;
    uint32_t b;
};
static_assert(sizeof(Node) == 16, "Node must stay 16 bytes");

struct Rational {
    int32_t num;
    uint32_t den;  // > 0
};
Rational makeRational(int64_t num, int64_t den);  // normalised; den 0 -> 0/1

struct Error {
    bool failed = false;
    int pos = -1;   // character position in source text, if any
    uint16_t node = 0;
    char msg[96] = {0};
    void set(int p, const char* fmt, ...);
};

// ---- Program --------------------------------------------------------------
class Program {
public:
    Program() { clear(); }
    void clear();

    // node pool
    uint16_t alloc(Kind k);           // 0 if the pool is full
    void freeTree(uint16_t idx);      // returns a subtree to the pool
    Node& at(uint16_t idx) { return nodes_[idx]; }
    const Node& at(uint16_t idx) const { return nodes_[idx]; }
    int liveNodes() const { return live_; }

    // tree helpers
    void appendChild(uint16_t parent, uint16_t child);
    uint16_t child(uint16_t parent, int n) const;   // nth child or 0
    int childCount(uint16_t parent) const;
    uint16_t parentOf(uint16_t idx) const;          // 0 if root / not found
    uint16_t prevSibling(uint16_t idx) const;
    int subtreeSize(uint16_t idx) const;
    // Replace node `old` in its parent's child list with `repl` (keeps
    // old's siblings). Does not free `old`.
    void replace(uint16_t old, uint16_t repl);
    uint16_t copyTree(const Program& src, uint16_t idx);  // 0 on overflow

    // strings
    int intern(const char* s, int len);   // -1 if table full or too long
    const char* str(int id) const { return strings_[id]; }
    int stringCount() const { return nstrings_; }

    // blocks: one root per slot (0 = empty)
    uint16_t block[kSlots];
    char comment[kSlots][kMaxCommentLen + 1];
    uint16_t cpsExpr;  // `setcps <expr>` (0 = none)

private:
    Node nodes_[kMaxNodes];
    uint16_t freeHead_;
    int live_;
    char strings_[kMaxStrings][kMaxStringLen + 1];
    int nstrings_;
};

// Rational helpers stored in Num / MNum nodes
inline Rational nodeRational(const Node& n) {
    return Rational{(int32_t)n.a, n.b};
}
inline int nodeDecimals(const Node& n) {
    return (n.flags & kDecimalsMask) >> kDecimalsShift;
}
inline void setNodeRational(Node& n, Rational r, int decimals) {
    n.a = (uint32_t)r.num;
    n.b = r.den;
    n.flags = (uint16_t)((n.flags & ~kDecimalsMask) |
                         ((decimals << kDecimalsShift) & kDecimalsMask));
}

const char* kindName(Kind k);
bool isMiniKind(Kind k);
bool isMiniAtom(Kind k);
bool isMiniModifier(Kind k);

}  // namespace tc
