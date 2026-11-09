// Exact time for the query engine: 32-bit rationals, GCD-normalised
// (CLAUDE.md "Engine constraints"). Intermediate maths in 64 bits; a result
// that doesn't fit 32 bits sets qOverflow() and yields 0.
#pragma once
#include <stdint.h>

namespace tc {

struct Q {
    int32_t n;
    int32_t d;  // > 0
};

bool& qOverflowFlag();
inline bool qOverflow() { return qOverflowFlag(); }

int64_t qgcd(int64_t a, int64_t b);

inline Q qmake(int64_t n, int64_t d) {
    if (d == 0) {
        qOverflowFlag() = true;
        return Q{0, 1};
    }
    if (d < 0) {
        n = -n;
        d = -d;
    }
    int64_t g = qgcd(n < 0 ? -n : n, d);
    if (g > 1) {
        n /= g;
        d /= g;
    }
    if (n > INT32_MAX || n < INT32_MIN || d > INT32_MAX) {
        qOverflowFlag() = true;
        return Q{0, 1};
    }
    return Q{(int32_t)n, (int32_t)d};
}
inline Q qint(int64_t v) { return qmake(v, 1); }
inline Q operator+(Q a, Q b) { return qmake((int64_t)a.n * b.d + (int64_t)b.n * a.d, (int64_t)a.d * b.d); }
inline Q operator-(Q a, Q b) { return qmake((int64_t)a.n * b.d - (int64_t)b.n * a.d, (int64_t)a.d * b.d); }
inline Q operator-(Q a) { return Q{-a.n, a.d}; }
inline Q operator*(Q a, Q b) { return qmake((int64_t)a.n * b.n, (int64_t)a.d * b.d); }
inline Q operator/(Q a, Q b) { return qmake((int64_t)a.n * b.d, (int64_t)a.d * b.n); }
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
inline int64_t qfloor(Q a) { return a.n >= 0 ? a.n / a.d : -(((int64_t)-a.n + a.d - 1) / a.d); }
inline Q qsam(Q a) { return qint(qfloor(a)); }
inline double qdouble(Q a) { return (double)a.n / (double)a.d; }

// A time span [s, e). s == e is a point ("zero-width") query.
struct Arc {
    Q s, e;
};

// Intersection of two arcs, following Tidal's rules for touching and
// zero-width arcs. False if they don't overlap.
bool subArc(Arc a, Arc b, Arc& out);

}  // namespace tc
