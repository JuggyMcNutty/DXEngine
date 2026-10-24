
#include "Precomp.h"
#include "SoftwareRasterizer.h"
#include "SoftwareSetup.h"
#include "SoftwareReference.h"
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

namespace
{
	// A quad of a primitive, lanes in mask: depth tested, shaded, blended,
	// written. Each lane's attributes are the primitive's planes at its
	// centre, over w for the perspective.
	void DrawQuad(SoftwareFrame& frame, const SoftwareRasterJob& job, const SoftwareDraw& draw, const SoftwarePrimitive& prim, int qx, int qy, int mask)
	{
		float depth[4];
		float dx[4], dy[4];
		for (int lane = 0; lane < 4; lane++)
		{
			int x = qx + (lane & 1);
			int y = qy + (lane >> 1);
			dx[lane] = x + 0.5f - prim.RefX;
			dy[lane] = y + 0.5f - prim.RefY;
			float z = prim.Depth.Value + prim.Depth.Dx * dx[lane] + prim.Depth.Dy * dy[lane];
			depth[lane] = std::clamp(z, draw.DepthMin, draw.DepthMax);
			if ((mask & (1 << lane)) && draw.DepthTest && !(depth[lane] <= frame.DepthRow(y)[x]))
				mask &= ~(1 << lane);
		}
		if (!mask)
			return;

		SoftwareQuad quad;
		for (int lane = 0; lane < 4; lane++)
		{
			float q = prim.InvW.Value + prim.InvW.Dx * dx[lane] + prim.InvW.Dy * dy[lane];
			float w = 1.0f / q;
			quad.W[lane] = w;
			float a[SoftwarePrimitive::NumAttributes];
			for (int i = 0; i < SoftwarePrimitive::NumAttributes; i++)
			{
				const SoftwarePlane& p = prim.Attributes[i];
				a[i] = (p.Value + p.Dx * dx[lane] + p.Dy * dy[lane]) * w;
			}
			for (int i = 0; i < 4; i++)
			{
				quad.U[i][lane] = a[i * 2];
				quad.V[i][lane] = a[i * 2 + 1];
				quad.Color[i][lane] = a[8 + i];
			}
		}

		float out[4][4];
		int kept = SoftwareReference::Shade(draw, quad, mask, job.LODBias, out);
		for (int lane = 0; lane < 4; lane++)
		{
			if (!(kept & (1 << lane)))
				continue;
			int x = qx + (lane & 1);
			int y = qy + (lane >> 1);
			float src[4] = { out[0][lane], out[1][lane], out[2][lane], out[3][lane] };
			uint32_t& pixel = frame.ColorRow(y)[x];
			pixel = SoftwareReference::Blend(draw, src, pixel);
			if (draw.DepthWrite)
				frame.DepthRow(y)[x] = depth[lane];
		}
	}

	// A rectangle's pixels within the tile, a quad at a time
	void DrawRectangle(SoftwareFrame& frame, const SoftwareRasterJob& job, const SoftwarePrimitive& prim, int x0, int y0, int x1, int y1)
	{
		const SoftwareDraw& draw = job.Commands->Draws[prim.Draw];
		int xa = std::max(prim.MinX, x0), xb = std::min(prim.MaxX, x1);
		int ya = std::max(prim.MinY, y0), yb = std::min(prim.MaxY, y1);
		for (int qy = ya & ~1; qy < yb; qy += 2)
		{
			for (int qx = xa & ~1; qx < xb; qx += 2)
			{
				int mask = 0;
				for (int lane = 0; lane < 4; lane++)
				{
					int x = qx + (lane & 1);
					int y = qy + (lane >> 1);
					if (x >= xa && x < xb && y >= ya && y < yb)
						mask |= 1 << lane;
				}
				DrawQuad(frame, job, draw, prim, qx, qy, mask);
			}
		}
	}

	// The screen flash: the frame kept at its scale, the colour added, where
	// the depth test passes it
	void DrawFlash(SoftwareFrame& frame, const SoftwareRasterJob& job, const SoftwarePrimitive& prim, int x0, int y0, int x1, int y1)
	{
		const SoftwareDraw& draw = job.Commands->Draws[prim.Draw];
		int xa = std::max(prim.MinX, x0), xb = std::min(prim.MaxX, x1);
		int ya = std::max(prim.MinY, y0), yb = std::min(prim.MaxY, y1);
		float src[4] = { draw.Constant.x, draw.Constant.y, draw.Constant.z, draw.Constant.w };
		for (int y = ya; y < yb; y++)
		{
			uint32_t* color = frame.ColorRow(y);
			const float* depth = frame.DepthRow(y);
			for (int x = xa; x < xb; x++)
			{
				if (prim.Depth.Value <= depth[x])
					color[x] = SoftwareReference::Blend(draw, src, color[x]);
			}
		}
	}
}

void SoftwareRasterizer::DrawTile(SoftwareFrame& frame, const SoftwareRasterJob& job, int tileX, int tileY)
{
	const int x0 = tileX * SoftwareFrame::TileSize;
	const int y0 = tileY * SoftwareFrame::TileSize;
	const int x1 = x0 + SoftwareFrame::TileSize;
	const int y1 = y0 + SoftwareFrame::TileSize;

	if (job.Clear)
	{
		for (int y = y0; y < y1; y++)
		{
			std::fill(frame.ColorRow(y) + x0, frame.ColorRow(y) + x1, job.ClearColor);
			std::fill(frame.DepthRow(y) + x0, frame.DepthRow(y) + x1, 1.0f);
		}
	}

	const int tile = tileY * frame.TilesX + tileX;
	for (size_t c = 0; c < job.ChunkCount; c++)
	{
		const SoftwareBins& bins = job.Bins[c];
		for (uint32_t i = bins.Offsets[tile], end = bins.Offsets[tile + 1]; i < end; i++)
		{
			const SoftwarePrimitive& prim = bins.Primitives[bins.Entries[i]];
			switch (prim.Type)
			{
			case SoftwarePrimitiveType::Rectangle:
				DrawRectangle(frame, job, prim, x0, y0, x1, y1);
				break;
			case SoftwarePrimitiveType::ClearZ:
				for (int y = y0; y < y1; y++)
					std::fill(frame.DepthRow(y) + x0, frame.DepthRow(y) + x1, 1.0f);
				break;
			case SoftwarePrimitiveType::Flash:
				DrawFlash(frame, job, prim, x0, y0, x1, y1);
				break;
			default:
				break;
			}
		}
	}
}
