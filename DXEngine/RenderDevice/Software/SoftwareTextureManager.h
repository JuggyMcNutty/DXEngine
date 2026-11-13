#pragma once

#include "SoftwareRasterizer.h"
#include "RenderDevice/RenderDevice.h"
#include <functional>
#include <unordered_map>
#include <memory>
#include <utility>
#include <vector>

struct TextureInfo;

// One mip of a texture's copy: BGRA8 texels, each 0xAARRGGBB, rows Width + 1
// texels apart. Column Width repeats column 0 and row Height repeats row 0,
// so a bilinear tap's two texels of a row lie side by side, and its two
// rows one after the other, wrapped or not.
struct SoftwareMip
{
	int Width = 0;
	int Height = 0;
	int Pitch = 0;
	uint32_t* Texels = nullptr;

	uint32_t* Row(int y) const { return Texels + (size_t)y * Pitch; }
};

// Texels of a texture's first mip, [X0, X1) by [Y0, Y1)
struct SoftwareRect
{
	int X0 = 0;
	int Y0 = 0;
	int X1 = 0;
	int Y1 = 0;

	bool Overlaps(const SoftwareRect& other) const { return X0 < other.X1 && other.X0 < X1 && Y0 < other.Y1 && other.Y0 < Y1; }
};

// The device's own copy of a texture: never the engine's memory, which a
// realtime texture changes after the draw that names it.
class SoftwareTexture
{
public:
	static constexpr int MaxMips = 16;

	int NumMips = 0;
	SoftwareMip Mips[MaxMips];
	SoftwareBuffer<uint32_t> Data;

	// What the draws not yet drawn read of it, as of the device's epoch
	// (SoftwareRenderDevice::Drain): all of it, or the rectangles a light
	// map's draws read of the page it shares
	uint64_t NamedEpoch = 0;
	bool NamedAll = false;
	std::vector<SoftwareRect> NamedRects;

	// Colour 0 cleared in it, as the draw that made it was masked
	bool Masked = false;
	// STAT HARDWARE's: its format, bytes and the frame it was last set in
	TextureCacheStats::Entry Stat;
};

// One copy a texture by its cache ID, as D3DDrv keeps them (dx-reverse-info
// d3ddrv-dll.md, textures): colour 0 cleared or not as the draw that made it
// was masked or not.
class SoftwareTextureManager
{
public:
	SoftwareTextureManager();

	// The texture's copy, made or brought up to date as the Vulkan device's
	// GetTexture does it: a realtime change made again, all of it or the
	// part that changed, and the flag cleared. A null info is the white
	// texel the GPU devices bind for none.
	SoftwareTexture* GetTexture(TextureInfo* info, bool masked);
	void UpdateTextureRect(TextureInfo* info, int x, int y, int w, int h);
	void ClearCache();

	SoftwareTexture* GetWhite() { return &White; }

	// STAT HARDWARE's: the cache's counts, and its pools
	TextureCacheStats CacheStats;
	void AddPoolStats(Array<RenderDevice::TexturePoolStats>& pools) const;

	// Told before a copy is changed in place, all of it (w < 0) or a
	// rectangle: the device draws first what reads it
	std::function<void(SoftwareTexture* tex, int x, int y, int w, int h)> BeforeWrite;

private:
	SoftwareTexture* Find(uint64_t cacheID);

	std::unordered_map<uint64_t, std::unique_ptr<SoftwareTexture>> TextureCache;
	// The last texture found of each kind, as the GL device's lookup keeps it
	std::pair<uint64_t, SoftwareTexture*> LastHit;
	SoftwareTexture White;
};
