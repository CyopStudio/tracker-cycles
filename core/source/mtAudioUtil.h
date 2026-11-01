#ifndef SOURCE_MTAUDIOUTIL_H_
#define SOURCE_MTAUDIOUTIL_H_

// Interpolates signal, with given t position in range <0, 1>
// if t == 0 returns x1
// if t == 1 returns x2
static inline int16_t interpolateCubic(int16_t x0, int16_t x1, int16_t x2, int16_t x3, float t)
{
    int32_t a0, a1, a2, a3;
    a0 = x3 - x2 - x0 + x1;
    a1 = x0 - x1 - a0;
    a2 = x2 - x0;
    a3 = x1;
    int32_t out = (a0 * (t * t * t)) + (a1 * (t * t)) + (a2 * t) + (a3);

    return out > INT16_MAX ? INT16_MAX : (out < INT16_MIN ? INT16_MIN : out);
}

#endif
