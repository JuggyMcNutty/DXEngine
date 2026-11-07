#pragma once
#include <bit>
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
// x87 holds it: at 24 bits, a float's, and past a float's range its 24 bits
// under the x87's own exponent, which holds the square of 2^64 or more
// (VSize); at 64, a double's, within a bit of the original's. What it stores
// is a float at both.
class X87
{
public:
	static inline bool Single = false;

	static inline double Round(double v)
	{
		if (!Single)
			return v;
		if (std::fabs(v) >= FLT_MIN && std::fabs(v) <= FLT_MAX)
			return (double)(float)v;
		// The significand to 24 bits, to nearest and ties to even, the
		// exponent kept
		uint64_t bits = std::bit_cast<uint64_t>(v);
		bits += 0x0fffffff + ((bits >> 29) & 1);
		return std::bit_cast<double>(bits & ~(uint64_t)0x1fffffff);
	}

	// A compare as the original's code makes it: an fcomp, then a test of
	// the x87's flags, C3 for equal and C0 for under, both of which a NaN on
	// either side sets (unordered). Equal is C3: the script's == and !=
	// (Core.dll 0x10135620, 0x10135700), a vector's (0x10138430,
	// 0x10138530), bool of a float or a vector (0x10131be0, 0x10136cd0) and
	// a float property's Identical (0x10166780), so a NaN equals anything.
	// Under is C0: the script's < (0x101352a0), ~= (0x101357e0), Abs
	// (0x10135c60), FMax (0x10136400), FClamp (0x101364e0) and Normal's
	// SafeNormal (0x10144be0), so a NaN is under anything. <= tests either
	// (0x10135460), > neither (0x10135380) and >= not C0 (0x10135540): a NaN
	// is under or equal to anything and over nothing (NaNConsole).
	static inline bool Equal(double a, double b)
	{
		return a == b || std::isunordered(a, b);
	}

	static inline bool Under(double a, double b)
	{
		return a < b || std::isunordered(a, b);
	}

	// Each axis Equal, as the original's vector == tests each in turn
	template<typename V>
	static inline bool Equal3(const V& a, const V& b)
	{
		return Equal(a.x, b.x) && Equal(a.y, b.y) && Equal(a.z, b.z);
	}

	// An int times a float made an int by _ftol (ftol), the product as the
	// x87 holds it: the original's rotator times a float (Core.dll
	// 0x101395e0: fild, fmul, _ftol). rot(10, 20, 30) * 0.7 is (7, 14, 21)
	// in the console's tick, each product rounded to 24 bits, and (6, 13, 20)
	// in the level's, each whole (SweepVmConsole).
	static inline int TimesFloat(int a, float b)
	{
		return ftol(Round((double)a * b));
	}

	// An int times one over a float made an int by _ftol (ftol), the
	// reciprocal and the product as the x87 holds them: the original's
	// rotator over a float (Core.dll 0x101397c0: fdivr of 1.0, then fild,
	// fmul, _ftol). At 64 bits the two roundings, each within 2^-64, take no
	// quotient within an int's range past a whole number, but a whole one, k,
	// comes out k - 1 where the reciprocal rounds down by enough: 1 / m to 64
	// bits, m the float's odd mantissa, is under it by rem / 2^s when rem, 2^s
	// mod m for s of 63 and m's bits, is under half m, and k times it rounds
	// under k past half the 64-bit step under k. rot(90, 180, 270) / 90 is
	// (1, 2, 3) in the console's tick and (0, 1, 3) in the level's
	// (SweepVmConsole). Past an int's range, where _ftol wraps, the low bits
	// are a double quotient's.
	static inline int OverFloat(int a, float b)
	{
		if (Single)
			return ftol(Round(a * Round(1.0 / b)));
		double q = (double)a / b;
		if (!(std::fabs(q) >= 1.0 && std::fabs(q) < 2147483648.0))
			return ftol(q);
		// The quotient's whole part: |a| 2^-e over m, the float being m 2^e
		int e;
		uint64_t m = (uint64_t)std::ldexp(std::frexp(std::fabs(b), &e), 24);
		int zeros = std::countr_zero(m);
		m >>= zeros;
		e += zeros - 24;
		uint64_t num = (uint64_t)(a < 0 ? -(int64_t)a : (int64_t)a) << (e < 0 ? -e : 0);
		uint64_t den = m << (e > 0 ? e : 0);
		uint64_t whole = num / den;
		if (num % den == 0 && m > 1)
		{
			int bits = std::bit_width(m);
			uint64_t rem = (((UINT64_MAX % m + 1) % m) << (bits - 1)) % m;
			int half = bits - 2 + std::bit_width(whole) - (std::has_single_bit(whole) ? 1 : 0);
			if (2 * rem < m && whole * rem > (uint64_t)1 << half)
				whole--;
		}
		return q < 0.0 ? -(int)whole : (int)whole;
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
