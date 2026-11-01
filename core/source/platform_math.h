#ifndef SOURCE_PLATFORM_MATH_H_
#define SOURCE_PLATFORM_MATH_H_
#include "stdint.h"

extern int16_t sin_q15(int16_t value);
#define SIN_Q15(value) sin_q15(value)

#endif /* SOURCE_PLATFORM_MATH_H_ */
