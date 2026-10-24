
#include "Precomp.h"
#include "SoftwareSetup.h"
#include <algorithm>
#include <cmath>

void SoftwareBins::Reset()
{
	Primitives.clear();
	Found.clear();
}

void SoftwareBins::Finish(int tileCount)
{
	Offsets.assign(tileCount + 1, 0);
	for (uint64_t found : Found)
		Offsets[(found >> 32) + 1]++;
	for (int i = 0; i < tileCount; i++)
		Offsets[i + 1] += Offsets[i];
	Entries.resize(Found.size());
	Cursor.assign(Offsets.begin(), Offsets.end() - 1);
	for (uint64_t found : Found)
		Entries[Cursor[found >> 32]++] = (uint32_t)found;
}

namespace
{
	// 8 subpixel bits, as RADV and lavapipe snap
	constexpr int SubpixelBits = 8;
	constexpr float Subpixels = (float)(1 << SubpixelBits);
	// The guard band, in pixels either way of the frame's origin: a snapped
	// coordinate fits in 22 bits
	constexpr float GuardBand = 8192.0f;

	// A vertex transformed: its clip coordinates and its distance from the
	// clip plane
	struct ClipVertex
	{
		vec4 Clip;
		float Distance;
	};

	ClipVertex Transform(const SoftwareState& state, const vec3& p)
	{
		vec4 v(p, 1.0f);
		ClipVertex c;
		c.Clip = state.ObjectToProjection * v;
		c.Distance = state.ClipPlane.x * p.x + state.ClipPlane.y * p.y + state.ClipPlane.z * p.z + state.ClipPlane.w;
		return c;
	}

	// A vertex on the screen: its position snapped, in 1/256 pixels, the
	// window's depth and 1/w
	struct ScreenVertex
	{
		int32_t X = 0;
		int32_t Y = 0;
		float Z = 0.0f;
		float InvW = 0.0f;
	};

	// The viewport's mapping of a clip vertex, as Vulkan's maps it; false
	// outside the guard band
	bool ToScreen(const SoftwareState& state, const SoftwareDraw& draw, const vec4& clip, ScreenVertex& out)
	{
		float invw = 1.0f / clip.w;
		float halfWidth = 0.5f * state.ViewportWidth;
		float halfHeight = 0.5f * state.ViewportHeight;
		float x = state.ViewportX + halfWidth + halfWidth * (clip.x * invw);
		float y = state.ViewportY + halfHeight + halfHeight * (clip.y * invw);
		if (!(std::fabs(x) <= GuardBand && std::fabs(y) <= GuardBand))
			return false;
		out.X = (int32_t)std::lrint(x * Subpixels);
		out.Y = (int32_t)std::lrint(y * Subpixels);
		out.Z = draw.DepthMin + (clip.z * invw) * (draw.DepthMax - draw.DepthMin);
		out.InvW = invw;
		return true;
	}

	float Attribute(const SoftwareVertex& v, int index)
	{
		if (index < 8)
			return index & 1 ? v.TexCoord[index >> 1].y : v.TexCoord[index >> 1].x;
		return v.Color.v[index - 8];
	}

	// The planes of a triangle's attributes, from its snapped corners: each
	// attribute over w and 1/w for the perspective, the depth linear
	bool SetUpPlanes(SoftwarePrimitive& prim, const ScreenVertex& s0, const ScreenVertex& s1, const ScreenVertex& s2, const SoftwareVertex& v0, const SoftwareVertex& v1, const SoftwareVertex& v2)
	{
		double x0 = s0.X / (double)Subpixels, y0 = s0.Y / (double)Subpixels;
		double dx1 = (s1.X - s0.X) / (double)Subpixels, dy1 = (s1.Y - s0.Y) / (double)Subpixels;
		double dx2 = (s2.X - s0.X) / (double)Subpixels, dy2 = (s2.Y - s0.Y) / (double)Subpixels;
		double det = dx1 * dy2 - dx2 * dy1;
		if (det == 0.0)
			return false;
		double inv = 1.0 / det;
		auto plane = [&](double f0, double f1, double f2)
		{
			SoftwarePlane p;
			p.Value = (float)f0;
			p.Dx = (float)(((f1 - f0) * dy2 - (f2 - f0) * dy1) * inv);
			p.Dy = (float)(((f2 - f0) * dx1 - (f1 - f0) * dx2) * inv);
			return p;
		};
		prim.RefX = (float)x0;
		prim.RefY = (float)y0;
		prim.Depth = plane(s0.Z, s1.Z, s2.Z);
		prim.InvW = plane(s0.InvW, s1.InvW, s2.InvW);
		for (int i = 0; i < SoftwarePrimitive::NumAttributes; i++)
			prim.Attributes[i] = plane((double)Attribute(v0, i) * s0.InvW, (double)Attribute(v1, i) * s1.InvW, (double)Attribute(v2, i) * s2.InvW);
		return true;
	}

	// The scissor: the state's viewport within the frame
	bool Scissor(const SoftwareState& state, const SoftwareFrame& frame, int& x0, int& y0, int& x1, int& y1)
	{
		x0 = std::max(state.ViewportX, 0);
		y0 = std::max(state.ViewportY, 0);
		x1 = std::min(state.ViewportX + state.ViewportWidth, frame.Width);
		y1 = std::min(state.ViewportY + state.ViewportHeight, frame.Height);
		return x0 < x1 && y0 < y1;
	}

	// The primitive's bounds within the scissor; false if none are left
	bool ClipBounds(SoftwarePrimitive& prim, const SoftwareState& state, const SoftwareFrame& frame)
	{
		int x0, y0, x1, y1;
		if (!Scissor(state, frame, x0, y0, x1, y1))
			return false;
		prim.MinX = std::max(prim.MinX, x0);
		prim.MinY = std::max(prim.MinY, y0);
		prim.MaxX = std::min(prim.MaxX, x1);
		prim.MaxY = std::min(prim.MaxY, y1);
		return prim.MinX < prim.MaxX && prim.MinY < prim.MaxY;
	}

	// The primitive into every tile its bounds reach
	void Bin(SoftwareBins& bins, const SoftwareFrame& frame, const SoftwarePrimitive& prim)
	{
		uint32_t index = (uint32_t)bins.Primitives.size();
		bins.Primitives.push_back(prim);
		int tx0 = prim.MinX / SoftwareFrame::TileSize;
		int ty0 = prim.MinY / SoftwareFrame::TileSize;
		int tx1 = (prim.MaxX - 1) / SoftwareFrame::TileSize;
		int ty1 = (prim.MaxY - 1) / SoftwareFrame::TileSize;
		for (int ty = ty0; ty <= ty1; ty++)
		{
			for (int tx = tx0; tx <= tx1; tx++)
				bins.Add(ty * frame.TilesX + tx, index);
		}
	}

	// The first pixel whose centre lies at or past a snapped coordinate:
	// a rectangle's left and top edges take a centre on them, its right and
	// bottom ones leave it, as the top-left rule has them
	int FirstCentre(int32_t coordinate)
	{
		return (coordinate - (1 << (SubpixelBits - 1)) + (1 << SubpixelBits) - 1) >> SubpixelBits;
	}

	// A tile: four corners from its top left, clockwise. Square on the
	// screen, at one depth, nothing clipped, it is a rectangle; the GPU
	// devices' two triangles, (0, 1, 2) and (0, 2, 3), cover the same pixels
	// with the same attributes.
	void SetUpTile(const SoftwareCommands& commands, uint32_t drawIndex, const SoftwareFrame& frame, SoftwareBins& bins)
	{
		const SoftwareDraw& draw = commands.Draws[drawIndex];
		const SoftwareState& state = commands.States[draw.State];
		const SoftwareVertex* v = commands.Vertices.data() + draw.FirstVertex;

		ClipVertex c[4];
		ScreenVertex s[4];
		bool rectangle = true;
		for (int i = 0; i < 4 && rectangle; i++)
		{
			c[i] = Transform(state, v[i].Position);
			rectangle = c[i].Clip.w > 0.0f && c[i].Distance >= 0.0f && c[i].Clip.w == c[0].Clip.w && ToScreen(state, draw, c[i].Clip, s[i]);
		}
		rectangle = rectangle && s[0].Y == s[1].Y && s[2].Y == s[3].Y && s[0].X == s[3].X && s[1].X == s[2].X;
		if (!rectangle)
			return; // drawn only as a rectangle

		SoftwarePrimitive prim;
		prim.Type = SoftwarePrimitiveType::Rectangle;
		prim.Draw = drawIndex;
		prim.MinX = FirstCentre(std::min(s[0].X, s[1].X));
		prim.MaxX = FirstCentre(std::max(s[0].X, s[1].X));
		prim.MinY = FirstCentre(std::min(s[0].Y, s[3].Y));
		prim.MaxY = FirstCentre(std::max(s[0].Y, s[3].Y));
		if (!ClipBounds(prim, state, frame))
			return;
		if (!SetUpPlanes(prim, s[0], s[1], s[3], v[0], v[1], v[3]))
			return;
		Bin(bins, frame, prim);
	}

	// The depth buffer cleared over the whole frame
	void SetUpClearZ(uint32_t drawIndex, const SoftwareFrame& frame, SoftwareBins& bins)
	{
		SoftwarePrimitive prim;
		prim.Type = SoftwarePrimitiveType::ClearZ;
		prim.Draw = drawIndex;
		prim.MaxX = frame.Width;
		prim.MaxY = frame.Height;
		Bin(bins, frame, prim);
	}

	// The screen flash over the viewport, the Vulkan device's quad at depth
	// 0 of clip space, the scene's 0.1 in the window
	void SetUpFlash(const SoftwareCommands& commands, uint32_t drawIndex, const SoftwareFrame& frame, SoftwareBins& bins)
	{
		const SoftwareDraw& draw = commands.Draws[drawIndex];
		SoftwarePrimitive prim;
		prim.Type = SoftwarePrimitiveType::Flash;
		prim.Draw = drawIndex;
		prim.MaxX = frame.Width;
		prim.MaxY = frame.Height;
		prim.Depth.Value = draw.DepthMin;
		if (ClipBounds(prim, commands.States[draw.State], frame))
			Bin(bins, frame, prim);
	}
}

void SoftwareSetup::SetUp(const SoftwareCommands& commands, const SoftwareChunk& chunk, const SoftwareFrame& frame, SoftwareBins& bins)
{
	bins.Reset();
	for (uint32_t i = chunk.FirstDraw; i < chunk.LastDraw; i++)
	{
		switch (commands.Draws[i].Type)
		{
		case SoftwareDrawType::Tile: SetUpTile(commands, i, frame, bins); break;
		case SoftwareDrawType::ClearZ: SetUpClearZ(i, frame, bins); break;
		case SoftwareDrawType::Flash: SetUpFlash(commands, i, frame, bins); break;
		default: break;
		}
	}
	bins.Finish(frame.TilesX * frame.TilesY);
}
