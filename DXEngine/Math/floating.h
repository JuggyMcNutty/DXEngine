#pragma once
#include <cmath>
#include <cfloat>
#include <cstdint>

// The original's float to int (MSVCRT's _ftol, RotatorConsole): truncated
// to 64 bits and the low 32 kept, so past an int's range it wraps; NaN, or
// past 64 bits, 0. A C++ cast is undefined there, INT_MIN on x86 and
// saturated on aarch64.
inline int ftol(double v)
{
	if (!(v >= -9223372036854775808.0 && v < 9223372036854775808.0))
		return 0;
	return (int)(uint32_t)(int64_t)v;
}

inline int ftol(float v)
{
	return ftol((double)v);
}

class Float
{
public:
	static inline bool Equals(float a, float b, float epsilon = FLT_EPSILON)
	{
		return (std::fabs(a - b) < epsilon);
	}
};

class Double
{
public:
	static inline bool Equals(double a, double b, double epsilon = DBL_EPSILON)
	{
		return (std::abs(a - b) < epsilon);
	}
};
