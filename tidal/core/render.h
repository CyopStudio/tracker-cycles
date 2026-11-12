// Tree -> Tidal text (spec 1: round trip, minimal parentheses).
#pragma once
#include "tree.h"

namespace tc {

struct Span {
    uint16_t start, end;  // [start, end) in the output text; end 0 = not rendered
};

class Renderer {
public:
    // Output buffer and optional per-node spans (indexed by node id,
    // kMaxNodes entries).
    Renderer(char* out, int cap, Span* spans = nullptr) : out_(out), cap_(cap), spans_(spans) {}

    int expr(const Program& p, uint16_t root);   // expression only
    int block(const Program& p, int slot);       // "d1 $ ..." (no comment): the slot's selected block
    int blockNode(const Program& p, uint16_t b);  // any block root (several per dN)
    int cps(const Program& p);                   // "setcps (...)" (nothing without one)
    int file(const Program& p);                  // whole .tidal file
    bool overflow() const { return overflow_; }
    // Line breaks (kBreak / kBreakOp) as new lines (files, the screen); off:
    // spaces (statements for GHCi, labels).
    void setBreaks(bool on) { breaks_ = on; }
    int length() const { return n_; }

private:
    void put(char c);
    void puts(const char* s);
    void putRational(int32_t num, uint32_t den, int decimals);
    void node(const Program& p, uint16_t idx, int needPrec, int side);
    void mini(const Program& p, uint16_t idx);
    void miniArg(const Program& p, uint16_t seq);
    void begin(uint16_t idx);
    void end(uint16_t idx);

    char* out_;
    int cap_;
    Span* spans_;
    int n_ = 0;
    bool overflow_ = false;
    bool breaks_ = false;
    void sep(bool brk);  // " ", or a new line + indent
};

// Precedence of a rendered expression node (11 = atomic).
int exprPrec(const Program& p, uint16_t idx);

}  // namespace tc
