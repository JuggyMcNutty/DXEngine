
#include "Precomp.h"
#include "SoftwareReference.h"
#include "SoftwareSampler.h"
#include <algorithm>
#include <cmath>

// The mip a quad reads of a texture, by its coordinates' steps across the
// quad, times scale (a detail band's repeat)
static int QuadMip(const SoftwareTexture* tex, const float u[4], const float v[4], float scale, float mipFactor)
{
	const SoftwareMip& top = tex->Mips[0];
	float w = (float)top.Width * scale;
	float h = (float)top.Height * scale;
	float dudx = (u[1] - u[0]) * w;
	float dvdx = (v[1] - v[0]) * h;
	float dudy = (u[2] - u[0]) * w;
	float dvdy = (v[2] - v[0]) * h;
	return SoftwareSampler::SelectMip(tex, SoftwareSampler::StepSquared(dudx, dvdx, dudy, dvdy), mipFactor);
}

int SoftwareReference::Shade(const SoftwareDraw& draw, const SoftwareQuad& quad, int mask, float mipFactor, float out[4][4])
{
	const uint32_t flags = draw.ShaderFlags;
	const float actorXBlending = (flags & 32) != 0 ? 1.5f : 1.0f;
	const float oneXBlending = (flags & 64) != 0 ? 1.0f : 2.0f;

	const SoftwareTexture* tex = draw.Texture;
	const SoftwareMip& texMip = tex->Mips[QuadMip(tex, quad.U[0], quad.V[0], 1.0f, mipFactor)];
	const SoftwareMip* macroMip = (flags & 2) ? &draw.MacroTexture->Mips[QuadMip(draw.MacroTexture, quad.U[2], quad.V[2], 1.0f, mipFactor)] : nullptr;
	const SoftwareMip* lightMip = (flags & 1) ? &draw.LightMap->Mips[QuadMip(draw.LightMap, quad.U[1], quad.V[1], 1.0f, mipFactor)] : nullptr;
	const SoftwareMip* detailMips[3] = {};
	const SoftwareMip* fogMip = nullptr;
	if (flags & 4)
	{
		float repeat = 1.0f;
		for (int band = 0; band < 3; band++)
		{
			detailMips[band] = &draw.DetailTexture->Mips[QuadMip(draw.DetailTexture, quad.U[3], quad.V[3], repeat, mipFactor)];
			repeat *= 4.223f;
		}
	}
	else if (flags & 8)
	{
		fogMip = &draw.DetailTexture->Mips[QuadMip(draw.DetailTexture, quad.U[3], quad.V[3], 1.0f, mipFactor)];
	}

	int kept = 0;
	for (int lane = 0; lane < 4; lane++)
	{
		if (!(mask & (1 << lane)))
			continue;

		float t[4];
		if (draw.NoSmooth)
			SoftwareSampler::Nearest(texMip, quad.U[0][lane], quad.V[0][lane], draw.Clamp, t);
		else
			SoftwareSampler::Bilinear(texMip, quad.U[0][lane], quad.V[0][lane], draw.Clamp, t);

		float c[4];
		for (int i = 0; i < 4; i++)
			c[i] = t[i] * (1.0f / 255.0f) * quad.Color[i][lane];
		for (int i = 0; i < 3; i++)
			c[i] *= actorXBlending;

		float lightmapScale = oneXBlending;
		if (macroMip)
		{
			// D3DDrv's passes: the macro modulated, twice its colour, into the
			// 8-bit frame; the light map after, doubled
			float m[4];
			SoftwareSampler::Bilinear(*macroMip, quad.U[2][lane], quad.V[2][lane], false, m);
			for (int i = 0; i < 3; i++)
				c[i] = std::min(c[i] * (m[i] * (1.0f / 255.0f)) * 2.0f, 1.0f);
			lightmapScale *= 2.0f;
		}

		if (lightMip)
		{
			float l[4];
			SoftwareSampler::Bilinear(*lightMip, quad.U[1][lane], quad.V[1][lane], false, l);
			for (int i = 0; i < 3; i++)
				c[i] *= std::clamp(l[i] * (1.0f / 255.0f), 0.0f, 1.0f) * lightmapScale;
		}

		if (flags & 4)
		{
			// D3DDrv's three passes, a band each: nearer than 380 units, each
			// further bound 4.223 times nearer and its grain 4.223 times
			// finer, the detail faded to grey by (bound/z - 1) * 100/255 and
			// modulated in at twice its colour, clamped after each
			float z = quad.W[lane];
			float bound = 380.0f;
			float repeat = 1.0f;
			for (int band = 0; band < 3; band++)
			{
				if (z < bound)
				{
					float a = std::min((bound / z - 1.0f) * (100.0f / 255.0f), 1.0f);
					float d[4];
					SoftwareSampler::Bilinear(*detailMips[band], quad.U[3][lane] * repeat, quad.V[3][lane] * repeat, false, d);
					for (int i = 0; i < 3; i++)
					{
						float detail = d[i] * (1.0f / 255.0f);
						c[i] = std::min(c[i] * 2.0f * (0.5f + (detail - 0.5f) * a), 1.0f);
					}
				}
				bound /= 4.223f;
				repeat *= 4.223f;
			}
		}
		else if (fogMip)
		{
			float f[4];
			SoftwareSampler::Bilinear(*fogMip, quad.U[3][lane], quad.V[3][lane], false, f);
			float keep = 1.0f - f[3] * (1.0f / 255.0f);
			for (int i = 0; i < 3; i++)
				c[i] = f[i] * (1.0f / 255.0f) + c[i] * keep;
		}
		else if (flags & 16)
		{
			float f[4] = { quad.U[1][lane], quad.V[1][lane], quad.U[2][lane], quad.V[2][lane] };
			for (int i = 0; i < 3; i++)
				c[i] = f[i] + c[i] * (1.0f - f[3]);
		}

		if (draw.AlphaTest && c[3] < 0.5f)
			continue;

		for (int i = 0; i < 4; i++)
			out[i][lane] = std::clamp(c[i], 0.0f, 1.0f);
		kept |= 1 << lane;
	}
	return kept;
}

uint32_t SoftwareReference::Blend(const SoftwareDraw& draw, const float src[4], uint32_t dest)
{
	if (!draw.ColorWrite)
		return dest;

	float d[3] = { ((dest >> 16) & 0xff) * (1.0f / 255.0f), ((dest >> 8) & 0xff) * (1.0f / 255.0f), (dest & 0xff) * (1.0f / 255.0f) };
	float r[3];
	for (int i = 0; i < 3; i++)
	{
		switch (draw.Blend)
		{
		default:
		case SoftwareBlend::Solid: r[i] = src[i]; break;
		case SoftwareBlend::Translucent: r[i] = src[i] + d[i] * (1.0f - src[i]); break;
		case SoftwareBlend::Modulated: r[i] = src[i] * d[i] + d[i] * src[i]; break;
		case SoftwareBlend::Highlighted: r[i] = src[i] + d[i] * (1.0f - src[3]); break;
		case SoftwareBlend::SubpixelFont: r[i] = draw.Constant.v[i] * src[i] + d[i] * (1.0f - src[i]); break;
		}
	}
	auto level = [](float v) { return (uint32_t)std::clamp((int)std::floor(v * 255.0f + 0.5f), 0, 255); };
	return 0xff000000 | (level(r[0]) << 16) | (level(r[1]) << 8) | level(r[2]);
}
