#pragma once

#include "Packages/Engine/Resources/Textures/UTexture.h"
#include <memory>

struct LightmapRect;

// The light maps in shared pages, many to a texture, so that the surfaces
// drawn with them make one batch where their other textures match. Each map
// keeps a border of the texels a wrap samples beside its edges, the opposite
// edges', so that its page samples as the map alone does: D3DDrv wraps its
// light maps (D3DDrv.dll InitTextureStageState), and a surface's points lie
// between its map's first and last texel centres, the border taking what the
// anisotropic filter reaches past them.
class LightmapAtlas
{
public:
	enum { PageSize = 1024, Border = 4 };

	// Where a map's first texel lies in its page; no page for one too large
	struct Place
	{
		int Page = -1;
		int X = 0;
		int Y = 0;
	};

	// A place for a map of width x height texels and its border
	Place Allocate(int width, int height);

	// The part of the map that changed, and its border where that touches
	// an edge, written into its page: the page's rectangle written
	void Write(const Place& place, const UnrealMipmap& map, const LightmapRect& changed, LightmapRect& written);

	UnrealMipmap* PageMip(int page) { return &Pages[page]->Mip; }
	uint64_t PageCacheID(int page) const { return Pages[page]->CacheID; }

	// Every page given up: a level's maps are its own
	void Clear();

private:
	struct Shelf
	{
		int Y = 0;
		int Height = 0;
		int Used = 0;
	};
	struct Page
	{
		UnrealMipmap Mip;
		Array<Shelf> Shelves;
		int Top = 0;
		uint64_t CacheID = 0;
	};

	Array<std::unique_ptr<Page>> Pages;
	uint32_t Generation = 0;
};
