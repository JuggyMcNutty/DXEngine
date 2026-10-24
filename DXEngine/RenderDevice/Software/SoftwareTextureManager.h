#pragma once

#include "SoftwareRasterizer.h"
#include <unordered_map>
#include <memory>
#include <utility>

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

// The device's own copy of a texture: never the engine's memory, which a
// realtime texture changes after the draw that names it.
class SoftwareTexture
{
public:
	static constexpr int MaxMips = 16;

	int NumMips = 0;
	SoftwareMip Mips[MaxMips];
	SoftwareBuffer<uint32_t> Data;
};

// The copies by cache ID, a masked one apart (only a palette's colour 0 is
// cleared for a masked draw), as the Vulkan device keeps its images.
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

private:
	SoftwareTexture* Find(int masked, uint64_t cacheID);

	std::unordered_map<uint64_t, std::unique_ptr<SoftwareTexture>> TextureCache[2];
	// The last texture found of each kind, as the GL device's lookup keeps it
	std::pair<uint64_t, SoftwareTexture*> LastHit[2];
	SoftwareTexture White;
};
