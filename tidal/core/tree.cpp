#include "tree.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace tc {

static int64_t gcd64(int64_t a, int64_t b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) {
        int64_t t = a % b;
        a = b;
        b = t;
    }
    return a ? a : 1;
}

Rational makeRational(int64_t num, int64_t den) {
    if (den == 0) return Rational{0, 1};
    if (den < 0) {
        num = -num;
        den = -den;
    }
    int64_t g = gcd64(num, den);
    return Rational{(int32_t)(num / g), (uint32_t)(den / g)};
}

void Error::set(int p, const char* fmt, ...) {
    if (failed) return;  // keep the first error
    failed = true;
    pos = p;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
}

void Program::clear() {
    // O(1) in the pool: nodes past hwm_ are handed out fresh (alloc zeroes
    // them), freed ones go on the free list; strings past nstrings_ are
    // never read
    nodes_[0] = Node{};
    freeHead_ = 0;
    hwm_ = 1;
    live_ = 0;
    nstrings_ = 0;
    memset(block, 0, sizeof block);
    memset(comment, 0, sizeof comment);
    cpsExpr = 0;
}

uint16_t Program::alloc(Kind k) {
    uint16_t idx;
    if (freeHead_) {
        idx = freeHead_;
        freeHead_ = nodes_[idx].next;
    } else if (hwm_ < kMaxNodes) {
        idx = hwm_++;
    } else {
        return 0;
    }
    nodes_[idx] = Node{};
    nodes_[idx].kind = k;
    live_++;
    return idx;
}

void Program::freeTree(uint16_t idx) {
    if (!idx) return;
    uint16_t c = nodes_[idx].first;
    while (c) {
        uint16_t nx = nodes_[c].next;
        freeTree(c);
        c = nx;
    }
    nodes_[idx] = Node{};
    nodes_[idx].next = freeHead_;
    freeHead_ = idx;
    live_--;
}

void Program::appendChild(uint16_t parent, uint16_t c) {
    nodes_[c].next = 0;
    uint16_t* link = &nodes_[parent].first;
    while (*link) link = &nodes_[*link].next;
    *link = c;
}

uint16_t Program::child(uint16_t parent, int n) const {
    uint16_t c = nodes_[parent].first;
    while (c && n-- > 0) c = nodes_[c].next;
    return c;
}

int Program::childCount(uint16_t parent) const {
    int n = 0;
    for (uint16_t c = nodes_[parent].first; c; c = nodes_[c].next) n++;
    return n;
}

uint16_t Program::parentOf(uint16_t idx) const {
    if (!idx) return 0;
    for (int i = 1; i < hwm_; i++) {
        if (nodes_[i].kind == Kind::None) continue;
        for (uint16_t c = nodes_[i].first; c; c = nodes_[c].next)
            if (c == idx) return (uint16_t)i;
    }
    return 0;
}

uint16_t Program::prevSibling(uint16_t idx) const {
    uint16_t p = parentOf(idx);
    if (!p) return 0;
    uint16_t prev = 0;
    for (uint16_t c = nodes_[p].first; c; c = nodes_[c].next) {
        if (c == idx) return prev;
        prev = c;
    }
    return 0;
}

int Program::subtreeSize(uint16_t idx) const {
    if (!idx) return 0;
    int n = 1;
    for (uint16_t c = nodes_[idx].first; c; c = nodes_[c].next) n += subtreeSize(c);
    return n;
}

void Program::replace(uint16_t old, uint16_t repl) {
    uint16_t p = parentOf(old);
    nodes_[repl].next = nodes_[old].next;
    nodes_[old].next = 0;
    if (!p) {
        for (int s = 0; s < kSlots; s++)
            if (block[s] == old) block[s] = repl;
        if (cpsExpr == old) cpsExpr = repl;
        return;
    }
    uint16_t* link = &nodes_[p].first;
    while (*link && *link != old) link = &nodes_[*link].next;
    if (*link == old) *link = repl;
}

uint16_t Program::copyTree(const Program& src, uint16_t idx) {
    if (!idx) return 0;
    const Node& sn = src.at(idx);
    uint16_t n = alloc(sn.kind);
    if (!n) return 0;
    Node& dn = nodes_[n];
    dn.type = sn.type;
    dn.flags = sn.flags;
    dn.a = sn.a;
    dn.b = sn.b;
    // strings are per program: re-intern if this node refers to one
    if (&src != this && (sn.kind == Kind::MWord || sn.kind == Kind::MChord)) {
        const char* s = src.str((int)sn.a);
        int id = intern(s, (int)strlen(s));
        if (id < 0) {
            freeTree(n);
            return 0;
        }
        nodes_[n].a = (uint32_t)id;
    }
    for (uint16_t c = sn.first; c; c = src.at(c).next) {
        uint16_t cc = copyTree(src, c);
        if (!cc) {
            freeTree(n);
            return 0;
        }
        appendChild(n, cc);
    }
    return n;
}

int Program::intern(const char* s, int len) {
    if (len <= 0 || len > kMaxStringLen) return -1;
    for (int i = 0; i < nstrings_; i++)
        if ((int)strlen(strings_[i]) == len && memcmp(strings_[i], s, len) == 0) return i;
    if (nstrings_ >= kMaxStrings) return -1;
    memcpy(strings_[nstrings_], s, len);
    strings_[nstrings_][len] = 0;
    return nstrings_++;
}

const char* kindName(Kind k) {
    static const char* names[] = {
        "None",     "Block",   "Num",        "Mini",    "Fn",       "App",
        "Op",       "SectionL", "SectionR",  "Compose", "List",     "Hole",
        "MSeq",     "MSub",    "MAlt",       "MPoly",   "MChoice",  "MFast",
        "MSlow",    "MReplicate", "MWeight", "MDegrade", "MEuclid", "MRange",
        "MRest",    "MWord",   "MNum",       "MNote",   "MChord",   "MBool",
    };
    static_assert(sizeof(names) / sizeof(names[0]) == (size_t)Kind::Count, "names");
    return (size_t)k < (size_t)Kind::Count ? names[(size_t)k] : "?";
}

bool isMiniKind(Kind k) { return k >= Kind::MSeq && k <= Kind::MBool; }
bool isMiniAtom(Kind k) { return k >= Kind::MRest && k <= Kind::MBool; }
bool isMiniModifier(Kind k) { return k >= Kind::MFast && k <= Kind::MRange; }

}  // namespace tc
