#ifndef UTIL_MATH_H_
#define UTIL_MATH_H_

#include <polyassert.h>

template<class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi)
{
    assert(!(hi < lo));
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

template<class T>
constexpr const T map(const T& v, const T& in_lo, const T& in_hi, const T& out_lo, const T& out_hi)
{
    return (v - in_lo) * (out_hi - out_lo) / (in_hi - in_lo) + out_lo;
}

template<class T>
constexpr const T mapClamped(const T& v,
                             const T& in_lo,
                             const T& in_hi,
                             const T& out_lo,
                             const T& out_hi)
{
    return map(in_lo > in_hi ? clamp(v, in_hi, in_lo) : clamp(v, in_lo, in_hi),
               in_lo,
               in_hi,
               out_lo,
               out_hi);
}

#endif /* UTIL_MATH_H_ */
