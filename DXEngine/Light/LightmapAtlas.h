#pragma once

#include "Packages/Engine/Resources/Textures/UTexture.h"
#include <memory>

struct LightmapRect;

// The light maps in shared pages, many to a texture, so that the surfaces
// drawn with them make one batch where their other textures match. A
// surface's points lie between its map's first and last texel centres, so
// the bilinear filter takes none of its neighbours'; D3DDrv's, though it
// wraps its light maps (D3DDrv.dll InitTextureStageState), takes the edge
// texel there. Each map keeps a border repeating its edges for what the
// devices' anisotropic filter reaches past them.
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

	// The part of the map that changed, and the border beside it where it
	// meets an edge, written into its page: the page's rectangle written
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
