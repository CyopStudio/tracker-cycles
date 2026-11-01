#pragma once

#include <cassert>
#include <cmath>
#include <numbers>

inline auto scaleLin(auto value, auto inLow, auto inHi, auto outLow, auto outHi)
{
    return outLow + (value - inLow) * (outHi - outLow) / (inHi - inLow);
}

inline auto interpLin(auto x, auto x1, auto x2, auto y1, auto y2)
{
    auto dydx = (y2 - y1) / (x2 - x1);
    return y1 + dydx * (x - x1);
}

// maps range [0..1] to [-1..1]
inline auto bipolar(auto value)
{
    return 2.0f * value - 1.0f;
}

// approximatio of a sine function
inline auto parabolicSine(auto angle)
{
    constexpr decltype(angle) B = 4.0f / std::numbers::pi_v<decltype(angle)>;
    constexpr decltype(angle) C =
      -4.0f / (std::numbers::pi_v<decltype(angle)> * std::numbers::pi_v<decltype(angle)>);
    constexpr decltype(angle) P = 0.225f;

    auto y = B * angle + C * angle * std::abs(angle);
    return P * (y * std::abs(y) - y) + y;
}

// tanh aproximation
inline auto fast_tanh(auto x)
{
    auto x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

template<class T>
inline constexpr const T& clamp(const T& v, const T& lo, const T& hi)
{
    assert(!(hi < lo));
    return (v < lo) ? lo : (hi < v) ? hi : v;
}