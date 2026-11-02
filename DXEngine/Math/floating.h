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

// The precision the original's x87 works its sums out at: 24 bits from its
// render's PreRender to its PostRender (Render.dll 0x10b14560 and
// 0x10b14700, through Core.dll's appEnableFastMath, 0x1016e050), which
// UGameEngine::Draw calls before the console's PreRender, which ticks the
// console, and after the console's PostRender (Engine.dll 0x1038f66c to
// 0x1038f79b): so in the console's tick and the frame's script events; 64
// bits outside them, in the level's tick. Round gives a step's result as the
// x87 holds it: a float's at 24 bits; at 64, a double's, within a bit of the
// original's. What it stores is a float at both.
class X87
{
public:
	static inline bool Single = false;

	static inline double Round(double v)
	{
		return Single ? (double)(float)v : v;
	}
};

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
