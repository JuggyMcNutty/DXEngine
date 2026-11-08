#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>

// DXE_SW_SCALAR, defined for a build, takes the loops on any machine: a
// scalar build's frame to compare with a vector build's
#if defined(USE_SSE2) && !defined(DXE_SW_SCALAR)
#define SOFTWARE_SSE2
#include <emmintrin.h>
#elif defined(USE_NEON) && !defined(DXE_SW_SCALAR)
#define SOFTWARE_NEON
#include <arm_neon.h>
#endif

// The vectors the software device's pipelines are written in, four lanes of
// a float or an int32, or eight of 16 bits: SSE2 on x86 (Precomp.h's
// USE_SSE2), NEON on ARM (USE_NEON), else plain loops. Each operation gives
// the same bits on all three: a scalar build's frame is an SSE2 build's.
namespace SoftwareSimd
{
#if defined(SOFTWARE_SSE2)
	struct F4 { __m128 v; };
	struct I4 { __m128i v; };
	struct W8 { __m128i v; }; // eight lanes of 16 bits, unsigned unless said
#elif defined(SOFTWARE_NEON)
	struct F4 { float32x4_t v; };
	struct I4 { int32x4_t v; };
	struct W8 { uint16x8_t v; };
#else
	struct F4 { float v[4]; };
	struct I4 { int32_t v[4]; };
	struct W8 { uint16_t v[8]; };
#endif

	// Floats

	inline F4 SetF(float a)
	{
#if defined(SOFTWARE_SSE2)
		return { _mm_set1_ps(a) };
#elif defined(SOFTWARE_NEON)
		return { vdupq_n_f32(a) };
#else
		return { { a, a, a, a } };
#endif
	}

	inline F4 SetF(float a, float b, float c, float d)
	{
#if defined(SOFTWARE_SSE2)
		return { _mm_setr_ps(a, b, c, d) };
#elif defined(SOFTWARE_NEON)
		float t[4] = { a, b, c, d };
		return { vld1q_f32(t) };
#else
		return { { a, b, c, d } };
#endif
	}

	inline F4 LoadF(const float* p)
	{
#if defined(SOFTWARE_SSE2)
		return { _mm_loadu_ps(p) };
#elif defined(SOFTWARE_NEON)
		return { vld1q_f32(p) };
#else
		F4 r;
		std::memcpy(r.v, p, sizeof(r.v));
		return r;
#endif
	}

	inline void StoreF(float* p, F4 a)
	{
#if defined(SOFTWARE_SSE2)
		_mm_storeu_ps(p, a.v);
#elif defined(SOFTWARE_NEON)
		vst1q_f32(p, a.v);
#else
		std::memcpy(p, a.v, sizeof(a.v));
#endif
	}

#if defined(SOFTWARE_SSE2)
	inline F4 operator+(F4 a, F4 b) { return { _mm_add_ps(a.v, b.v) }; }
	inline F4 operator-(F4 a, F4 b) { return { _mm_sub_ps(a.v, b.v) }; }
	inline F4 operator*(F4 a, F4 b) { return { _mm_mul_ps(a.v, b.v) }; }
	inline F4 operator/(F4 a, F4 b) { return { _mm_div_ps(a.v, b.v) }; }
	inline F4 Min(F4 a, F4 b) { return { _mm_min_ps(a.v, b.v) }; }
	inline F4 Max(F4 a, F4 b) { return { _mm_max_ps(a.v, b.v) }; }
#elif defined(SOFTWARE_NEON)
	inline F4 operator+(F4 a, F4 b) { return { vaddq_f32(a.v, b.v) }; }
	inline F4 operator-(F4 a, F4 b) { return { vsubq_f32(a.v, b.v) }; }
	inline F4 operator*(F4 a, F4 b) { return { vmulq_f32(a.v, b.v) }; }
	inline F4 operator/(F4 a, F4 b) { return { vdivq_f32(a.v, b.v) }; }
	// As SSE2's minps and maxps: b where either is NaN
	inline F4 Min(F4 a, F4 b) { return { vbslq_f32(vcltq_f32(a.v, b.v), a.v, b.v) }; }
	inline F4 Max(F4 a, F4 b) { return { vbslq_f32(vcgtq_f32(a.v, b.v), a.v, b.v) }; }
#else
	inline F4 operator+(F4 a, F4 b) { F4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] + b.v[i]; return r; }
	inline F4 operator-(F4 a, F4 b) { F4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] - b.v[i]; return r; }
	inline F4 operator*(F4 a, F4 b) { F4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] * b.v[i]; return r; }
	inline F4 operator/(F4 a, F4 b) { F4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] / b.v[i]; return r; }
	inline F4 Min(F4 a, F4 b) { F4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] < b.v[i] ? a.v[i] : b.v[i]; return r; }
	inline F4 Max(F4 a, F4 b) { F4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] > b.v[i] ? a.v[i] : b.v[i]; return r; }
#endif

	inline float Lane(F4 a, int i)
	{
		float t[4];
		StoreF(t, a);
		return t[i];
	}

	// Int32s

	inline I4 SetI(int32_t a)
	{
#if defined(SOFTWARE_SSE2)
		return { _mm_set1_epi32(a) };
#elif defined(SOFTWARE_NEON)
		return { vdupq_n_s32(a) };
#else
		return { { a, a, a, a } };
#endif
	}

	inline I4 SetI(int32_t a, int32_t b, int32_t c, int32_t d)
	{
#if defined(SOFTWARE_SSE2)
		return { _mm_setr_epi32(a, b, c, d) };
#elif defined(SOFTWARE_NEON)
		int32_t t[4] = { a, b, c, d };
		return { vld1q_s32(t) };
#else
		return { { a, b, c, d } };
#endif
	}

	inline void StoreI(int32_t* p, I4 a)
	{
#if defined(SOFTWARE_SSE2)
		_mm_storeu_si128((__m128i*)p, a.v);
#elif defined(SOFTWARE_NEON)
		vst1q_s32(p, a.v);
#else
		std::memcpy(p, a.v, sizeof(a.v));
#endif
	}

	inline I4 LoadI(const int32_t* p)
	{
#if defined(SOFTWARE_SSE2)
		return { _mm_loadu_si128((const __m128i*)p) };
#elif defined(SOFTWARE_NEON)
		return { vld1q_s32(p) };
#else
		I4 r;
		std::memcpy(r.v, p, sizeof(r.v));
		return r;
#endif
	}

#if defined(SOFTWARE_SSE2)
	inline I4 operator+(I4 a, I4 b) { return { _mm_add_epi32(a.v, b.v) }; }
	inline I4 operator-(I4 a, I4 b) { return { _mm_sub_epi32(a.v, b.v) }; }
	inline I4 operator&(I4 a, I4 b) { return { _mm_and_si128(a.v, b.v) }; }
	inline I4 operator|(I4 a, I4 b) { return { _mm_or_si128(a.v, b.v) }; }
	inline I4 AndNot(I4 a, I4 b) { return { _mm_andnot_si128(a.v, b.v) }; } // ~a & b
	inline I4 CmpGt(I4 a, I4 b) { return { _mm_cmpgt_epi32(a.v, b.v) }; }
	inline I4 ShiftRight(I4 a, int n) { return { _mm_srai_epi32(a.v, n) }; }
	inline I4 ShiftLeft(I4 a, int n) { return { _mm_slli_epi32(a.v, n) }; }
	inline I4 CmpLtF(F4 a, F4 b) { return { _mm_castps_si128(_mm_cmplt_ps(a.v, b.v)) }; }
	inline I4 CmpLeF(F4 a, F4 b) { return { _mm_castps_si128(_mm_cmple_ps(a.v, b.v)) }; }
	inline I4 Truncate(F4 a) { return { _mm_cvttps_epi32(a.v) }; }
	inline F4 ToFloat(I4 a) { return { _mm_cvtepi32_ps(a.v) }; }
	// A lane's sign bit each, lane 0 the lowest
	inline int Mask(I4 a) { return _mm_movemask_ps(_mm_castsi128_ps(a.v)); }
	inline I4 Select(I4 mask, I4 a, I4 b) { return { _mm_or_si128(_mm_and_si128(mask.v, a.v), _mm_andnot_si128(mask.v, b.v)) }; }
	inline F4 Select(I4 mask, F4 a, F4 b) { __m128 m = _mm_castsi128_ps(mask.v); return { _mm_or_ps(_mm_and_ps(m, a.v), _mm_andnot_ps(m, b.v)) }; }
#elif defined(SOFTWARE_NEON)
	inline I4 operator+(I4 a, I4 b) { return { vaddq_s32(a.v, b.v) }; }
	inline I4 operator-(I4 a, I4 b) { return { vsubq_s32(a.v, b.v) }; }
	inline I4 operator&(I4 a, I4 b) { return { vandq_s32(a.v, b.v) }; }
	inline I4 operator|(I4 a, I4 b) { return { vorrq_s32(a.v, b.v) }; }
	inline I4 AndNot(I4 a, I4 b) { return { vbicq_s32(b.v, a.v) }; }
	inline I4 CmpGt(I4 a, I4 b) { return { vreinterpretq_s32_u32(vcgtq_s32(a.v, b.v)) }; }
	inline I4 ShiftRight(I4 a, int n) { return { vshlq_s32(a.v, vdupq_n_s32(-n)) }; }
	inline I4 ShiftLeft(I4 a, int n) { return { vshlq_s32(a.v, vdupq_n_s32(n)) }; }
	inline I4 CmpLtF(F4 a, F4 b) { return { vreinterpretq_s32_u32(vcltq_f32(a.v, b.v)) }; }
	inline I4 CmpLeF(F4 a, F4 b) { return { vreinterpretq_s32_u32(vcleq_f32(a.v, b.v)) }; }
	// As cvttps2dq: toward zero, 0x80000000 where out of range or NaN
	inline I4 Truncate(F4 a)
	{
		uint32x4_t inRange = vandq_u32(vcltq_f32(vabsq_f32(a.v), vdupq_n_f32(2147483648.0f)), vceqq_f32(a.v, a.v));
		return { vbslq_s32(inRange, vcvtq_s32_f32(a.v), vdupq_n_s32((int32_t)0x80000000)) };
	}
	inline F4 ToFloat(I4 a) { return { vcvtq_f32_s32(a.v) }; }
	inline int Mask(I4 a)
	{
		uint32x4_t s = vshrq_n_u32(vreinterpretq_u32_s32(a.v), 31);
		return (int)(vgetq_lane_u32(s, 0) | (vgetq_lane_u32(s, 1) << 1) | (vgetq_lane_u32(s, 2) << 2) | (vgetq_lane_u32(s, 3) << 3));
	}
	inline I4 Select(I4 mask, I4 a, I4 b) { return { vbslq_s32(vreinterpretq_u32_s32(mask.v), a.v, b.v) }; }
	inline F4 Select(I4 mask, F4 a, F4 b) { return { vbslq_f32(vreinterpretq_u32_s32(mask.v), a.v, b.v) }; }
#else
	inline I4 operator+(I4 a, I4 b) { I4 r; for (int i = 0; i < 4; i++) r.v[i] = (int32_t)((uint32_t)a.v[i] + (uint32_t)b.v[i]); return r; }
	inline I4 operator-(I4 a, I4 b) { I4 r; for (int i = 0; i < 4; i++) r.v[i] = (int32_t)((uint32_t)a.v[i] - (uint32_t)b.v[i]); return r; }
	inline I4 operator&(I4 a, I4 b) { I4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] & b.v[i]; return r; }
	inline I4 operator|(I4 a, I4 b) { I4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] | b.v[i]; return r; }
	inline I4 AndNot(I4 a, I4 b) { I4 r; for (int i = 0; i < 4; i++) r.v[i] = ~a.v[i] & b.v[i]; return r; }
	inline I4 CmpGt(I4 a, I4 b) { I4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] > b.v[i] ? -1 : 0; return r; }
	inline I4 ShiftRight(I4 a, int n) { I4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] >> n; return r; }
	inline I4 ShiftLeft(I4 a, int n) { I4 r; for (int i = 0; i < 4; i++) r.v[i] = (int32_t)((uint32_t)a.v[i] << n); return r; }
	inline I4 CmpLtF(F4 a, F4 b) { I4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] < b.v[i] ? -1 : 0; return r; }
	inline I4 CmpLeF(F4 a, F4 b) { I4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] <= b.v[i] ? -1 : 0; return r; }
	inline I4 Truncate(F4 a)
	{
		I4 r;
		for (int i = 0; i < 4; i++)
			r.v[i] = (a.v[i] > -2147483648.0f && a.v[i] < 2147483648.0f) ? (int32_t)a.v[i] : (int32_t)0x80000000;
		return r;
	}
	inline F4 ToFloat(I4 a) { F4 r; for (int i = 0; i < 4; i++) r.v[i] = (float)a.v[i]; return r; }
	inline int Mask(I4 a) { int m = 0; for (int i = 0; i < 4; i++) m |= (a.v[i] < 0 ? 1 : 0) << i; return m; }
	inline I4 Select(I4 mask, I4 a, I4 b) { I4 r; for (int i = 0; i < 4; i++) r.v[i] = (mask.v[i] & a.v[i]) | (~mask.v[i] & b.v[i]); return r; }
	inline F4 Select(I4 mask, F4 a, F4 b) { F4 r; for (int i = 0; i < 4; i++) r.v[i] = mask.v[i] ? a.v[i] : b.v[i]; return r; }
#endif

	// Rounded down, for the floats a texture coordinate holds. From 2^23 on
	// a float is a whole number, its own floor, which Truncate, with no int
	// past 2^31 (INT_MIN there), cannot give: a coordinate of 2^31 texels or
	// more wrapped to some -2^31, a read gigabytes before the texture
	inline F4 Floor(F4 a)
	{
		F4 t = ToFloat(Truncate(a));
		// Truncated toward zero: one less where that rounded a negative up
		t = Select(CmpLtF(a, t), t - SetF(1.0f), t);
		I4 fractional = CmpLtF(a, SetF(8388608.0f)) & CmpLtF(SetF(-8388608.0f), a);
		return Select(fractional, t, a);
	}

	inline int32_t Lane(I4 a, int i)
	{
		int32_t t[4];
		StoreI(t, a);
		return t[i];
	}

	// 16-bit lanes

	inline W8 SetW(uint16_t a)
	{
#if defined(SOFTWARE_SSE2)
		return { _mm_set1_epi16((short)a) };
#elif defined(SOFTWARE_NEON)
		return { vdupq_n_u16(a) };
#else
		W8 r;
		for (int i = 0; i < 8; i++) r.v[i] = a;
		return r;
#endif
	}

	// Four lanes' values for four channels each: the lane's in all four of its
	// lanes, two lanes a vector
	inline W8 SetW4(uint16_t a, uint16_t b)
	{
#if defined(SOFTWARE_SSE2)
		return { _mm_setr_epi16((short)a, (short)a, (short)a, (short)a, (short)b, (short)b, (short)b, (short)b) };
#elif defined(SOFTWARE_NEON)
		return { vcombine_u16(vdup_n_u16(a), vdup_n_u16(b)) };
#else
		W8 r;
		for (int i = 0; i < 4; i++) { r.v[i] = a; r.v[i + 4] = b; }
		return r;
#endif
	}

#if defined(SOFTWARE_SSE2)
	inline W8 operator^(W8 a, W8 b) { return { _mm_xor_si128(a.v, b.v) }; }
	inline W8 operator+(W8 a, W8 b) { return { _mm_add_epi16(a.v, b.v) }; }
	inline W8 operator-(W8 a, W8 b) { return { _mm_sub_epi16(a.v, b.v) }; }
	inline W8 AddSat(W8 a, W8 b) { return { _mm_adds_epu16(a.v, b.v) }; }
	inline W8 SubSat(W8 a, W8 b) { return { _mm_subs_epu16(a.v, b.v) }; }
	inline W8 MulLow(W8 a, W8 b) { return { _mm_mullo_epi16(a.v, b.v) }; }
	inline W8 MulHigh(W8 a, W8 b) { return { _mm_mulhi_epu16(a.v, b.v) }; } // (a * b) >> 16, unsigned
	// (a * b + 32768) >> 16: the low half's top bit added to the high half
	inline W8 MulHighRound(W8 a, W8 b) { return { _mm_add_epi16(_mm_mulhi_epu16(a.v, b.v), _mm_srli_epi16(_mm_mullo_epi16(a.v, b.v), 15)) }; }
	inline W8 ShiftRight(W8 a, int n) { return { _mm_srli_epi16(a.v, n) }; }
	inline W8 ShiftLeft(W8 a, int n) { return { _mm_slli_epi16(a.v, n) }; }
	inline W8 operator&(W8 a, W8 b) { return { _mm_and_si128(a.v, b.v) }; }
	inline W8 operator|(W8 a, W8 b) { return { _mm_or_si128(a.v, b.v) }; }
	// The lesser, unsigned (SSE2 has no pminuw)
	inline W8 Min(W8 a, W8 b) { return { _mm_sub_epi16(a.v, _mm_subs_epu16(a.v, b.v)) }; }
#elif defined(SOFTWARE_NEON)
	inline W8 operator^(W8 a, W8 b) { return { veorq_u16(a.v, b.v) }; }
	inline W8 operator+(W8 a, W8 b) { return { vaddq_u16(a.v, b.v) }; }
	inline W8 operator-(W8 a, W8 b) { return { vsubq_u16(a.v, b.v) }; }
	inline W8 AddSat(W8 a, W8 b) { return { vqaddq_u16(a.v, b.v) }; }
	inline W8 SubSat(W8 a, W8 b) { return { vqsubq_u16(a.v, b.v) }; }
	inline W8 MulLow(W8 a, W8 b) { return { vmulq_u16(a.v, b.v) }; }
	inline W8 MulHigh(W8 a, W8 b)
	{
		uint32x4_t lo = vmull_u16(vget_low_u16(a.v), vget_low_u16(b.v));
		uint32x4_t hi = vmull_u16(vget_high_u16(a.v), vget_high_u16(b.v));
		return { vcombine_u16(vshrn_n_u32(lo, 16), vshrn_n_u32(hi, 16)) };
	}
	inline W8 MulHighRound(W8 a, W8 b)
	{
		uint32x4_t lo = vmull_u16(vget_low_u16(a.v), vget_low_u16(b.v));
		uint32x4_t hi = vmull_u16(vget_high_u16(a.v), vget_high_u16(b.v));
		return { vcombine_u16(vrshrn_n_u32(lo, 16), vrshrn_n_u32(hi, 16)) };
	}
	inline W8 ShiftRight(W8 a, int n) { return { vshlq_u16(a.v, vdupq_n_s16((int16_t)-n)) }; }
	inline W8 ShiftLeft(W8 a, int n) { return { vshlq_u16(a.v, vdupq_n_s16((int16_t)n)) }; }
	inline W8 operator&(W8 a, W8 b) { return { vandq_u16(a.v, b.v) }; }
	inline W8 operator|(W8 a, W8 b) { return { vorrq_u16(a.v, b.v) }; }
	inline W8 Min(W8 a, W8 b) { return { vminq_u16(a.v, b.v) }; }
#else
	inline W8 operator^(W8 a, W8 b) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = a.v[i] ^ b.v[i]; return r; }
	inline W8 operator+(W8 a, W8 b) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = (uint16_t)(a.v[i] + b.v[i]); return r; }
	inline W8 operator-(W8 a, W8 b) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = (uint16_t)(a.v[i] - b.v[i]); return r; }
	inline W8 AddSat(W8 a, W8 b) { W8 r; for (int i = 0; i < 8; i++) { uint32_t s = (uint32_t)a.v[i] + b.v[i]; r.v[i] = (uint16_t)(s > 65535 ? 65535 : s); } return r; }
	inline W8 SubSat(W8 a, W8 b) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = a.v[i] > b.v[i] ? (uint16_t)(a.v[i] - b.v[i]) : 0; return r; }
	inline W8 MulLow(W8 a, W8 b) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = (uint16_t)((uint32_t)a.v[i] * b.v[i]); return r; }
	inline W8 MulHigh(W8 a, W8 b) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = (uint16_t)(((uint32_t)a.v[i] * b.v[i]) >> 16); return r; }
	inline W8 MulHighRound(W8 a, W8 b) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = (uint16_t)(((uint32_t)a.v[i] * b.v[i] + 32768) >> 16); return r; }
	inline W8 ShiftRight(W8 a, int n) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = (uint16_t)(a.v[i] >> n); return r; }
	inline W8 ShiftLeft(W8 a, int n) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = (uint16_t)(a.v[i] << n); return r; }
	inline W8 operator&(W8 a, W8 b) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = a.v[i] & b.v[i]; return r; }
	inline W8 operator|(W8 a, W8 b) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = a.v[i] | b.v[i]; return r; }
	inline W8 Min(W8 a, W8 b) { W8 r; for (int i = 0; i < 8; i++) r.v[i] = a.v[i] < b.v[i] ? a.v[i] : b.v[i]; return r; }
#endif

	// Two texels side by side (BGRA8, eight bytes) of each of two lanes
	// widened to 16 bits: the first texels, lane 0's then lane 1's, and the
	// second ones
	inline void Widen(const uint32_t* pair0, const uint32_t* pair1, W8& first, W8& second)
	{
#if defined(SOFTWARE_SSE2)
		__m128i a = _mm_loadl_epi64((const __m128i*)pair0);
		__m128i b = _mm_loadl_epi64((const __m128i*)pair1);
		__m128i firsts = _mm_unpacklo_epi32(a, b); // a0 b0 a1 b1
		__m128i zero = _mm_setzero_si128();
		first.v = _mm_unpacklo_epi8(firsts, zero);
		second.v = _mm_unpackhi_epi8(firsts, zero);
#elif defined(SOFTWARE_NEON)
		uint32x2_t a = vld1_u32(pair0);
		uint32x2_t b = vld1_u32(pair1);
		uint32x2x2_t z = vzip_u32(a, b); // (a0 b0) (a1 b1)
		first.v = vmovl_u8(vreinterpret_u8_u32(z.val[0]));
		second.v = vmovl_u8(vreinterpret_u8_u32(z.val[1]));
#else
		for (int i = 0; i < 4; i++)
		{
			first.v[i] = (uint16_t)((pair0[0] >> (i * 8)) & 0xff);
			first.v[i + 4] = (uint16_t)((pair1[0] >> (i * 8)) & 0xff);
			second.v[i] = (uint16_t)((pair0[1] >> (i * 8)) & 0xff);
			second.v[i + 4] = (uint16_t)((pair1[1] >> (i * 8)) & 0xff);
		}
#endif
	}

	// One texel of each of two lanes widened to 16 bits
	inline W8 Widen(uint32_t texel0, uint32_t texel1)
	{
#if defined(SOFTWARE_SSE2)
		__m128i t = _mm_unpacklo_epi32(_mm_cvtsi32_si128((int)texel0), _mm_cvtsi32_si128((int)texel1));
		return { _mm_unpacklo_epi8(t, _mm_setzero_si128()) };
#elif defined(SOFTWARE_NEON)
		uint32x2_t t = vset_lane_u32(texel1, vdup_n_u32(texel0), 1);
		return { vmovl_u8(vreinterpret_u8_u32(t)) };
#else
		W8 r;
		for (int i = 0; i < 4; i++)
		{
			r.v[i] = (uint16_t)((texel0 >> (i * 8)) & 0xff);
			r.v[i + 4] = (uint16_t)((texel1 >> (i * 8)) & 0xff);
		}
		return r;
#endif
	}

	inline W8 operator~(W8 a) { return a ^ SetW(0xffff); }
	inline W8 Select(W8 mask, W8 a, W8 b) { return (mask & a) | (~mask & b); }

	// Each lane's alpha, the fourth of its channels, in all four
	inline W8 BroadcastAlpha(W8 a)
	{
#if defined(SOFTWARE_SSE2)
		return { _mm_shufflehi_epi16(_mm_shufflelo_epi16(a.v, 0xff), 0xff) };
#elif defined(SOFTWARE_NEON)
		return { vcombine_u16(vdup_lane_u16(vget_low_u16(a.v), 3), vdup_lane_u16(vget_high_u16(a.v), 3)) };
#else
		W8 r;
		for (int i = 0; i < 4; i++) { r.v[i] = a.v[3]; r.v[i + 4] = a.v[7]; }
		return r;
#endif
	}

	// Two lanes' channels from four lanes' channel by channel, values up to
	// 65535: lanes 0 and 1 (half 0) or 2 and 3 (half 1), each lane's blue,
	// green, red and alpha in turn
	inline W8 Interleave(I4 b, I4 g, I4 r, I4 a, int half)
	{
#if defined(SOFTWARE_SSE2)
		// packssdw saturates as signed: moved by 32768 and back
		const __m128i bias32 = _mm_set1_epi32(32768);
		const __m128i bias16 = _mm_set1_epi16((short)0x8000);
		__m128i bg = _mm_xor_si128(_mm_packs_epi32(_mm_sub_epi32(b.v, bias32), _mm_sub_epi32(g.v, bias32)), bias16);
		__m128i ra = _mm_xor_si128(_mm_packs_epi32(_mm_sub_epi32(r.v, bias32), _mm_sub_epi32(a.v, bias32)), bias16);
		__m128i x = _mm_unpacklo_epi16(bg, ra); // b0 r0 b1 r1 b2 r2 b3 r3
		__m128i y = _mm_unpackhi_epi16(bg, ra); // g0 a0 g1 a1 g2 a2 g3 a3
		return { half ? _mm_unpackhi_epi16(x, y) : _mm_unpacklo_epi16(x, y) };
#elif defined(SOFTWARE_NEON)
		uint16x4_t b16 = vqmovun_s32(b.v), g16 = vqmovun_s32(g.v), r16 = vqmovun_s32(r.v), a16 = vqmovun_s32(a.v);
		uint16x4x2_t br = vzip_u16(b16, r16); // b0 r0 b1 r1, b2 r2 b3 r3
		uint16x4x2_t ga = vzip_u16(g16, a16);
		uint16x4x2_t lo = vzip_u16(br.val[half], ga.val[half]); // b g r a of each lane
		return { vcombine_u16(lo.val[0], lo.val[1]) };
#else
		auto c = [](int32_t v) { return (uint16_t)(v < 0 ? 0 : (v > 65535 ? 65535 : v)); };
		W8 out;
		for (int i = 0; i < 2; i++)
		{
			int lane = half * 2 + i;
			out.v[i * 4 + 0] = c(b.v[lane]);
			out.v[i * 4 + 1] = c(g.v[lane]);
			out.v[i * 4 + 2] = c(r.v[lane]);
			out.v[i * 4 + 3] = c(a.v[lane]);
		}
		return out;
#endif
	}

	// The reverse: two vectors of two lanes' channels, channel by channel
	inline void Deinterleave(W8 lanes01, W8 lanes23, I4& b, I4& g, I4& r, I4& a)
	{
#if defined(SOFTWARE_SSE2)
		__m128i zero = _mm_setzero_si128();
		__m128i l0 = _mm_unpacklo_epi16(lanes01.v, zero); // b0 g0 r0 a0
		__m128i l1 = _mm_unpackhi_epi16(lanes01.v, zero);
		__m128i l2 = _mm_unpacklo_epi16(lanes23.v, zero);
		__m128i l3 = _mm_unpackhi_epi16(lanes23.v, zero);
		__m128i t0 = _mm_unpacklo_epi32(l0, l1); // b0 b1 g0 g1
		__m128i t1 = _mm_unpackhi_epi32(l0, l1); // r0 r1 a0 a1
		__m128i t2 = _mm_unpacklo_epi32(l2, l3);
		__m128i t3 = _mm_unpackhi_epi32(l2, l3);
		b.v = _mm_unpacklo_epi64(t0, t2);
		g.v = _mm_unpackhi_epi64(t0, t2);
		r.v = _mm_unpacklo_epi64(t1, t3);
		a.v = _mm_unpackhi_epi64(t1, t3);
#elif defined(SOFTWARE_NEON)
		uint16x4x2_t z0 = vuzp_u16(vget_low_u16(lanes01.v), vget_high_u16(lanes01.v)); // b0 r0 b1 r1, g0 a0 g1 a1
		uint16x4x2_t z1 = vuzp_u16(vget_low_u16(lanes23.v), vget_high_u16(lanes23.v));
		uint16x4x2_t br = vuzp_u16(z0.val[0], z1.val[0]); // b0 b1 b2 b3, r0 r1 r2 r3
		uint16x4x2_t ga = vuzp_u16(z0.val[1], z1.val[1]);
		b.v = vreinterpretq_s32_u32(vmovl_u16(br.val[0]));
		r.v = vreinterpretq_s32_u32(vmovl_u16(br.val[1]));
		g.v = vreinterpretq_s32_u32(vmovl_u16(ga.val[0]));
		a.v = vreinterpretq_s32_u32(vmovl_u16(ga.val[1]));
#else
		for (int lane = 0; lane < 4; lane++)
		{
			const W8& src = lane < 2 ? lanes01 : lanes23;
			int i = (lane & 1) * 4;
			b.v[lane] = src.v[i + 0];
			g.v[lane] = src.v[i + 1];
			r.v[lane] = src.v[i + 2];
			a.v[lane] = src.v[i + 3];
		}
#endif
	}

	// The 2x2 floats of a quad: two from each row
	inline F4 LoadF2x2(const float* row0, const float* row1)
	{
#if defined(SOFTWARE_SSE2)
		return { _mm_loadh_pi(_mm_loadl_pi(_mm_setzero_ps(), (const __m64*)row0), (const __m64*)row1) };
#elif defined(SOFTWARE_NEON)
		return { vcombine_f32(vld1_f32(row0), vld1_f32(row1)) };
#else
		return { { row0[0], row0[1], row1[0], row1[1] } };
#endif
	}

	inline void StoreW(uint16_t* p, W8 a)
	{
#if defined(SOFTWARE_SSE2)
		_mm_storeu_si128((__m128i*)p, a.v);
#elif defined(SOFTWARE_NEON)
		vst1q_u16(p, a.v);
#else
		std::memcpy(p, a.v, sizeof(a.v));
#endif
	}

	inline W8 LoadW(const uint16_t* p)
	{
#if defined(SOFTWARE_SSE2)
		return { _mm_loadu_si128((const __m128i*)p) };
#elif defined(SOFTWARE_NEON)
		return { vld1q_u16(p) };
#else
		W8 r;
		std::memcpy(r.v, p, sizeof(r.v));
		return r;
#endif
	}
}
