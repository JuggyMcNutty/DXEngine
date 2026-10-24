
#include "Precomp.h"
#include "TextureUploader.h"
#include "RenderDevice/RenderDevice.h"

#ifdef USE_NEON
#include <arm_neon.h>
#endif

#ifdef USE_SSE2
#include <immintrin.h>
#endif

TextureUploader* TextureUploader::GetUploader(TextureFormat format, VkPhysicalDevice physicalDevice)
{
	static std::map<TextureFormat, std::unique_ptr<TextureUploader>> Uploaders;
	if (Uploaders.empty())
	{
		// Deus Ex's formats (UnTex.h's TEXF_P8 to TEXF_RGBA8), and DXEngine's
		// own light and fog maps (RGBA32_F).
		Uploaders[TextureFormat::P8].reset(new TextureUploader_P8());
		Uploaders[TextureFormat::BGRA8_LM].reset(new TextureUploader_BGRA8_LM());
		Uploaders[TextureFormat::R5G6B5].reset(new TextureUploader_Simple(VK_FORMAT_R5G6B5_UNORM_PACK16, 2));
		// DXT1 with its alpha bit, which some textures use.
		Uploaders[TextureFormat::BC1].reset(new TextureUploader_4x4Block(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8));
		Uploaders[TextureFormat::RGB8].reset(new TextureUploader_Simple(VK_FORMAT_R8G8B8_UNORM, 3));
		Uploaders[TextureFormat::BGRA8].reset(new TextureUploader_Simple(VK_FORMAT_B8G8R8A8_UNORM, 4));
		Uploaders[TextureFormat::RGBA32_F].reset(new TextureUploader_Simple(VK_FORMAT_R32G32B32A32_SFLOAT, 16));
	}

	auto it = Uploaders.find(format);
	if (it != Uploaders.end())
	{
		if (physicalDevice == VK_NULL_HANDLE || !it->second)
			return it->second.get();

		// The canonical format exists, but the GPU may not be able to sample it.
		// BC1 and RGB8 are optional in Vulkan and missing on several mobile
		// GPUs (the PowerVR Rogue GE8300, for one). Sampling an
		// unsupported format is undefined behavior and produces garbage, so
		// substitute a CPU decoder writing a format the device does support.
		// A format that samples but cannot be linearly filtered is just as broken
		// with the engine's linear samplers, so it falls back the same way (the
		// GE8300 samples RGBA32F but cannot filter it, which turns every lightmap
		// and fog map into speckle).
		VkFormatProperties props = {};
		vkGetPhysicalDeviceFormatProperties(physicalDevice, it->second->GetVkFormat(), &props);
		if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) &&
			(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			return it->second.get();

		static std::map<TextureFormat, std::unique_ptr<TextureUploader>> Decoders;
		if (Decoders.empty())
		{
			Decoders[TextureFormat::BC1].reset(new TextureUploader_BC1_Decode());
			Decoders[TextureFormat::RGB8].reset(new TextureUploader_RGB8_Decode());
			Decoders[TextureFormat::RGBA32_F].reset(new TextureUploader_RGBA32F_Decode());
		}
		auto dec = Decoders.find(format);
		return dec != Decoders.end() ? dec->second.get() : nullptr;
	}
	else
		return nullptr;
}

/////////////////////////////////////////////////////////////////////////////

int TextureUploader_P8::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

void TextureUploader_P8::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int pitch = mip->Width;
	uint8_t* src = mip->Data.data() + x + y * pitch;
	TextureColor* Ptr = (TextureColor*)d;
	if (masked)
	{
		TextureColor translucent(0, 0, 0, 0);
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j++)
			{
				int idx = src[j];
				*Ptr++ = (idx != 0) ? palette[idx] : translucent;
			}
			src += pitch;
		}
	}
	else
	{
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j++)
			{
				int idx = src[j];
				*Ptr++ = palette[idx];
			}
			src += pitch;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

int TextureUploader_BGRA8_LM::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

void TextureUploader_BGRA8_LM::UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
#ifdef USE_SSE2
	int pitch = mip->Width;
	TextureColor* src = ((TextureColor*)mip->Data.data()) + x + y * pitch;
	auto Ptr = (TextureColor*)dst;
	if (w % 4 == 0)
	{
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j += 4)
			{
				__m128i p = _mm_loadu_si128((const __m128i*)(src + j));
				__m128i p_hi = _mm_unpackhi_epi8(p, _mm_setzero_si128());
				__m128i p_lo = _mm_unpacklo_epi8(p, _mm_setzero_si128());
				p_hi = _mm_shufflehi_epi16(p_hi, _MM_SHUFFLE(3, 0, 1, 2));
				p_hi = _mm_shufflelo_epi16(p_hi, _MM_SHUFFLE(3, 0, 1, 2));
				p_hi = _mm_slli_epi16(p_hi, 1);
				p_lo = _mm_shufflehi_epi16(p_lo, _MM_SHUFFLE(3, 0, 1, 2));
				p_lo = _mm_shufflelo_epi16(p_lo, _MM_SHUFFLE(3, 0, 1, 2));
				p_lo = _mm_slli_epi16(p_lo, 1);
				p = _mm_packus_epi16(p_lo, p_hi);
				_mm_storeu_si128((__m128i*)(Ptr + j), p);
			}
			Ptr += w;
			src += pitch;
		}
	}
	else
	{
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j++)
			{
				TextureColor Src = src[j];
				Ptr->R = Src.B << 1;
				Ptr->G = Src.G << 1;
				Ptr->B = Src.R << 1;
				Ptr->A = Src.A << 1;
				Ptr++;
			}
			src += pitch;
		}
	}
#else
	int pitch = mip->Width;
	TextureColor* src = ((TextureColor*)mip->Data.data()) + x + y * pitch;
	auto Ptr = (TextureColor*)dst;
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			TextureColor Src = src[j];
			Ptr->R = Src.B << 1;
			Ptr->G = Src.G << 1;
			Ptr->B = Src.R << 1;
			Ptr->A = Src.A << 1;
			Ptr++;
		}
		src += pitch;
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////

int TextureUploader_Simple::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * BytesPerPixel;
}

void TextureUploader_Simple::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int pitch = mip->Width * BytesPerPixel;
	int size = w * BytesPerPixel;
	uint8_t* src = mip->Data.data() + x * BytesPerPixel + y * pitch;
	uint8_t* dst = (uint8_t*)d;
	for (int i = 0; i < h; i++)
	{
		memcpy(dst, src, size);
		dst += size;
		src += pitch;
	}
}

/////////////////////////////////////////////////////////////////////////////

int TextureUploader_4x4Block::GetUploadSize(int x, int y, int w, int h)
{
	int x0 = x / 4;
	int y0 = y / 4;
	int x1 = (x + w + 3) / 4;
	int y1 = (y + h + 3) / 4;
	return (x1 - x0) * (y1 - y0) * BytesPerBlock;
}

void TextureUploader_4x4Block::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int x0 = x / 4;
	int y0 = y / 4;
	int x1 = (x + w + 3) / 4;
	int y1 = (y + h + 3) / 4;

	int pitch = (mip->Width + 3) / 4 * BytesPerBlock;
	int size = (x1 - x0) * BytesPerBlock;
	uint8_t* src = mip->Data.data() + x0 * BytesPerBlock + y0 * pitch;
	uint8_t* dst = (uint8_t*)d;
	for (int i = y0; i < y1; i++)
	{
		memcpy(dst, src, size);
		dst += size;
		src += pitch;
	}
}


/////////////////////////////////////////////////////////////////////////////

// CPU decoders for GPUs that cannot sample a format natively. Each writes a
// simple unpacked format instead of the block compressed one.

static void DecodeColorBlock(const uint8_t* src, uint8_t* dst, int dstWidth, bool oneBitAlpha)
{
	uint16_t c0 = src[0] | (src[1] << 8);
	uint16_t c1 = src[2] | (src[3] << 8);
	uint32_t idx = src[4] | (src[5] << 8) | (src[6] << 16) | ((uint32_t)src[7] << 24);

	uint8_t r[4], g[4], b[4], a[4];
	r[0] = (uint8_t)(((c0 >> 11) << 3) | ((c0 >> 11) >> 2));
	g[0] = (uint8_t)((((c0 >> 5) & 63) << 2) | (((c0 >> 5) & 63) >> 4));
	b[0] = (uint8_t)(((c0 & 31) << 3) | ((c0 & 31) >> 2));
	r[1] = (uint8_t)(((c1 >> 11) << 3) | ((c1 >> 11) >> 2));
	g[1] = (uint8_t)((((c1 >> 5) & 63) << 2) | (((c1 >> 5) & 63) >> 4));
	b[1] = (uint8_t)(((c1 & 31) << 3) | ((c1 & 31) >> 2));

	if (oneBitAlpha && c0 <= c1)
	{
		// DXTC1 transparency: index 3 is a fully transparent texel.
		r[2] = (uint8_t)((r[0] + r[1]) / 2);
		g[2] = (uint8_t)((g[0] + g[1]) / 2);
		b[2] = (uint8_t)((b[0] + b[1]) / 2);
		a[2] = 255;
		r[3] = g[3] = b[3] = 0;
		a[3] = 0;
	}
	else
	{
		r[2] = (uint8_t)((2 * r[0] + r[1]) / 3);
		g[2] = (uint8_t)((2 * g[0] + g[1]) / 3);
		b[2] = (uint8_t)((2 * b[0] + b[1]) / 3);
		r[3] = (uint8_t)((r[0] + 2 * r[1]) / 3);
		g[3] = (uint8_t)((g[0] + 2 * g[1]) / 3);
		b[3] = (uint8_t)((b[0] + 2 * b[1]) / 3);
		a[2] = a[3] = 255;
	}
	a[0] = a[1] = 255;

	for (int i = 0; i < 16; i++)
	{
		int t = (idx >> (2 * i)) & 3;
		dst[i * 4 + 0] = r[t];
		dst[i * 4 + 1] = g[t];
		dst[i * 4 + 2] = b[t];
		dst[i * 4 + 3] = a[t];
	}
}

// Decode a block compressed mip range into dst (texels, row pitch w * dstbytes).
// x and y must be block aligned. outTexels gets one decoded texel per pixel.
template<int DstBytes, typename BlockToTexels>
static void UploadDecoded(void* d, UnrealMipmap* mip, int x, int y, int w, int h, int blockBytes, int blockX, int blockY, BlockToTexels blockToTexels)
{
	int bx0 = x / blockX;
	int by0 = y / blockY;
	int bx1 = (x + w + blockX - 1) / blockX;
	int by1 = (y + h + blockY - 1) / blockY;
	int blockCols = (mip->Width + blockX - 1) / blockX;
	int pitch = blockCols * blockBytes;
	uint8_t* dst = (uint8_t*)d;
	for (int by = by0; by < by1; by++)
	{
		for (int bx = bx0; bx < bx1; bx++)
		{
			const uint8_t* block = mip->Data.data() + ((size_t)by * blockCols + bx) * blockBytes;
			uint8_t texels[16 * 4]; // up to 4x4 RGBA8
			blockToTexels(block, texels);
			for (int ty = 0; ty < blockY; ty++)
			{
				int gy = by * blockY + ty - y;
				if (gy < 0 || gy >= h) continue;
				for (int tx = 0; tx < blockX; tx++)
				{
					int gx = bx * blockX + tx - x;
					if (gx < 0 || gx >= w) continue;
					memcpy(dst + ((size_t)gy * w + gx) * DstBytes, texels + ((size_t)ty * blockX + tx) * DstBytes, DstBytes);
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

int TextureUploader_BC1_Decode::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

void TextureUploader_BC1_Decode::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	UploadDecoded<4>(d, mip, x, y, w, h, 8, 4, 4, [](const uint8_t* block, uint8_t* texels) {
		DecodeColorBlock(block, texels, 4, true);
	});
}

/////////////////////////////////////////////////////////////////////////////

int TextureUploader_RGB8_Decode::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

void TextureUploader_RGB8_Decode::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int pitch = mip->Width * 3;
	uint8_t* src = mip->Data.data() + (size_t)x * 3 + (size_t)y * pitch;
	uint8_t* dst = (uint8_t*)d;
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			dst[j * 4 + 0] = src[j * 3 + 0];
			dst[j * 4 + 1] = src[j * 3 + 1];
			dst[j * 4 + 2] = src[j * 3 + 2];
			dst[j * 4 + 3] = 255;
		}
		dst += (size_t)w * 4;
		src += pitch;
	}
}

/////////////////////////////////////////////////////////////////////////////

int TextureUploader_RGBA32F_Decode::GetUploadSize(int x, int y, int w, int h)
{
	return w * h * 4;
}

void TextureUploader_RGBA32F_Decode::UploadRect(void* d, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked)
{
	int pitch = mip->Width * 16;
	float* src = (float*)(mip->Data.data() + (size_t)x * 16 + (size_t)y * pitch);
	uint8_t* dst = (uint8_t*)d;
#ifdef USE_NEON
	// Every lightmap rebuilt goes through here, several a frame where a light
	// animates. The same clamp, scale and truncation, four channels at once:
	// checked on the device against the loop below for every float from 0 to
	// 1 and for negatives, overflows, infinities and NaNs -- all the same.
	const float32x4_t zero = vdupq_n_f32(0.0f), one = vdupq_n_f32(1.0f), scale = vdupq_n_f32(255.0f), half = vdupq_n_f32(0.5f);
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			float32x4_t c = vld1q_f32(src + (size_t)j * 4);
			uint32x4_t lt = vcltq_f32(c, zero), gt = vcgtq_f32(c, one);
			c = vbslq_f32(lt, zero, vbslq_f32(gt, one, c));
			uint32x4_t u = vcvtq_u32_f32(vaddq_f32(vmulq_f32(c, scale), half));
			uint16x4_t u16 = vmovn_u32(u);
			uint8x8_t u8 = vmovn_u16(vcombine_u16(u16, u16));
			vst1_lane_u32((uint32_t*)(dst + (size_t)j * 4), vreinterpret_u32_u8(u8), 0);
		}
		dst += (size_t)w * 4;
		src = (float*)((uint8_t*)src + pitch);
	}
#else
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			const float* c = src + (size_t)j * 4;
			for (int ch = 0; ch < 4; ch++)
			{
				float v = c[ch];
				v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
				dst[j * 4 + ch] = (uint8_t)(v * 255.0f + 0.5f);
			}
		}
		dst += (size_t)w * 4;
		src = (float*)((uint8_t*)src + pitch);
	}
#endif
}
