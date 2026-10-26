#pragma once

#include "SoftwareCommands.h"
#include "SoftwareRasterizer.h"
#include "SoftwareSampler.h"
#include "SoftwareSetup.h"
#include "SoftwareSimd.h"
#include <array>
#include <algorithm>

// A draw's state as its pipeline takes it, a bit or a field each. A
// specialized pipeline has its key fixed when it is compiled; the general
// one reads the draw's at run time. One template is both, so the two draw
// the same bits (DXE_SW_GENERAL=1 draws with the general one alone).
namespace SoftwarePipelineKey
{
	enum : uint32_t
	{
		BlendMask = 7,       // SoftwareBlend
		ColorWrite = 1 << 3,
		DepthWrite = 1 << 4,
		DepthTest = 1 << 5,
		AlphaTest = 1 << 6,
		NoSmooth = 1 << 7,
		Clamp = 1 << 8,
		ShaderShift = 9,     // Scene.frag's flags: 1 light map, 2 macro, 4 detail, 8 fog map, 16 vertex fog, 32 1.5x, 64 1x
		ShaderMask = 127 << ShaderShift,
		Rectangle = 1 << 16  // a rectangle's pixels, or a triangle's under its edges
	};

	inline uint32_t Of(const SoftwareDraw& draw, bool rectangle)
	{
		uint32_t key = (uint32_t)draw.Blend;
		if (draw.ColorWrite) key |= ColorWrite;
		if (draw.DepthWrite) key |= DepthWrite;
		if (draw.DepthTest) key |= DepthTest;
		if (draw.AlphaTest) key |= AlphaTest;
		if (draw.NoSmooth) key |= NoSmooth;
		if (draw.Clamp) key |= Clamp;
		key |= (draw.ShaderFlags & 127) << ShaderShift;
		if (rectangle) key |= Rectangle;
		return key;
	}

	constexpr uint32_t Shader(uint32_t flags) { return (flags & 127) << ShaderShift; }
}

// A pipeline draws one primitive's pixels within a tile's rectangle
// (SoftwarePipeline, SoftwareSetup.h)

namespace SoftwarePipelines
{
	// The pipeline for a draw's primitive: a specialized one where one has
	// its key, else the general one
	SoftwarePipeline Select(const SoftwareDraw& draw, bool rectangle, bool generalOnly);

	// The specialized pipelines, each file's, by key
	struct Entry
	{
		uint32_t Key;
		SoftwarePipeline Pipeline;
	};

	// The keys a file specializes, each once, made as it is compiled
	struct KeyList
	{
		static constexpr size_t MaxKeys = 128;
		uint32_t Keys[MaxKeys] = {};
		size_t Count = 0;

		constexpr void Add(uint32_t key)
		{
			for (size_t i = 0; i < Count; i++)
			{
				if (Keys[i] == key)
					return;
			}
			Keys[Count++] = key;
		}
	};
	const Entry* ScenePipelines(int& count);
	const Entry* TilePipelines(int& count);
	SoftwarePipeline General();
}

namespace SoftwarePixel
{
	using namespace SoftwareSimd;

	// 1.0 of the shading's fixed point: 2.14, unsigned, so up to 4 -- more
	// than a lit surface reaches before its clamp, and fine enough that a
	// dark pixel's several multiplies stay within a level
	constexpr int One = 16384;

	// A quad's texels from one mip of a texture, lanes 0 and 1 in the first
	// vector, 2 and 3 in the second, each channel times 256 (0 to 65280):
	// bilinear with 8-bit weights, as SoftwareSampler::Bilinear. alpha, if
	// asked for, is each lane's alpha exactly, times 65536, for the alpha
	// test to take as the reference does.
	// A coordinate that is no number or infinite (a DrawScale of 0, a NaN
	// nothing on the path stops) samples as 0, as the float reference maps it:
	// wrapped, it became INT_MIN through Floor and Truncate, a read gigabytes
	// before the texture
	inline F4 FiniteOrZero(F4 a)
	{
		F4 zero = SetF(0.0f);
		a = Select(CmpLtF(a, SetF(1e30f)), a, zero);
		return Select(CmpLtF(SetF(-1e30f), a), a, zero);
	}

	inline void Bilinear(const SoftwareMip& mip, F4 u, F4 v, bool clamp, W8& out01, W8& out23, int32_t* alpha)
	{
		u = FiniteOrZero(u);
		v = FiniteOrZero(v);
		if (!clamp)
		{
			u = u - Floor(u);
			v = v - Floor(v);
		}
		else
		{
			u = Min(Max(u, SetF(-1.0f)), SetF(2.0f));
			v = Min(Max(v, SetF(-1.0f)), SetF(2.0f));
		}
		F4 tx = u * SetF((float)mip.Width) - SetF(0.5f);
		F4 ty = v * SetF((float)mip.Height) - SetF(0.5f);
		F4 fx = Floor(tx);
		F4 fy = Floor(ty);
		I4 x = Truncate(fx);
		I4 y = Truncate(fy);
		I4 wx = Truncate((tx - fx) * SetF(256.0f));
		I4 wy = Truncate((ty - fy) * SetF(256.0f));
		I4 w255 = SetI(255);
		wx = Select(CmpGt(wx, w255), w255, wx);
		wy = Select(CmpGt(wy, w255), w255, wy);
		I4 zero = SetI(0);
		if (clamp)
		{
			// Held at the edges: the first texel at either end, the second's
			// weight none
			I4 lastX = SetI(mip.Width - 1), lastY = SetI(mip.Height - 1);
			I4 below = CmpGt(zero, x), past = CmpGt(x, lastX - SetI(1));
			x = Select(below, zero, Select(past, lastX, x));
			wx = Select(below | past, zero, wx);
			below = CmpGt(zero, y);
			past = CmpGt(y, lastY - SetI(1));
			y = Select(below, zero, Select(past, lastY, y));
			wy = Select(below | past, zero, wy);
		}
		else
		{
			x = x + (CmpGt(zero, x) & SetI(mip.Width));
			y = y + (CmpGt(zero, y) & SetI(mip.Height));
			x = x - (CmpGt(x, SetI(mip.Width - 1)) & SetI(mip.Width));
			y = y - (CmpGt(y, SetI(mip.Height - 1)) & SetI(mip.Height));
		}

		int32_t xs[4], ys[4], wxs[4], wys[4];
		StoreI(xs, x);
		StoreI(ys, y);
		StoreI(wxs, wx);
		StoreI(wys, wy);

		W8* out[2] = { &out01, &out23 };
		for (int half = 0; half < 2; half++)
		{
			int l0 = half * 2, l1 = l0 + 1;
			const uint32_t* a0 = mip.Row(ys[l0]) + xs[l0];
			const uint32_t* a1 = mip.Row(ys[l1]) + xs[l1];
			W8 left0, right0, left1, right1;
			Widen(a0, a1, left0, right0);
			Widen(a0 + mip.Pitch, a1 + mip.Pitch, left1, right1);
			W8 wr = SetW4((uint16_t)wxs[l0], (uint16_t)wxs[l1]);
			W8 wl = SetW(256) - wr;
			// Each row across: at most 255 x 256, 16 bits
			W8 top = MulLow(left0, wl) + MulLow(right0, wr);
			W8 bottom = MulLow(left1, wl) + MulLow(right1, wr);
			// Then down, weights times 256 (the first's 65536 at none taken
			// as 65535, a part in 65536 less)
			uint16_t wd0 = (uint16_t)(wys[l0] << 8), wd1 = (uint16_t)(wys[l1] << 8);
			W8 wd = SetW4(wd0, wd1);
			W8 wu = SetW4((uint16_t)std::min(65536 - wd0, 65535), (uint16_t)std::min(65536 - wd1, 65535));
			*out[half] = MulHighRound(top, wu) + MulHighRound(bottom, wd);
			if (alpha)
			{
				uint16_t t[8], b[8];
				StoreW(t, top);
				StoreW(b, bottom);
				alpha[l0] = t[3] * (256 - wys[l0]) + b[3] * wys[l0];
				alpha[l1] = t[7] * (256 - wys[l1]) + b[7] * wys[l1];
			}
		}
	}

	// The nearest texel for each lane, its channels times 256; the alpha
	// times 65536, as Bilinear gives it
	inline void Nearest(const SoftwareMip& mip, F4 u, F4 v, bool clamp, W8& out01, W8& out23, int32_t* alpha)
	{
		u = FiniteOrZero(u);
		v = FiniteOrZero(v);
		I4 x, y;
		if (clamp)
		{
			u = Min(Max(u, SetF(-1.0f)), SetF(2.0f));
			v = Min(Max(v, SetF(-1.0f)), SetF(2.0f));
			x = Truncate(Floor(u * SetF((float)mip.Width)));
			y = Truncate(Floor(v * SetF((float)mip.Height)));
			I4 zero = SetI(0), lastX = SetI(mip.Width - 1), lastY = SetI(mip.Height - 1);
			x = Select(CmpGt(zero, x), zero, Select(CmpGt(x, lastX), lastX, x));
			y = Select(CmpGt(zero, y), zero, Select(CmpGt(y, lastY), lastY, y));
		}
		else
		{
			x = Truncate((u - Floor(u)) * SetF((float)mip.Width));
			y = Truncate((v - Floor(v)) * SetF((float)mip.Height));
			I4 lastX = SetI(mip.Width - 1), lastY = SetI(mip.Height - 1);
			x = Select(CmpGt(x, lastX), lastX, x);
			y = Select(CmpGt(y, lastY), lastY, y);
		}
		int32_t xs[4], ys[4];
		StoreI(xs, x);
		StoreI(ys, y);
		uint32_t texels[4];
		for (int i = 0; i < 4; i++)
			texels[i] = mip.Row(ys[i])[xs[i]];
		out01 = ShiftLeft(Widen(texels[0], texels[1]), 8);
		out23 = ShiftLeft(Widen(texels[2], texels[3]), 8);
		if (alpha)
		{
			for (int i = 0; i < 4; i++)
				alpha[i] = (int32_t)(texels[i] >> 24) << 16;
		}
	}

	// A texel's channels times 256 as fractions of 1, 0.16 (65280 to 65535)
	inline W8 Fraction(W8 t)
	{
		return t + ShiftRight(t, 8);
	}

	// The mip a quad reads, as SoftwareReference's QuadMip chooses it
	inline int QuadMip(const SoftwareTexture* tex, F4 u, F4 v, float scale, float mipFactor)
	{
		float us[4], vs[4];
		StoreF(us, u);
		StoreF(vs, v);
		const SoftwareMip& top = tex->Mips[0];
		float w = (float)top.Width * scale;
		float h = (float)top.Height * scale;
		float dudx = (us[1] - us[0]) * w;
		float dvdx = (vs[1] - vs[0]) * h;
		float dudy = (us[2] - us[0]) * w;
		float dvdy = (vs[2] - vs[0]) * h;
		return SoftwareSampler::SelectMip(tex, SoftwareSampler::StepSquared(dudx, dvdx, dudy, dvdy), mipFactor);
	}

	// A texture sampled for the quad's lanes in mask: its 0.16 fractions
	template<bool NoSmooth>
	inline void Sample(const SoftwareTexture* tex, F4 u, F4 v, bool clamp, float scale, float mipFactor, int mask, W8& out01, W8& out23, int32_t* alpha = nullptr)
	{
		const SoftwareMip& mip = tex->Mips[QuadMip(tex, u, v, scale, mipFactor)];
		if (scale != 1.0f)
		{
			u = u * SetF(scale);
			v = v * SetF(scale);
		}

		// The lanes outside the mask read where one inside does: their pixels
		// are not drawn, and their coordinates, carried past the primitive
		// (wildly past the near plane), would reach texels the draw never
		// reads, which the game's thread may be writing meanwhile
		if (mask != 15)
		{
			int first = (mask & 1) ? 0 : (mask & 2) ? 1 : (mask & 4) ? 2 : 3;
			I4 inside = SetI((mask & 1) ? -1 : 0, (mask & 2) ? -1 : 0, (mask & 4) ? -1 : 0, (mask & 8) ? -1 : 0);
			u = Select(inside, u, SetF(Lane(u, first)));
			v = Select(inside, v, SetF(Lane(v, first)));
		}
		if (NoSmooth)
			Nearest(mip, u, v, clamp, out01, out23, alpha);
		else
			Bilinear(mip, u, v, clamp, out01, out23, alpha);
		out01 = Fraction(out01);
		out23 = Fraction(out23);
	}

	// The lanes of a two-lane vector: all four channels of a lane set where
	// its bit is
	inline W8 LaneMask(int mask, int half)
	{
		int bits = (mask >> (half * 2)) & 3;
		return SetW4((bits & 1) ? 0xffff : 0, (bits & 2) ? 0xffff : 0);
	}

	// Each lane's alpha alone
	inline W8 AlphaMask()
	{
		static const uint16_t mask[8] = { 0, 0, 0, 0xffff, 0, 0, 0, 0xffff };
		return LoadW(mask);
	}

	// Four lanes' colours, channel by channel, rounded to a fixed point of
	// scale to 1: two vectors of two lanes each
	inline void ToFixed(F4 r, F4 g, F4 b, F4 a, float scale, W8& out01, W8& out23)
	{
		F4 s = SetF(scale);
		F4 half = SetF(0.5f);
		I4 bi = Truncate(Max(b * s + half, SetF(0.0f)));
		I4 gi = Truncate(Max(g * s + half, SetF(0.0f)));
		I4 ri = Truncate(Max(r * s + half, SetF(0.0f)));
		I4 ai = Truncate(Max(a * s + half, SetF(0.0f)));
		out01 = Interleave(bi, gi, ri, ai, 0);
		out23 = Interleave(bi, gi, ri, ai, 1);
	}

	// x, times 2, saturating
	inline W8 Twice(W8 x) { return AddSat(x, x); }

	// A fraction, 0.16, as the shading's 2.14, rounded
	inline W8 ToShading(W8 x) { return ShiftRight(AddSat(x, SetW(2)), 2); }
}

// The pipeline: a primitive's pixels within a tile's rectangle, a quad at a
// time -- coverage, the depth test, the attributes, the scene shader in
// 2.14 fixed point, the alpha test, the blend in float on the frame's bytes
// over 255, the writes.
template<uint32_t FixedKey, bool General>
void SoftwareDrawPrimitive(SoftwareFrame& frame, const SoftwareRasterJob& job, const SoftwareDraw& draw, const SoftwarePrimitive& prim, int x0, int y0, int x1, int y1)
{
	using namespace SoftwareSimd;
	using namespace SoftwarePixel;
	namespace K = SoftwarePipelineKey;

	const uint32_t key = General ? K::Of(draw, prim.Type == SoftwarePrimitiveType::Rectangle) : FixedKey;
	const SoftwareBlend blend = (SoftwareBlend)(key & K::BlendMask);
	const bool colorWrite = (key & K::ColorWrite) != 0;
	const bool depthWrite = (key & K::DepthWrite) != 0;
	const bool depthTest = (key & K::DepthTest) != 0;
	const bool alphaTest = (key & K::AlphaTest) != 0;
	const bool noSmooth = (key & K::NoSmooth) != 0;
	const bool clamp = (key & K::Clamp) != 0;
	const uint32_t flags = (key & K::ShaderMask) >> K::ShaderShift;
	const bool rectangle = (key & K::Rectangle) != 0;

	int xa = std::max(prim.MinX, x0), xb = std::min(prim.MaxX, x1);
	int ya = std::max(prim.MinY, y0), yb = std::min(prim.MaxY, y1);
	if (xa >= xb || ya >= yb)
		return;
	const int qx0 = xa & ~1, qy0 = ya & ~1;

	// The edges a triangle's rectangle crosses, in 32 bits from the first
	// quad's corner (an edge across 65 pixels of at most 2^23 a pixel)
	int count = 0;
	int32_t edgeA[3] = {}, edgeB[3] = {}, edgeRow[3] = {};
	if (!rectangle)
	{
		int64_t w = xb - 1 - xa, h = yb - 1 - ya;
		for (int k = 0; k < 3; k++)
		{
			int64_t A = prim.EdgeA[k], B = prim.EdgeB[k];
			int64_t e = A * xa + B * ya + prim.EdgeC[k];
			int64_t least = e + std::min(A, (int64_t)0) * w + std::min(B, (int64_t)0) * h;
			int64_t most = e + std::max(A, (int64_t)0) * w + std::max(B, (int64_t)0) * h;
			if (most < 0)
				return;
			if (least < 0)
			{
				edgeA[count] = (int32_t)A;
				edgeB[count] = (int32_t)B;
				edgeRow[count] = (int32_t)(e + A * (qx0 - xa) + B * (qy0 - ya));
				count++;
			}
		}
	}

	const SoftwareTexture* texture = draw.Texture;
	const float mipFactor = job.MipFactor;
	const F4 laneX = SetF(0.0f, 1.0f, 0.0f, 1.0f);
	const F4 laneY = SetF(0.0f, 0.0f, 1.0f, 1.0f);
	const F4 refX = SetF(prim.RefX), refY = SetF(prim.RefY);
	const W8 alphaMask = AlphaMask();
	const W8 one = SetW(One);

	auto plane = [](const SoftwarePlane& p, F4 dx, F4 dy) { return SetF(p.Value) + SetF(p.Dx) * dx + SetF(p.Dy) * dy; };

	for (int qy = qy0; qy < yb; qy += 2)
	{
		I4 edges[3];
		for (int i = 0; i < count; i++)
		{
			edges[i] = SetI(edgeRow[i]) + SetI(0, edgeA[i], edgeB[i], edgeA[i] + edgeB[i]);
			edgeRow[i] += 2 * edgeB[i];
		}
		const int rowMask = (qy >= ya ? 3 : 0) | (qy + 1 < yb ? 12 : 0);
		const F4 dy = (SetF((float)qy) + laneY) + SetF(0.5f) - refY;

		for (int qx = qx0; qx < xb; qx += 2)
		{
			int mask = rowMask & ((qx >= xa ? 5 : 0) | (qx + 1 < xb ? 10 : 0));
			if (count > 0)
			{
				I4 inside = CmpGt(edges[0], SetI(-1));
				for (int i = 1; i < count; i++)
					inside = inside & CmpGt(edges[i], SetI(-1));
				mask &= Mask(inside);
				for (int i = 0; i < count; i++)
					edges[i] = edges[i] + SetI(2 * edgeA[i]);
			}
			if (!mask)
				continue;

			float* depthRow0 = frame.DepthRow(qy) + qx;
			float* depthRow1 = frame.DepthRow(qy + 1) + qx;
			const F4 dx = (SetF((float)qx) + laneX) + SetF(0.5f) - refX;
			F4 depth = Min(Max(plane(prim.Depth, dx, dy), SetF(draw.DepthMin)), SetF(draw.DepthMax));
			if (depthTest)
			{
				mask &= Mask(CmpLeF(depth, LoadF2x2(depthRow0, depthRow1)));
				if (!mask)
					continue;
			}

			auto writeDepth = [&]()
			{
				float zs[4];
				StoreF(zs, depth);
				if (mask & 1) depthRow0[0] = zs[0];
				if (mask & 2) depthRow0[1] = zs[1];
				if (mask & 4) depthRow1[0] = zs[2];
				if (mask & 8) depthRow1[1] = zs[3];
			};

			// Depth alone: no colour written and none tested
			if (!colorWrite && !alphaTest)
			{
				if (depthWrite)
					writeDepth();
				continue;
			}

			// The attributes at each lane's centre
			const F4 w = SetF(1.0f) / plane(prim.InvW, dx, dy);
			auto attribute = [&](int i) { return plane(prim.Attributes[i], dx, dy) * w; };

			// The colour, then the texture, each lane's blue, green, red and
			// alpha as the texels' bytes lie
			W8 c01, c23;
			ToFixed(attribute(8), attribute(9), attribute(10), attribute(11), (float)One, c01, c23);

			W8 t01, t23;
			int32_t alpha[4];
			if (noSmooth)
				Sample<true>(texture, attribute(0), attribute(1), clamp, 1.0f, mipFactor, mask, t01, t23, alphaTest ? alpha : nullptr);
			else
				Sample<false>(texture, attribute(0), attribute(1), clamp, 1.0f, mipFactor, mask, t01, t23, alphaTest ? alpha : nullptr);

			if (alphaTest)
			{
				// As the reference takes it: the texel's alpha in float, over
				// 255, times the colour's, under a half discarded
				F4 a = ToFloat(LoadI(alpha)) * SetF(1.0f / 65536.0f) * SetF(1.0f / 255.0f) * attribute(11);
				mask &= ~Mask(CmpLtF(a, SetF(0.5f)));
				if (!mask)
					continue;
			}

			// texture x colour
			W8 o01 = MulHighRound(c01, t01);
			W8 o23 = MulHighRound(c23, t23);
			W8 a01 = o01 & alphaMask, a23 = o23 & alphaMask;

			if (flags & 32)
			{
				o01 = AddSat(o01, ShiftRight(o01, 1));
				o23 = AddSat(o23, ShiftRight(o23, 1));
			}
			int lightmapShift = (flags & 64) ? 0 : 1;
			if (flags & 2)
			{
				W8 m01, m23;
				Sample<false>(draw.MacroTexture, attribute(4), attribute(5), false, 1.0f, mipFactor, mask, m01, m23);
				o01 = Min(Twice(MulHighRound(o01, m01)), one);
				o23 = Min(Twice(MulHighRound(o23, m23)), one);
				lightmapShift++;
			}
			if (flags & 1)
			{
				W8 l01, l23;
				Sample<false>(draw.LightMap, attribute(2), attribute(3), false, 1.0f, mipFactor, mask, l01, l23);
				o01 = MulHighRound(o01, l01);
				o23 = MulHighRound(o23, l23);
				for (int i = 0; i < lightmapShift; i++)
				{
					o01 = Twice(o01);
					o23 = Twice(o23);
				}
			}
			if (flags & 4)
			{
				// The detail texture's bands, each where a lane is nearer than
				// its bound, its grey's share a fraction per lane
				F4 u3 = attribute(6), v3 = attribute(7);
				float bound = 380.0f;
				float repeat = 1.0f;
				for (int band = 0; band < 3; band++)
				{
					int nearer = Mask(CmpLtF(w, SetF(bound)));
					if (nearer)
					{
						F4 a = Min((SetF(bound) / w - SetF(1.0f)) * SetF(100.0f / 255.0f), SetF(1.0f));
						I4 ai = Truncate(Max(a * SetF(65535.0f) + SetF(0.5f), SetF(0.0f)));
						int32_t as[4];
						StoreI(as, ai);
						W8 d01, d23;
						Sample<false>(draw.DetailTexture, u3, v3, false, repeat, mipFactor, mask, d01, d23);
						W8 f01 = SetW4((uint16_t)as[0], (uint16_t)as[1]);
						W8 f23 = SetW4((uint16_t)as[2], (uint16_t)as[3]);
						// mix(0.5, detail, a), then twice the colour times it
						W8 h01 = MulHighRound(SetW(32768), ~f01) + MulHighRound(d01, f01);
						W8 h23 = MulHighRound(SetW(32768), ~f23) + MulHighRound(d23, f23);
						o01 = Select(LaneMask(nearer, 0), Min(Twice(MulHighRound(o01, h01)), one), o01);
						o23 = Select(LaneMask(nearer, 1), Min(Twice(MulHighRound(o23, h23)), one), o23);
					}
					bound /= 4.223f;
					repeat *= 4.223f;
				}
			}
			else if (flags & 8)
			{
				W8 f01, f23;
				Sample<false>(draw.DetailTexture, attribute(6), attribute(7), false, 1.0f, mipFactor, mask, f01, f23);
				o01 = AddSat(ToShading(f01), MulHighRound(o01, ~BroadcastAlpha(f01)));
				o23 = AddSat(ToShading(f23), MulHighRound(o23, ~BroadcastAlpha(f23)));
			}
			else if (flags & 16)
			{
				// The fog's colour and alpha, the second and third coordinates
				W8 f01, f23;
				ToFixed(attribute(2), attribute(3), attribute(4), attribute(5), 65535.0f, f01, f23);
				o01 = AddSat(ToShading(f01), MulHighRound(o01, ~BroadcastAlpha(f01)));
				o23 = AddSat(ToShading(f23), MulHighRound(o23, ~BroadcastAlpha(f23)));
			}

			// Alpha as the texture and the colour made it
			o01 = (o01 & ~alphaMask) | a01;
			o23 = (o23 & ~alphaMask) | a23;

			I4 sb, sg, sr, sa;
			Deinterleave(Min(o01, one), Min(o23, one), sb, sg, sr, sa);

			if (colorWrite)
			{
				// The blend, in float as SoftwareReference::Blend has it
				const F4 toUnit = SetF(1.0f / One);
				F4 src[4] = { ToFloat(sr) * toUnit, ToFloat(sg) * toUnit, ToFloat(sb) * toUnit, ToFloat(sa) * toUnit };
				uint32_t* row0 = frame.ColorRow(qy) + qx;
				uint32_t* row1 = frame.ColorRow(qy + 1) + qx;
				I4 dest = SetI((int32_t)row0[0], (int32_t)row0[1], (int32_t)row1[0], (int32_t)row1[1]);
				const F4 byteToUnit = SetF(1.0f / 255.0f);
				const I4 byteMask = SetI(0xff);
				F4 d[3] = { ToFloat(ShiftRight(dest, 16) & byteMask) * byteToUnit, ToFloat(ShiftRight(dest, 8) & byteMask) * byteToUnit, ToFloat(dest & byteMask) * byteToUnit };
				F4 one4 = SetF(1.0f);
				I4 result = SetI((int32_t)0xff000000);
				for (int i = 0; i < 3; i++)
				{
					F4 r;
					switch (blend)
					{
					default:
					case SoftwareBlend::Solid: r = src[i]; break;
					case SoftwareBlend::Translucent: r = src[i] + d[i] * (one4 - src[i]); break;
					case SoftwareBlend::Modulated: r = src[i] * d[i] + d[i] * src[i]; break;
					case SoftwareBlend::Highlighted: r = src[i] + d[i] * (one4 - src[3]); break;
					case SoftwareBlend::SubpixelFont: r = SetF(draw.Constant.v[i]) * src[i] + d[i] * (one4 - src[i]); break;
					}
					I4 level = Truncate(Min(Max(r * SetF(255.0f) + SetF(0.5f), SetF(0.0f)), SetF(255.0f)));
					result = result | ShiftLeft(level, 16 - i * 8);
				}
				int32_t pixels[4];
				StoreI(pixels, result);
				if (mask & 1) row0[0] = (uint32_t)pixels[0];
				if (mask & 2) row0[1] = (uint32_t)pixels[1];
				if (mask & 4) row1[0] = (uint32_t)pixels[2];
				if (mask & 8) row1[1] = (uint32_t)pixels[3];
			}

			if (depthWrite)
				writeDepth();
		}
	}
}
