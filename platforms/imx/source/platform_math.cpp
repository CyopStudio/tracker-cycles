#include "stdint.h"

extern "C"
{
    int16_t arm_sin_q15(int16_t);
}

int16_t sin_q15(int16_t value)
{
    return arm_sin_q15(value);
}
