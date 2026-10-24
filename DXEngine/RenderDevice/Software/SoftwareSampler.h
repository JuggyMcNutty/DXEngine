#pragma once

#include "SoftwareTextureManager.h"
#include <cmath>
#include <algorithm>

// Sampling as D3DDrv samples with the game's ini (UseMipmapping=True,
// UseTrilinear=False): bilinear from the nearest mip, no anisotropy; under
// PF_NoSmooth the nearest texel of the nearest mip. A texture wraps; a tile
// whose coordinates lie within it is held at its edges, as the GPU devices'
// mirrored clamp holds it there. A bilinear weight has 8 bits, as the GPUs'.
namespace SoftwareSampler
{
	// The mip lambda chooses: the nearest, lambda being log2 of the larger of
	// the quad's two texel steps in mip 0's texels, plus the bias
	inline int SelectMip(const SoftwareTexture* tex, float lambda)
	{
		if (!(lambda > 0.5f))
			return 0;
		int level = (int)std::min(std::floor(lambda + 0.5f), 15.0f);
		return std::min(level, tex->NumMips - 1);
	}

	// lambda from a quad's coordinate steps across x and across y, in mip
	// 0's texels: the scale factor of the Vulkan spec, the larger of the
	// two steps' lengths
	inline float Lambda(float dudx, float dvdx, float dudy, float dvdy)
	{
		float rx = dudx * dudx + dvdx * dvdx;
		float ry = dudy * dudy + dvdy * dvdy;
		float rho2 = std::max(rx, ry);
		return rho2 > 0.0f ? 0.5f * std::log2(rho2) : -100.0f;
	}

	// u's fraction of a turn, for a coordinate that wraps: what the texel
	// it falls on is, at any distance from the origin
	inline float Frac(float u)
	{
		if (!std::isfinite(u))
			return 0.0f;
		return u - std::floor(u);
	}

	// The two texels along an axis and the second's weight, 0 to 255, for a
	// bilinear tap at t (a coordinate in texels, 0.5 a texel's centre)
	inline void Taps(float t, int size, bool clamp, int& first, int& weight)
	{
		float f = std::floor(t);
		first = (int)f;
		weight = std::min((int)((t - f) * 256.0f), 255);
		if (clamp)
		{
			if (first < 0)
			{
				first = 0;
				weight = 0;
			}
			else if (first >= size - 1)
			{
				first = size - 1;
				weight = 0;
			}
		}
		else if (first < 0)
		{
			first += size;
		}
		else if (first >= size)
		{
			first -= size;
		}
	}

	// A bilinear sample, each channel 0 to 255 (out: r, g, b, a)
	inline void Bilinear(const SoftwareMip& mip, float u, float v, bool clamp, float out[4])
	{
		if (!clamp)
		{
			u = Frac(u);
			v = Frac(v);
		}
		else
		{
			u = std::isfinite(u) ? std::clamp(u, -1.0f, 2.0f) : 0.0f;
			v = std::isfinite(v) ? std::clamp(v, -1.0f, 2.0f) : 0.0f;
		}
		int x, y, wx, wy;
		Taps(u * mip.Width - 0.5f, mip.Width, clamp, x, wx);
		Taps(v * mip.Height - 0.5f, mip.Height, clamp, y, wy);
		const uint32_t* row0 = mip.Row(y);
		const uint32_t* row1 = mip.Row(y + 1);
		// Held at an edge, the second texel (the repeated column or row) has
		// no weight
		uint32_t c00 = row0[x], c10 = row0[x + 1], c01 = row1[x], c11 = row1[x + 1];
		for (int i = 0; i < 4; i++)
		{
			int shift = i == 3 ? 24 : 16 - i * 8; // r, g, b, a
			float t0 = (float)((c00 >> shift) & 0xff) * (256 - wx) + (float)((c10 >> shift) & 0xff) * wx;
			float t1 = (float)((c01 >> shift) & 0xff) * (256 - wx) + (float)((c11 >> shift) & 0xff) * wx;
			out[i] = (t0 * (256 - wy) + t1 * wy) * (1.0f / 65536.0f);
		}
	}

	// The nearest texel, each channel 0 to 255
	inline void Nearest(const SoftwareMip& mip, float u, float v, bool clamp, float out[4])
	{
		int x, y;
		if (clamp)
		{
			x = std::clamp((int)std::floor((std::isfinite(u) ? std::clamp(u, -1.0f, 2.0f) : 0.0f) * mip.Width), 0, mip.Width - 1);
			y = std::clamp((int)std::floor((std::isfinite(v) ? std::clamp(v, -1.0f, 2.0f) : 0.0f) * mip.Height), 0, mip.Height - 1);
		}
		else
		{
			x = std::min((int)(Frac(u) * mip.Width), mip.Width - 1);
			y = std::min((int)(Frac(v) * mip.Height), mip.Height - 1);
		}
		uint32_t c = mip.Row(y)[x];
		out[0] = (float)((c >> 16) & 0xff);
		out[1] = (float)((c >> 8) & 0xff);
		out[2] = (float)(c & 0xff);
		out[3] = (float)(c >> 24);
	}
}
