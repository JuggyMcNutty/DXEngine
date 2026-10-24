
#include "Precomp.h"
#include "SoftwareRasterizer.h"
#include <algorithm>

void SoftwareFrame::Resize(int width, int height)
{
	width = std::max(width, 1);
	height = std::max(height, 1);
	if (width == Width && height == Height)
		return;

	Width = width;
	Height = height;
	TilesX = (width + TileSize - 1) / TileSize;
	TilesY = (height + TileSize - 1) / TileSize;
	Pitch = TilesX * TileSize;
	size_t count = (size_t)Pitch * TilesY * TileSize;
	Color.Resize(count);
	Depth.Resize(count);
	Clear(0xff000000, 1.0f);
}

void SoftwareFrame::Clear(uint32_t color, float depth)
{
	std::fill(Color.Data(), Color.Data() + Color.Size(), color);
	std::fill(Depth.Data(), Depth.Data() + Depth.Size(), depth);
}
