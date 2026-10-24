
#include "Precomp.h"
#include "SoftwareTextureUploader.h"
#include "SoftwareTextureManager.h"
#include "RenderDevice/RenderDevice.h"
#include <algorithm>
#include <cmath>
#include <cstring>

static uint32_t PackBGRA(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
	return (a << 24) | (r << 16) | (g << 8) | b;
}

bool SoftwareTextureUploader::Supports(TextureFormat format)
{
	switch (format)
	{
	case TextureFormat::P8:
	case TextureFormat::BGRA8_LM:
	case TextureFormat::R5G6B5:
	case TextureFormat::BC1:
	case TextureFormat::RGB8:
	case TextureFormat::BGRA8:
	case TextureFormat::RGBA32_F:
		return true;
	default:
		return false;
	}
}

// The bytes a mip of the format holds
static size_t MipBytes(TextureFormat format, int width, int height)
{
	size_t texels = (size_t)width * height;
	switch (format)
	{
	case TextureFormat::P8: return texels;
	case TextureFormat::BGRA8_LM: return texels * 4;
	case TextureFormat::R5G6B5: return texels * 2;
	case TextureFormat::BC1: return (size_t)((width + 3) / 4) * ((height + 3) / 4) * 8;
	case TextureFormat::RGB8: return texels * 3;
	case TextureFormat::BGRA8: return texels * 4;
	case TextureFormat::RGBA32_F: return texels * 16;
	default: return 0;
	}
}

static bool ValidMip(TextureFormat format, const UnrealMipmap& mip)
{
	return mip.Width >= 1 && mip.Height >= 1 && mip.Width <= 16384 && mip.Height <= 16384 && mip.Data.size() >= MipBytes(format, mip.Width, mip.Height);
}

// A DXT1 block's sixteen texels, with its one-bit alpha, as the Vulkan
// device's CPU decoder makes them (TextureUploader_BC1_Decode)
static void DecodeBC1Block(const uint8_t* src, uint32_t* texels)
{
	uint16_t c0 = src[0] | (src[1] << 8);
	uint16_t c1 = src[2] | (src[3] << 8);
	uint32_t idx = src[4] | (src[5] << 8) | (src[6] << 16) | ((uint32_t)src[7] << 24);

	uint32_t r[4], g[4], b[4], a[4];
	r[0] = ((c0 >> 11) << 3) | ((c0 >> 11) >> 2);
	g[0] = (((c0 >> 5) & 63) << 2) | (((c0 >> 5) & 63) >> 4);
	b[0] = ((c0 & 31) << 3) | ((c0 & 31) >> 2);
	r[1] = ((c1 >> 11) << 3) | ((c1 >> 11) >> 2);
	g[1] = (((c1 >> 5) & 63) << 2) | (((c1 >> 5) & 63) >> 4);
	b[1] = ((c1 & 31) << 3) | ((c1 & 31) >> 2);
	a[0] = a[1] = 255;

	if (c0 <= c1)
	{
		// Index 3 a texel wholly transparent
		r[2] = (r[0] + r[1]) / 2;
		g[2] = (g[0] + g[1]) / 2;
		b[2] = (b[0] + b[1]) / 2;
		a[2] = 255;
		r[3] = g[3] = b[3] = a[3] = 0;
	}
	else
	{
		r[2] = (2 * r[0] + r[1]) / 3;
		g[2] = (2 * g[0] + g[1]) / 3;
		b[2] = (2 * b[0] + b[1]) / 3;
		r[3] = (r[0] + 2 * r[1]) / 3;
		g[3] = (g[0] + 2 * g[1]) / 3;
		b[3] = (b[0] + 2 * b[1]) / 3;
		a[2] = a[3] = 255;
	}

	for (int i = 0; i < 16; i++)
	{
		int t = (idx >> (2 * i)) & 3;
		texels[i] = PackBGRA(r[t], g[t], b[t], a[t]);
	}
}

// The texels x, y, w, h of a mip into its copy
static void Convert(TextureFormat format, const UnrealMipmap& src, const TextureColor* palette, bool masked, const SoftwareMip& dest, int x, int y, int w, int h)
{
	const uint8_t* data = src.Data.data();
	int width = src.Width;
	switch (format)
	{
	case TextureFormat::P8:
	{
		// The palette's colours as they are, colour 0 cleared for a masked
		// draw; a texture without a palette as its grey levels
		uint32_t colors[256];
		for (int i = 0; i < 256; i++)
		{
			if (palette)
				colors[i] = PackBGRA(palette[i].R, palette[i].G, palette[i].B, palette[i].A);
			else
				colors[i] = PackBGRA(i, i, i, 255);
		}
		if (masked)
			colors[0] = 0;
		for (int row = y; row < y + h; row++)
		{
			const uint8_t* line = data + (size_t)row * width;
			uint32_t* out = dest.Row(row);
			for (int col = x; col < x + w; col++)
				out[col] = colors[line[col]];
		}
		break;
	}
	case TextureFormat::BGRA8_LM:
	{
		// The original's light and fog map bytes, 0 to 127, doubled as D3DDrv
		// uploads them (the Vulkan device's TextureUploader_BGRA8_LM)
		for (int row = y; row < y + h; row++)
		{
			const uint8_t* line = data + (size_t)row * width * 4;
			uint32_t* out = dest.Row(row);
			for (int col = x; col < x + w; col++)
			{
				const uint8_t* p = line + col * 4;
				out[col] = PackBGRA((p[2] << 1) & 0xff, (p[1] << 1) & 0xff, (p[0] << 1) & 0xff, (p[3] << 1) & 0xff);
			}
		}
		break;
	}
	case TextureFormat::R5G6B5:
	{
		for (int row = y; row < y + h; row++)
		{
			const uint8_t* line = data + (size_t)row * width * 2;
			uint32_t* out = dest.Row(row);
			for (int col = x; col < x + w; col++)
			{
				uint32_t v = line[col * 2] | (line[col * 2 + 1] << 8);
				uint32_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
				out[col] = PackBGRA((r * 510 + 31) / 62, (g * 510 + 63) / 126, (b * 510 + 31) / 62, 255);
			}
		}
		break;
	}
	case TextureFormat::BC1:
	{
		int blocksX = (width + 3) / 4;
		for (int by = y / 4; by < (y + h + 3) / 4; by++)
		{
			for (int bx = x / 4; bx < (x + w + 3) / 4; bx++)
			{
				uint32_t texels[16];
				DecodeBC1Block(data + ((size_t)by * blocksX + bx) * 8, texels);
				for (int ty = 0; ty < 4; ty++)
				{
					int row = by * 4 + ty;
					if (row >= src.Height)
						break;
					uint32_t* out = dest.Row(row);
					for (int tx = 0; tx < 4; tx++)
					{
						int col = bx * 4 + tx;
						if (col < src.Width)
							out[col] = texels[ty * 4 + tx];
					}
				}
			}
		}
		break;
	}
	case TextureFormat::RGB8:
	{
		for (int row = y; row < y + h; row++)
		{
			const uint8_t* line = data + (size_t)row * width * 3;
			uint32_t* out = dest.Row(row);
			for (int col = x; col < x + w; col++)
			{
				const uint8_t* p = line + col * 3;
				out[col] = PackBGRA(p[0], p[1], p[2], 255);
			}
		}
		break;
	}
	case TextureFormat::BGRA8:
	{
		for (int row = y; row < y + h; row++)
			std::memcpy(dest.Row(row) + x, data + ((size_t)row * width + x) * 4, (size_t)w * 4);
		break;
	}
	case TextureFormat::RGBA32_F:
	{
		// DXEngine's light and fog maps: whole bytes over 255, so the bytes
		// again, as the Vulkan device's decoder takes them
		auto level = [](float v) { return (uint32_t)((v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v)) * 255.0f + 0.5f); };
		for (int row = y; row < y + h; row++)
		{
			const float* line = (const float*)(data + (size_t)row * width * 16);
			uint32_t* out = dest.Row(row);
			for (int col = x; col < x + w; col++)
			{
				const float* p = line + col * 4;
				out[col] = PackBGRA(level(p[0]), level(p[1]), level(p[2]), level(p[3]));
			}
		}
		break;
	}
	default:
		break;
	}
}

// The repeated column and row, where texels x, y, w, h touch column 0 or
// row 0
static void Wrap(const SoftwareMip& mip, int x, int y, int w, int h)
{
	if (x == 0)
	{
		for (int row = y; row < y + h; row++)
			mip.Row(row)[mip.Width] = mip.Row(row)[0];
	}
	if (y == 0)
		std::memcpy(mip.Row(mip.Height), mip.Row(0), (size_t)mip.Pitch * 4);
}

void SoftwareTextureUploader::MakeWhite(SoftwareTexture* tex)
{
	tex->Data.Resize(4);
	std::fill(tex->Data.Data(), tex->Data.Data() + 4, 0xffffffff);
	tex->NumMips = 1;
	tex->Mips[0].Width = 1;
	tex->Mips[0].Height = 1;
	tex->Mips[0].Pitch = 2;
	tex->Mips[0].Texels = tex->Data.Data();
}

void SoftwareTextureUploader::Upload(SoftwareTexture* tex, const TextureInfo& info, bool masked)
{
	if (!Supports(info.Format) || info.NumMips < 1 || !info.Mips || !ValidMip(info.Format, info.Mips[0]))
	{
		MakeWhite(tex);
		return;
	}

	// The mips in order while each has its data
	int count = 0;
	size_t total = 0;
	while (count < std::min(info.NumMips, (int)SoftwareTexture::MaxMips) && ValidMip(info.Format, info.Mips[count]))
	{
		total += (size_t)(info.Mips[count].Width + 1) * (info.Mips[count].Height + 1);
		count++;
	}

	tex->Data.Resize(total);
	uint32_t* texels = tex->Data.Data();
	for (int level = 0; level < count; level++)
	{
		const UnrealMipmap& src = info.Mips[level];
		SoftwareMip& mip = tex->Mips[level];
		mip.Width = src.Width;
		mip.Height = src.Height;
		mip.Pitch = src.Width + 1;
		mip.Texels = texels;
		texels += (size_t)mip.Pitch * (mip.Height + 1);

		Convert(info.Format, src, info.Palette, masked, mip, 0, 0, mip.Width, mip.Height);
		Wrap(mip, 0, 0, mip.Width, mip.Height);
	}
	tex->NumMips = count;
}

void SoftwareTextureUploader::UploadRect(SoftwareTexture* tex, const TextureInfo& info, bool masked, int x, int y, int w, int h)
{
	if (!Supports(info.Format) || info.NumMips < 1 || !info.Mips || !ValidMip(info.Format, info.Mips[0]))
		return;
	const UnrealMipmap& src = info.Mips[0];
	if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > src.Width || y + h > src.Height)
		return;

	SoftwareMip& mip = tex->Mips[0];
	if (tex->NumMips < 1 || mip.Width != src.Width || mip.Height != src.Height)
	{
		Upload(tex, info, masked);
		return;
	}

	if (info.Format == TextureFormat::BC1)
	{
		// Whole blocks
		int x1 = std::min((x + w + 3) / 4 * 4, src.Width);
		int y1 = std::min((y + h + 3) / 4 * 4, src.Height);
		x = x / 4 * 4;
		y = y / 4 * 4;
		w = x1 - x;
		h = y1 - y;
	}
	Convert(info.Format, src, info.Palette, masked, mip, x, y, w, h);
	Wrap(mip, x, y, w, h);
}
