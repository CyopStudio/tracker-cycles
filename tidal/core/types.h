// Type terms, signatures and unification (spec section 3).
#pragma once
#include <stdint.h>

namespace tc {

enum class Base : uint8_t { Time, Double, Int, Note, String, Bool, Control, Count };
const char* baseName(Base b);

// Constraint bits on type variables
constexpr uint8_t kCNum = 1 << 0;      // Num: Time Double Int Note Control
constexpr uint8_t kCFrac = 1 << 1;     // Fractional: Time Double Note Control
constexpr uint8_t kCLit = 1 << 2;      // numeric literal: Time Double Int Note
constexpr uint8_t kCFracLit = 1 << 3;  // decimal literal: Time Double Note
bool baseSatisfies(Base b, uint8_t constraints);

enum class Tag : uint8_t { Var, Base, Pat, Fn, List };

struct Term {
    Tag tag;
    Base base;     // Tag::Base
    uint8_t cons;  // Tag::Var: constraint bits
    uint8_t pad;
    uint16_t a;    // Var: bound term (0 = free); Pat/List: element; Fn: arg
    uint16_t b;    // Fn: result
};

// A pool of type terms used while checking one block. Index 0 is unused.
class TypePool {
public:
    static constexpr int kMax = 2048;
    void clear() { n_ = 1; }
    uint16_t var(uint8_t cons = 0);
    uint16_t base(Base b);
    uint16_t pat(uint16_t elem);
    uint16_t fn(uint16_t arg, uint16_t res);
    uint16_t list(uint16_t elem);
    bool full() const { return n_ >= kMax; }

    uint16_t resolve(uint16_t t) const;   // follow bound variables
    const Term& at(uint16_t t) const { return t_[t]; }
    // Unify a and b. On failure returns false and leaves a message.
    bool unify(uint16_t a, uint16_t b, char* msg, int msgLen);
    bool occurs(uint16_t v, uint16_t t) const;
    bool isGround(uint16_t t) const;

    // Parse a signature such as "Num a => Pat Time -> Pat a -> Pat a" and
    // instantiate it with fresh variables. Returns 0 on syntax error.
    uint16_t instantiate(const char* sig);

    // Human-readable form ("Pat Time -> Pat a") into buf.
    void print(uint16_t t, char* buf, int len) const;

private:
    uint16_t add(Term t);
    Term t_[kMax];
    int n_ = 1;
};

}  // namespace tc
