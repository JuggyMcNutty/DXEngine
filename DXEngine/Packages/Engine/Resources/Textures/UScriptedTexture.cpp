
#include "Precomp.h"
#include <cstring>
#include <cmath>
#include "Utils/Logger.h"
#include "UScriptedTexture.h"
#include "Engine.h"
#include "VM/ScriptCall.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Packages/Engine/Resources/UFont.h"

void UScriptedTexture::Load(ObjectStream* stream)
{
	UTexture::Load(stream);

	UsedFormat = TextureFormat::P8;
	UsedMipmaps.resize(1);

	int width = GetInt("UClamp");
	int height = GetInt("VClamp");

	UnrealMipmap& mipmap = UsedMipmaps.front();
	mipmap.Width = width;
	mipmap.Height = height;
	mipmap.Data.resize((size_t)mipmap.Width * mipmap.Height);
	uint8_t* pixels = (uint8_t*)mipmap.Data.data();
	memset(pixels, 0, (size_t)width * height);

	// The original's load takes its source texture's pixels once, its own top
	// mip's, scaled by the two textures' bits -- each pixel the source's at
	// its place shifted down by the difference, as an x86 shift counts a
	// negative one -- and its palette (Engine.dll 0x103ea3d0); its tick copies
	// that copy over the mip, whatever the source has become
	UTexture* source = SourceTexture();
	if (source)
	{
		source->LoadNow();
		if (!source->UsedMipmaps.empty() && source->UsedFormat == TextureFormat::P8)
		{
			Palette() = source->Palette();
			const UnrealMipmap& from = source->UsedMipmaps.front();
			const uint8_t* src = (const uint8_t*)from.Data.data();
			int ushift = (UBits() - source->UBits()) & 31, vshift = (VBits() - source->VBits()) & 31;
			int usize = std::max(USize(), 0), vsize = std::max(VSize(), 0);
			SourceCopy.assign((size_t)usize * vsize, 0);
			for (int y = 0; y < vsize; y++)
			{
				for (int x = 0; x < usize; x++)
				{
					size_t at = ((size_t)(y >> vshift) << source->UBits()) + (size_t)(x >> ushift);
					SourceCopy[(size_t)y * usize + x] = at < from.Data.size() ? src[at] : 0;
				}
			}
		}
	}
}

void UScriptedTexture::Save(PackageStreamWriter* stream)
{
	UTexture::Save(stream);
}

// A source texel's coordinate wrapped to the mip, as texture sampling wraps;
// a coordinate that is no number or far out reads texel 0
static int WrapTexel(float v, int size)
{
	if (!(std::fabs(v) < 16777216.0f) || size <= 0)
		return 0;
	int i = (int)std::floor(v) % size;
	return i < 0 ? i + size : i;
}

// The original's tick (Engine.dll 0x103ea5b0): the source's copy its load
// took over the mip, then the notify actor's RenderTexture
void UScriptedTexture::UpdateFrame()
{
	if (!SourceCopy.empty() && !UsedMipmaps.empty())
	{
		UnrealMipmap& mipmap = UsedMipmaps.front();
		memcpy(mipmap.Data.data(), SourceCopy.data(), std::min(mipmap.Data.size(), SourceCopy.size()));
	}

	UActor* actor = NotifyActor();
	if (actor)
		CallEvent(actor, "RenderTexture", { ExpressionValue::ObjectValue(this) });

	TextureModified = true;
}

// The palette's nearest colour, 0 with no palette (a mod's scripted texture
// made without one)
uint8_t UScriptedTexture::BestColor(const Color& color)
{
	return Palette() ? Palette()->FindBestColor(color) : 0;
}

void UScriptedTexture::DrawColoredText(float X, float Y, const std::string& Text, UFont* Font, const Color& FontColor)
{
	if (!Font)
		return;

	uint8_t color = BestColor(FontColor);

	for (char c : Text)
	{
		FontGlyph glyph = Font->GetGlyph(c);
		if (!glyph.Texture)
			continue;

		float StartU = (float)glyph.StartU;
		float StartV = (float)glyph.StartV;
		float USize = (float)glyph.USize;
		float VSize = (float)glyph.VSize;
		DrawTileP8Color(glyph.Texture, X, Y, USize, VSize, StartU, StartV, USize, VSize, color);
		X += USize;
	}
}

void UScriptedTexture::DrawText(float X, float Y, const std::string& Text, UFont* Font)
{
	if (!Font)
		return;

	for (char c : Text)
	{
		FontGlyph glyph = Font->GetGlyph(c);
		if (!glyph.Texture)
			continue;

		float StartU = (float)glyph.StartU;
		float StartV = (float)glyph.StartV;
		float USize = (float)glyph.USize;
		float VSize = (float)glyph.VSize;
		DrawTileP8Masked(glyph.Texture, X, Y, USize, VSize, StartU, StartV, USize, VSize);
		X += USize;
	}
}

void UScriptedTexture::DrawTile(float X, float Y, float XL, float YL, float U, float V, float UL, float VL, UTexture* Tex, bool bMasked)
{
	if (bMasked)
		DrawTileP8Masked(Tex, X, Y, XL, YL, U, V, UL, VL);
	else
		DrawTileP8(Tex, X, Y, XL, YL, U, V, UL, VL);
}

// The original's execReplaceTexture (Engine.dll 0x103eb1e0): when the
// texture's size is this one's, the texture stepped to now, as its Update
// steps it to the time this one last ticked at, and its current frame's top
// mip copied over this one's; else a line in the log, its texts. A frame of
// another size, which the original's copy would read past, is copied as far
// as both reach.
void UScriptedTexture::ReplaceTexture(UTexture* Tex)
{
	if (!Tex)
	{
		LogMessage("ReplaceTexture: Missing Texture");
		return;
	}
	if (Tex->USize() != USize() || Tex->VSize() != VSize())
	{
		LogMessage("ReplaceTexture: Dimensions are different");
		return;
	}
	double now = UTexture::ClockNow();
	Tex->Update((float)(now - Tex->LastUpdateTime));
	Tex->LastUpdateTime = now;
	UTexture* frame = Tex->GetAnimTexture();
	if (UsedMipmaps.empty() || frame->UsedMipmaps.empty())
		return;
	UnrealMipmap& dst = UsedMipmaps.front();
	const UnrealMipmap& src = frame->UsedMipmaps.front();
	memcpy(dst.Data.data(), src.Data.data(), std::min({ dst.Data.size(), src.Data.size(), (size_t)std::max(USize(), 0) * std::max(VSize(), 0) }));
	TextureModified = true;
}

void UScriptedTexture::TextSize(const std::string& Text, float& XL, float& YL, UFont* Font)
{
	if (!Font)
		return;

	int x = 0;
	int y = 0;
	for (char c : Text)
	{
		FontGlyph glyph = Font->GetGlyph(c);
		x += glyph.USize;
		y = std::max(y, glyph.VSize);
	}

	XL = (float)x;
	YL = (float)y;
}

void UScriptedTexture::DrawTileP8(UTexture* texture, float left, float top, float width, float height, float u, float v, float uvwidth, float uvheight)
{
	// A script's floats as they come: a size of 0 or less, a value that is
	// no number or past an int, or a source with no mip draws nothing
	auto sane = [](float f) { return std::fabs(f) < 1e8f; };
	if (!texture || texture->UsedMipmaps.empty() || UsedMipmaps.empty() || !(width > 0.0f) || !(height > 0.0f) ||
		!sane(left) || !sane(top) || !sane(width) || !sane(height) || !sane(u) || !sane(v) || !sane(uvwidth) || !sane(uvheight))
		return;
	if (!texture || width <= 0.0f || height <= 0.0f || UsedFormat != TextureFormat::P8 || texture->UsedFormat != TextureFormat::P8)
		return;

	int swidth = texture->UsedMipmaps.front().Width;
	int sheight = texture->UsedMipmaps.front().Height;
	const uint8_t* src = (const uint8_t*)texture->UsedMipmaps.front().Data.data();

	int dwidth = UsedMipmaps.front().Width;
	int dheight = UsedMipmaps.front().Height;
	uint8_t* dest = (uint8_t*)UsedMipmaps.front().Data.data();

	int x0 = std::max((int)left, 0);
	int x1 = std::min((int)(left + width), dwidth);
	int y0 = std::max((int)top, 0);
	int y1 = std::min((int)(top + height), dheight);
	if (x1 <= x0 || y1 <= y0)
		return;

	float uscale = uvwidth / width;
	float vscale = uvheight / height;

	for (int y = y0; y < y1; y++)
	{
		float vpix = v + vscale * (y + 0.5f - top);
		const uint8_t* sline = src + WrapTexel(vpix, sheight) * swidth;
		uint8_t* dline = dest + y * dwidth;

		for (int x = x0; x < x1; x++)
		{
			float upix = u + uscale * (x + 0.5f - left);
			uint8_t spixel = sline[WrapTexel(upix, swidth)];
			dline[x] = spixel;
		}
	}
}

void UScriptedTexture::DrawTileP8Masked(UTexture* texture, float left, float top, float width, float height, float u, float v, float uvwidth, float uvheight)
{
	// A script's floats as they come: a size of 0 or less, a value that is
	// no number or past an int, or a source with no mip draws nothing
	auto sane = [](float f) { return std::fabs(f) < 1e8f; };
	if (!texture || texture->UsedMipmaps.empty() || UsedMipmaps.empty() || !(width > 0.0f) || !(height > 0.0f) ||
		!sane(left) || !sane(top) || !sane(width) || !sane(height) || !sane(u) || !sane(v) || !sane(uvwidth) || !sane(uvheight))
		return;
	if (!texture || width <= 0.0f || height <= 0.0f || UsedFormat != TextureFormat::P8 || texture->UsedFormat != TextureFormat::P8)
		return;

	int swidth = texture->UsedMipmaps.front().Width;
	int sheight = texture->UsedMipmaps.front().Height;
	const uint8_t* src = (const uint8_t*)texture->UsedMipmaps.front().Data.data();

	int dwidth = UsedMipmaps.front().Width;
	int dheight = UsedMipmaps.front().Height;
	uint8_t* dest = (uint8_t*)UsedMipmaps.front().Data.data();

	int x0 = std::max((int)left, 0);
	int x1 = std::min((int)(left + width), dwidth);
	int y0 = std::max((int)top, 0);
	int y1 = std::min((int)(top + height), dheight);
	if (x1 <= x0 || y1 <= y0)
		return;

	float uscale = uvwidth / width;
	float vscale = uvheight / height;

	for (int y = y0; y < y1; y++)
	{
		float vpix = v + vscale * (y + 0.5f - top);
		const uint8_t* sline = src + WrapTexel(vpix, sheight) * swidth;
		uint8_t* dline = dest + y * dwidth;

		for (int x = x0; x < x1; x++)
		{
			float upix = u + uscale * (x + 0.5f - left);
			uint8_t spixel = sline[WrapTexel(upix, swidth)];
			if (spixel != 0)
				dline[x] = spixel;
		}
	}
}

void UScriptedTexture::DrawTileP8Color(UTexture* texture, float left, float top, float width, float height, float u, float v, float uvwidth, float uvheight, uint8_t color)
{
	// A script's floats as they come: a size of 0 or less, a value that is
	// no number or past an int, or a source with no mip draws nothing
	auto sane = [](float f) { return std::fabs(f) < 1e8f; };
	if (!texture || texture->UsedMipmaps.empty() || UsedMipmaps.empty() || !(width > 0.0f) || !(height > 0.0f) ||
		!sane(left) || !sane(top) || !sane(width) || !sane(height) || !sane(u) || !sane(v) || !sane(uvwidth) || !sane(uvheight))
		return;
	if (!texture || width <= 0.0f || height <= 0.0f || UsedFormat != TextureFormat::P8 || texture->UsedFormat != TextureFormat::P8)
		return;

	int swidth = texture->UsedMipmaps.front().Width;
	int sheight = texture->UsedMipmaps.front().Height;
	const uint8_t* src = (const uint8_t*)texture->UsedMipmaps.front().Data.data();

	int dwidth = UsedMipmaps.front().Width;
	int dheight = UsedMipmaps.front().Height;
	uint8_t* dest = (uint8_t*)UsedMipmaps.front().Data.data();

	int x0 = std::max((int)left, 0);
	int x1 = std::min((int)(left + width), dwidth);
	int y0 = std::max((int)top, 0);
	int y1 = std::min((int)(top + height), dheight);
	if (x1 <= x0 || y1 <= y0)
		return;

	float uscale = uvwidth / width;
	float vscale = uvheight / height;

	for (int y = y0; y < y1; y++)
	{
		float vpix = v + vscale * (y + 0.5f - top);
		const uint8_t* sline = src + WrapTexel(vpix, sheight) * swidth;
		uint8_t* dline = dest + y * dwidth;

		for (int x = x0; x < x1; x++)
		{
			float upix = u + uscale * (x + 0.5f - left);
			uint8_t spixel = sline[WrapTexel(upix, swidth)];
			if (spixel != 0)
				dline[x] = color;
		}
	}
}
