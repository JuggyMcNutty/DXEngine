
#include "Precomp.h"
#include "LightmapAtlas.h"
#include "LightEffect.h"
#include "Math/vec.h"

LightmapAtlas::Place LightmapAtlas::Allocate(int width, int height)
{
	Place place;
	int w = width + 2 * Border;
	int h = height + 2 * Border;
	if (width <= 0 || height <= 0 || w > PageSize || h > PageSize)
		return place;

	// The first shelf of a page tall enough, but not twice the height, with
	// room along it; else a new shelf on top, its height the box's rounded up
	// to 8; else a new page
	int shelfHeight = (h + 7) & ~7;
	for (size_t i = 0; i < Pages.size(); i++)
	{
		Page& page = *Pages[i];
		for (Shelf& shelf : page.Shelves)
		{
			if (shelf.Height >= h && shelf.Height < h * 2 && PageSize - shelf.Used >= w)
			{
				place = { (int)i, shelf.Used + Border, shelf.Y + Border };
				shelf.Used += w;
				return place;
			}
		}
		if (PageSize - page.Top >= shelfHeight)
		{
			Shelf shelf;
			shelf.Y = page.Top;
			shelf.Height = shelfHeight;
			shelf.Used = w;
			page.Shelves.push_back(shelf);
			page.Top += shelfHeight;
			return { (int)i, Border, shelf.Y + Border };
		}
	}

	auto page = std::make_unique<Page>();
	page->Mip.Width = PageSize;
	page->Mip.Height = PageSize;
	page->Mip.UBits = 10;
	page->Mip.VBits = 10;
	page->Mip.Data.resize((size_t)PageSize * PageSize * sizeof(vec4));
	// A light map's cache ID ends in 1, a fog map's in 2, a texture's
	// address in 0 or 8
	page->CacheID = ((uint64_t)Generation << 32) | ((uint64_t)Pages.size() << 8) | 3;
	Shelf shelf;
	shelf.Height = shelfHeight;
	shelf.Used = w;
	page->Shelves.push_back(shelf);
	page->Top = shelfHeight;
	Pages.push_back(std::move(page));
	return { (int)Pages.size() - 1, Border, Border };
}

void LightmapAtlas::Write(const Place& place, const UnrealMipmap& map, const LightmapRect& changed, LightmapRect& written)
{
	written = {};
	if (place.Page < 0 || changed.Empty())
		return;

	int width = map.Width;
	int height = map.Height;
	const vec4* src = (const vec4*)map.Data.data();
	vec4* dest = (vec4*)Pages[place.Page]->Mip.Data.data();

	for (int y = changed.y0; y < changed.y1; y++)
	{
		vec4* line = dest + (size_t)(place.Y + y) * PageSize + place.X;
		const vec4* srcline = src + (size_t)y * width;
		for (int x = changed.x0; x < changed.x1; x++)
			line[x] = srcline[x];
	}
	written = { place.X + changed.x0, place.Y + changed.y0, place.X + changed.x1, place.Y + changed.y1 };

	// At an edge, the border beside the change again, each of its texels
	// the edge texel nearest it
	bool left = changed.x0 == 0, right = changed.x1 == width, top = changed.y0 == 0, bottom = changed.y1 == height;
	if (!left && !right && !top && !bottom)
		return;
	int x0 = left ? -Border : changed.x0;
	int x1 = right ? width + Border : changed.x1;
	int y0 = top ? -Border : changed.y0;
	int y1 = bottom ? height + Border : changed.y1;
	for (int y = y0; y < y1; y++)
	{
		vec4* line = dest + (size_t)(place.Y + y) * PageSize + place.X;
		const vec4* srcline = src + (size_t)std::clamp(y, 0, height - 1) * width;
		bool inside = y >= 0 && y < height;
		for (int x = x0; x < x1; x++)
		{
			if (inside && x >= 0 && x < width)
			{
				x = width - 1;
				continue;
			}
			line[x] = srcline[std::clamp(x, 0, width - 1)];
		}
	}
	written = { place.X + x0, place.Y + y0, place.X + x1, place.Y + y1 };
}

void LightmapAtlas::Clear()
{
	Pages.clear();
	Generation++;
}
