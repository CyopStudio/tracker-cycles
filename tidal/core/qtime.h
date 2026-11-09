// Exact time for the query engine: 32-bit rationals, GCD-normalised
// (CLAUDE.md "Engine constraints"). A result that doesn't fit 32 bits sets
// qOverflow() and yields 0.
#pragma once
#include <stdint.h>

namespace tc {

struct Q {
    int32_t n;
    int32_t d;  // > 0
};

// Set by any result that doesn't fit; callers save / clear / check it.
inline bool gQOverflow = false;
inline bool& qOverflowFlag() { return gQOverflow; }
inline bool qOverflow() { return gQOverflow; }

// The MK66 (Cortex-M4) divides 32-bit integers in hardware but 64-bit ones
// in software (hundreds of cycles), so everything below stays in 32 bits
// whenever the values fit (nearly always: Tidal times have small
// denominators) and only falls back to 64-bit maths when they don't.
inline uint32_t qgcd32(uint32_t a, uint32_t b) {
    while (b) {
        uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}
// 64-bit fallback, binary GCD (no division). a, b >= 0.
inline uint64_t qgcd64(uint64_t a, uint64_t b) {
    if (!a) return b;
    if (!b) return a;
    int k = __builtin_ctzll(a | b);
    a >>= __builtin_ctzll(a);
    do {
        b >>= __builtin_ctzll(b);
        if (a > b) {
            uint64_t t = a;
            a = b;
            b = t;
        }
        b -= a;
    } while (b);
    return a << k;
}
inline bool qfits(int64_t v) { return v >= INT32_MIN && v <= INT32_MAX; }
inline Q qover() {
    gQOverflow = true;
    return Q{0, 1};
}

// n / d, normalised.
inline Q qmake(int64_t n, int64_t d) {
    if (d == 0) return qover();
    if (d < 0) {
        n = -n;
        d = -d;
    }
    if (d == 1) return qfits(n) ? Q{(int32_t)n, 1} : qover();
    if (qfits(n) && d <= INT32_MAX) {
        int32_t n32 = (int32_t)n, d32 = (int32_t)d;
        uint32_t g = qgcd32(n32 < 0 ? 0u - (uint32_t)n32 : (uint32_t)n32, (uint32_t)d32);
        if (g > 1) {
            n32 /= (int32_t)g;
            d32 /= (int32_t)g;
        }
        return Q{n32, d32};
    }
    uint64_t g = qgcd64(n < 0 ? 0 - (uint64_t)n : (uint64_t)n, (uint64_t)d);
    if (g > 1) {
        n /= (int64_t)g;
        d /= (int64_t)g;
    }
    if (!qfits(n) || d > INT32_MAX) return qover();
    return Q{(int32_t)n, (int32_t)d};
}
inline Q qint(int64_t v) { return qfits(v) ? Q{(int32_t)v, 1} : qover(); }

inline Q operator+(Q a, Q b) {
    // integer + normalised fraction stays normalised: no GCD
    if (a.d == 1 && b.d == 1) return qint((int64_t)a.n + b.n);
    if (a.d == 1) return qfits((int64_t)a.n * b.d + b.n) ? Q{(int32_t)((int64_t)a.n * b.d + b.n), b.d} : qover();
    if (b.d == 1) return qfits((int64_t)b.n * a.d + a.n) ? Q{(int32_t)((int64_t)b.n * a.d + a.n), a.d} : qover();
    if (a.d == b.d) return qmake((int64_t)a.n + b.n, a.d);
    int64_t sum;
    if (__builtin_add_overflow((int64_t)a.n * b.d, (int64_t)b.n * a.d, &sum)) return qover();
    return qmake(sum, (int64_t)a.d * b.d);
}
inline Q operator-(Q a) { return a.n == INT32_MIN ? qover() : Q{-a.n, a.d}; }
inline Q operator-(Q a, Q b) { return b.n == INT32_MIN ? qmake((int64_t)a.n * b.d - (int64_t)b.n * a.d, (int64_t)a.d * b.d) : a + Q{-b.n, b.d}; }
// Cross-reduce first: (a.n/g1 * b.n/g2) / (a.d/g2 * b.d/g1) is already
// normalised, so no GCD of the (bigger) products.
inline Q qmulCross(int32_t an, int32_t ad, int32_t bn, int32_t bd) {
    uint32_t g1 = qgcd32(an < 0 ? 0u - (uint32_t)an : (uint32_t)an, (uint32_t)bd);
    uint32_t g2 = qgcd32(bn < 0 ? 0u - (uint32_t)bn : (uint32_t)bn, (uint32_t)ad);
    if (g1 > 1) {
        an /= (int32_t)g1;
        bd /= (int32_t)g1;
    }
    if (g2 > 1) {
        bn /= (int32_t)g2;
        ad /= (int32_t)g2;
    }
    int64_t n = (int64_t)an * bn, d = (int64_t)ad * bd;
    if (!qfits(n) || d > INT32_MAX) return qover();
    return Q{(int32_t)n, (int32_t)d};
}
inline Q operator*(Q a, Q b) {
    if (a.d == 1 && b.d == 1) return qint((int64_t)a.n * b.n);
    return qmulCross(a.n, a.d, b.n, b.d);
}
inline Q operator/(Q a, Q b) {
    if (b.n == 0) return qover();
    if (b.n < 0) {
        if (b.n == INT32_MIN || a.n == INT32_MIN) return qmake((int64_t)a.n * b.d, (int64_t)a.d * b.n);
        b = Q{-b.n, b.d};
        a = Q{-a.n, a.d};
    }
    return qmulCross(a.n, a.d, b.d, b.n);
}
inline int qcmp(Q a, Q b) {
    int64_t l = (int64_t)a.n * b.d, r = (int64_t)b.n * a.d;
    return l < r ? -1 : l > r ? 1 : 0;
}
inline bool operator<(Q a, Q b) { return qcmp(a, b) < 0; }
inline bool operator>(Q a, Q b) { return qcmp(a, b) > 0; }
inline bool operator<=(Q a, Q b) { return qcmp(a, b) <= 0; }
inline bool operator>=(Q a, Q b) { return qcmp(a, b) >= 0; }
inline bool operator==(Q a, Q b) { return a.n == b.n && a.d == b.d; }
inline bool operator!=(Q a, Q b) { return !(a == b); }
inline Q qmin(Q a, Q b) { return a < b ? a : b; }
inline Q qmax(Q a, Q b) { return a > b ? a : b; }
// floor, as an integer cycle number ("sam")
inline int64_t qfloor(Q a) {
    if (a.d == 1) return a.n;
    int32_t q = a.n / a.d;  // 32-bit: hardware divide, truncates toward 0
    return (a.n < 0 && q * a.d != a.n) ? q - 1 : q;
}
inline Q qsam(Q a) { return qint(qfloor(a)); }

// A time span [s, e). s == e is a point ("zero-width") query.
struct Arc {
    Q s, e;
};

// Intersection of two arcs, following Tidal's rules for touching and
// zero-width arcs. False if they don't overlap.
bool subArc(Arc a, Arc b, Arc& out);

}  // namespace tc
