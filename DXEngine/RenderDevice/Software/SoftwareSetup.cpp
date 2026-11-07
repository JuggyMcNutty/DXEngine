
#include "Precomp.h"
#include "SoftwareSetup.h"
#include "SoftwarePipelines.h"
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
	constexpr int Subpixels = 1 << SubpixelBits;
	// The guard band, in pixels either way of the frame's origin: a snapped
	// coordinate fits in 22 bits. Clipping keeps a little within it, so a
	// vertex made on its edge still maps inside.
	constexpr float GuardBand = 8192.0f;
	constexpr float ClipGuardBand = 8000.0f;

	constexpr int NumAttributes = SoftwarePrimitive::NumAttributes;

	// The pipelines of the draw being set up, chosen once each
	struct DrawPipelines
	{
		const SoftwareDraw* Draw = nullptr;
		bool GeneralOnly = false;
		SoftwarePipeline Triangle = nullptr;
		SoftwarePipeline Rectangle = nullptr;

		SoftwarePipeline Get(bool rectangle)
		{
			SoftwarePipeline& pipeline = rectangle ? Rectangle : Triangle;
			if (!pipeline)
				pipeline = SoftwarePipelines::Select(*Draw, rectangle, GeneralOnly);
			return pipeline;
		}
	};
	thread_local DrawPipelines CurrentPipelines;

	// A vertex transformed: its clip coordinates, its distance from the
	// clip plane, and its attributes, which clipping carries along
	struct ClipVertex
	{
		vec4 Clip;
		float Distance;
		float Attributes[NumAttributes];
	};

	void Transform(const SoftwareState& state, const SoftwareVertex& v, ClipVertex& c)
	{
		const vec3& p = v.Position;
		c.Clip = state.ObjectToProjection * vec4(p, 1.0f);
		c.Distance = state.ClipPlane.x * p.x + state.ClipPlane.y * p.y + state.ClipPlane.z * p.z + state.ClipPlane.w;
		for (int i = 0; i < 4; i++)
		{
			c.Attributes[i * 2] = v.TexCoord[i].x;
			c.Attributes[i * 2 + 1] = v.TexCoord[i].y;
			c.Attributes[8 + i] = v.Color.v[i];
		}
	}

	// A vertex on the screen: its position snapped, in 1/256 pixels, the
	// window's depth, 1/w and its attributes
	struct ScreenVertex
	{
		int32_t X = 0;
		int32_t Y = 0;
		float Z = 0.0f;
		float InvW = 0.0f;
		const float* Attributes = nullptr;
	};

	// The viewport's mapping of a clip vertex, as Vulkan's maps it; false
	// outside the guard band
	bool ToScreen(const SoftwareState& state, const SoftwareDraw& draw, const ClipVertex& c, ScreenVertex& out)
	{
		float invw = 1.0f / c.Clip.w;
		float halfWidth = 0.5f * state.ViewportWidth;
		float halfHeight = 0.5f * state.ViewportHeight;
		float x = state.ViewportX + halfWidth + halfWidth * (c.Clip.x * invw);
		float y = state.ViewportY + halfHeight + halfHeight * (c.Clip.y * invw);
		if (!(std::fabs(x) <= GuardBand && std::fabs(y) <= GuardBand))
			return false;
		out.X = (int32_t)std::lrint(x * (float)Subpixels);
		out.Y = (int32_t)std::lrint(y * (float)Subpixels);
		out.Z = draw.DepthMin + (c.Clip.z * invw) * (draw.DepthMax - draw.DepthMin);
		out.InvW = invw;
		out.Attributes = c.Attributes;
		return true;
	}

	// The planes a primitive is clipped against, each a function of a vertex
	// not negative inside: the scene node's clip plane, w above nothing, the
	// guard band's four sides, on x/w and y/w, and the near and far planes,
	// z/w from 0 to 1, in every frame, a mirror's too, as the GPU devices clip
	// the depth and D3DDrv's under 0 and over 1 is cut (dx-reverse-info
	// d3ddrv-dll.md, the screen's pixels)
	struct ClipPlanes
	{
		float Left, Right, Top, Bottom;

		ClipPlanes(const SoftwareState& state)
		{
			float halfWidth = std::max(0.5f * state.ViewportWidth, 0.5f);
			float halfHeight = std::max(0.5f * state.ViewportHeight, 0.5f);
			Left = (-ClipGuardBand - state.ViewportX - halfWidth) / halfWidth;
			Right = (ClipGuardBand - state.ViewportX - halfWidth) / halfWidth;
			Top = (-ClipGuardBand - state.ViewportY - halfHeight) / halfHeight;
			Bottom = (ClipGuardBand - state.ViewportY - halfHeight) / halfHeight;
		}

		static constexpr int Count = 8;

		float Distance(const ClipVertex& v, int plane) const
		{
			switch (plane)
			{
			case 0: return v.Distance;
			case 1: return v.Clip.w - 1.0e-5f;
			case 2: return v.Clip.x - Left * v.Clip.w;
			case 3: return Right * v.Clip.w - v.Clip.x;
			case 4: return v.Clip.y - Top * v.Clip.w;
			case 5: return Bottom * v.Clip.w - v.Clip.y;
			case 6: return v.Clip.z;
			default: return v.Clip.w - v.Clip.z;
			}
		}

		bool Inside(const ClipVertex& v) const
		{
			for (int plane = 0; plane < Count; plane++)
			{
				if (!(Distance(v, plane) >= 0.0f))
					return false;
			}
			return true;
		}
	};

	void Lerp(const ClipVertex& a, const ClipVertex& b, float t, ClipVertex& out)
	{
		out.Clip = a.Clip + (b.Clip - a.Clip) * t;
		out.Distance = a.Distance + (b.Distance - a.Distance) * t;
		for (int i = 0; i < NumAttributes; i++)
			out.Attributes[i] = a.Attributes[i] + (b.Attributes[i] - a.Attributes[i]) * t;
	}

	// A convex polygon clipped against each plane in turn (Sutherland and
	// Hodgman): the vertices left, at most 3 + 8 of a triangle's
	constexpr int MaxClipVertices = 16;

	int ClipPolygon(ClipVertex* vertices, int count, ClipVertex* scratch, const ClipPlanes& planes)
	{
		ClipVertex* in = vertices;
		ClipVertex* out = scratch;
		for (int plane = 0; plane < ClipPlanes::Count && count >= 3; plane++)
		{
			int n = 0;
			for (int i = 0; i < count; i++)
			{
				const ClipVertex& a = in[i];
				const ClipVertex& b = in[i + 1 < count ? i + 1 : 0];
				float da = planes.Distance(a, plane);
				float db = planes.Distance(b, plane);
				if (da >= 0.0f)
					out[n++] = a;
				if ((da >= 0.0f) != (db >= 0.0f) && n < MaxClipVertices)
					Lerp(a, b, da / (da - db), out[n++]);
			}
			std::swap(in, out);
			count = n;
		}
		if (in != vertices)
		{
			for (int i = 0; i < count; i++)
				vertices[i] = in[i];
		}
		return count;
	}

	// The planes of a primitive's attributes, from three of its snapped
	// corners: each attribute over w and 1/w for the perspective, the depth
	// linear on the screen
	bool SetUpPlanes(SoftwarePrimitive& prim, const ScreenVertex& s0, const ScreenVertex& s1, const ScreenVertex& s2)
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
		for (int i = 0; i < NumAttributes; i++)
			prim.Attributes[i] = plane((double)s0.Attributes[i] * s0.InvW, (double)s1.Attributes[i] * s1.InvW, (double)s2.Attributes[i] * s2.InvW);
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

	// Whether a triangle's edges leave any of the tile's pixels: the tile's
	// corner most inside each edge is outside it otherwise
	bool TouchesTile(const SoftwarePrimitive& prim, int tx, int ty)
	{
		int64_t x0 = tx * SoftwareFrame::TileSize, y0 = ty * SoftwareFrame::TileSize;
		int64_t last = SoftwareFrame::TileSize - 1;
		for (int k = 0; k < 3; k++)
		{
			int64_t e = prim.EdgeA[k] * x0 + prim.EdgeB[k] * y0 + prim.EdgeC[k];
			e += std::max(prim.EdgeA[k], (int64_t)0) * last + std::max(prim.EdgeB[k], (int64_t)0) * last;
			if (e < 0)
				return false;
		}
		return true;
	}

	// The primitive into every tile its bounds reach, a triangle's only where
	// its edges leave some of the tile
	void Bin(SoftwareBins& bins, const SoftwareFrame& frame, SoftwarePrimitive& prim)
	{
		if (prim.Type == SoftwarePrimitiveType::Triangle || prim.Type == SoftwarePrimitiveType::Rectangle)
			prim.Pipeline = CurrentPipelines.Get(prim.Type == SoftwarePrimitiveType::Rectangle);
		uint32_t index = (uint32_t)bins.Primitives.size();
		bins.Primitives.push_back(prim);
		int tx0 = prim.MinX / SoftwareFrame::TileSize;
		int ty0 = prim.MinY / SoftwareFrame::TileSize;
		int tx1 = (prim.MaxX - 1) / SoftwareFrame::TileSize;
		int ty1 = (prim.MaxY - 1) / SoftwareFrame::TileSize;
		bool test = prim.Type == SoftwarePrimitiveType::Triangle && (tx0 != tx1 || ty0 != ty1);
		for (int ty = ty0; ty <= ty1; ty++)
		{
			for (int tx = tx0; tx <= tx1; tx++)
			{
				if (!test || TouchesTile(prim, tx, ty))
					bins.Add(ty * frame.TilesX + tx, index);
			}
		}
	}

	// The first pixel whose centre lies at or past a snapped coordinate
	int FirstCentre(int32_t coordinate)
	{
		return (coordinate - (Subpixels >> 1) + Subpixels - 1) >> SubpixelBits;
	}

	// A triangle on the screen set up and binned: its edges, bounds, planes
	// and a mover's depth bias
	void SetUpScreenTriangle(SoftwareBins& bins, const SoftwareFrame& frame, const SoftwareState& state, const SoftwareDraw& draw, uint32_t drawIndex, const ScreenVertex& s0, const ScreenVertex& s1, const ScreenVertex& s2)
	{
		// Twice the area, positive clockwise on the screen (y down); either
		// way round is drawn, as no device culls
		int64_t area = (int64_t)(s1.X - s0.X) * (s2.Y - s0.Y) - (int64_t)(s2.X - s0.X) * (s1.Y - s0.Y);
		if (area == 0)
			return;
		const ScreenVertex* v[3] = { &s0, &s1, &s2 };
		if (area < 0)
			std::swap(v[1], v[2]);

		SoftwarePrimitive prim;
		prim.Type = SoftwarePrimitiveType::Triangle;
		prim.Draw = drawIndex;

		// The top-left rule: a centre on a top edge (level, the inside below
		// it) or a left one (the inside to its right) is covered, on a right or
		// bottom one not
		for (int k = 0; k < 3; k++)
		{
			const ScreenVertex& a = *v[k];
			const ScreenVertex& b = *v[k < 2 ? k + 1 : 0];
			int64_t A = (int64_t)a.Y - b.Y;
			int64_t B = (int64_t)b.X - a.X;
			int64_t C = ((int64_t)b.Y - a.Y) * a.X - ((int64_t)b.X - a.X) * a.Y;
			bool topLeft = A > 0 || (A == 0 && B > 0);
			int64_t C1 = (Subpixels >> 1) * (A + B) + C + (topLeft ? 0 : -1);
			prim.EdgeA[k] = A;
			prim.EdgeB[k] = B;
			prim.EdgeC[k] = C1 >> SubpixelBits; // floor
		}

		int32_t minX = std::min({ s0.X, s1.X, s2.X }), maxX = std::max({ s0.X, s1.X, s2.X });
		int32_t minY = std::min({ s0.Y, s1.Y, s2.Y }), maxY = std::max({ s0.Y, s1.Y, s2.Y });
		prim.MinX = FirstCentre(minX);
		prim.MinY = FirstCentre(minY);
		prim.MaxX = ((maxX - (Subpixels >> 1)) >> SubpixelBits) + 1;
		prim.MaxY = ((maxY - (Subpixels >> 1)) >> SubpixelBits) + 1;
		if (!ClipBounds(prim, state, frame))
			return;
		if (!SetUpPlanes(prim, s0, s1, s2))
			return;

		if (draw.DepthBias)
		{
			// Vulkan's depth bias on a float depth buffer, constant and
			// slope -1 toward the viewer, 1 away: the larger of the depth's
			// steps, and the least step of the largest depth's exponent
			float slope = std::max(std::fabs(prim.Depth.Dx), std::fabs(prim.Depth.Dy));
			int exponent = 0;
			std::frexp(std::max({ s0.Z, s1.Z, s2.Z }), &exponent);
			float resolution = std::ldexp(1.0f, exponent - 1 - 23);
			prim.Depth.Value -= (float)draw.DepthBias * (slope + resolution);
		}

		Bin(bins, frame, prim);
	}

	// A convex polygon of clip vertices, clipped where it must be, mapped to
	// the screen and set up as a fan of triangles
	void SetUpClipPolygon(SoftwareBins& bins, const SoftwareFrame& frame, const SoftwareState& state, const SoftwareDraw& draw, uint32_t drawIndex, ClipVertex* polygon, int count)
	{
		ClipPlanes planes(state);
		bool inside = true;
		for (int i = 0; i < count && inside; i++)
			inside = planes.Inside(polygon[i]);
		if (!inside)
		{
			ClipVertex scratch[MaxClipVertices];
			count = ClipPolygon(polygon, count, scratch, planes);
		}
		if (count < 3)
			return;

		ScreenVertex screen[MaxClipVertices];
		for (int i = 0; i < count; i++)
		{
			if (!ToScreen(state, draw, polygon[i], screen[i]))
				return;
		}
		for (int i = 2; i < count; i++)
			SetUpScreenTriangle(bins, frame, state, draw, drawIndex, screen[0], screen[i - 1], screen[i]);
	}

	// A draw's triangles: a polygon's fan (0, i - 1, i), as the GPU devices
	// index it, or its triangles three vertices each; each triangle clipped
	// on its own, as the GPUs clip
	void SetUpTriangles(const SoftwareCommands& commands, uint32_t drawIndex, const SoftwareFrame& frame, SoftwareBins& bins)
	{
		const SoftwareDraw& draw = commands.Draws[drawIndex];
		const SoftwareState& state = commands.States[draw.State];
		const SoftwareVertex* v = commands.Vertices.data() + draw.FirstVertex;
		bool fan = draw.Type == SoftwareDrawType::Polygon;
		uint32_t count = fan ? draw.VertexCount - 2 : draw.VertexCount / 3;

		ClipVertex fanStart;
		if (fan)
			Transform(state, v[0], fanStart);
		for (uint32_t t = 0; t < count; t++)
		{
			ClipVertex polygon[MaxClipVertices];
			if (fan)
			{
				polygon[0] = fanStart;
				Transform(state, v[t + 1], polygon[1]);
				Transform(state, v[t + 2], polygon[2]);
			}
			else
			{
				for (int i = 0; i < 3; i++)
					Transform(state, v[t * 3 + i], polygon[i]);
			}
			SetUpClipPolygon(bins, frame, state, draw, drawIndex, polygon, 3);
		}
	}

	// A tile: four corners from its top left, clockwise. Square on the
	// screen, at one depth, nothing clipped, it is a rectangle, covering the
	// pixels with the attributes of the GPU devices' two triangles, (0, 1, 2)
	// and (0, 2, 3); else it is those triangles.
	void SetUpTile(const SoftwareCommands& commands, uint32_t drawIndex, const SoftwareFrame& frame, SoftwareBins& bins)
	{
		const SoftwareDraw& draw = commands.Draws[drawIndex];
		const SoftwareState& state = commands.States[draw.State];
		const SoftwareVertex* v = commands.Vertices.data() + draw.FirstVertex;

		ClipVertex c[4];
		ScreenVertex s[4];
		ClipPlanes planes(state);
		bool rectangle = true;
		for (int i = 0; i < 4; i++)
		{
			Transform(state, v[i], c[i]);
			rectangle = rectangle && c[i].Clip.w == c[0].Clip.w && planes.Inside(c[i]) && ToScreen(state, draw, c[i], s[i]);
		}
		rectangle = rectangle && s[0].Y == s[1].Y && s[2].Y == s[3].Y && s[0].X == s[3].X && s[1].X == s[2].X;
		if (!rectangle)
		{
			ClipVertex first[MaxClipVertices] = { c[0], c[1], c[2] };
			SetUpClipPolygon(bins, frame, state, draw, drawIndex, first, 3);
			ClipVertex second[MaxClipVertices] = { c[0], c[2], c[3] };
			SetUpClipPolygon(bins, frame, state, draw, drawIndex, second, 3);
			return;
		}

		SoftwarePrimitive prim;
		prim.Type = SoftwarePrimitiveType::Rectangle;
		prim.Draw = drawIndex;
		prim.MinX = FirstCentre(std::min(s[0].X, s[1].X));
		prim.MaxX = FirstCentre(std::max(s[0].X, s[1].X));
		prim.MinY = FirstCentre(std::min(s[0].Y, s[3].Y));
		prim.MaxY = FirstCentre(std::max(s[0].Y, s[3].Y));
		if (!ClipBounds(prim, state, frame))
			return;
		if (!SetUpPlanes(prim, s[0], s[1], s[3]))
			return;
		Bin(bins, frame, prim);
	}

	// A line: its ends clipped as a polygon's edges are, then on the screen
	void SetUpLine(const SoftwareCommands& commands, uint32_t drawIndex, const SoftwareFrame& frame, SoftwareBins& bins)
	{
		const SoftwareDraw& draw = commands.Draws[drawIndex];
		const SoftwareState& state = commands.States[draw.State];
		const SoftwareVertex* v = commands.Vertices.data() + draw.FirstVertex;

		ClipVertex c[2];
		Transform(state, v[0], c[0]);
		Transform(state, v[1], c[1]);
		ClipPlanes planes(state);
		float t0 = 0.0f, t1 = 1.0f;
		for (int plane = 0; plane < ClipPlanes::Count; plane++)
		{
			float d0 = planes.Distance(c[0], plane);
			float d1 = planes.Distance(c[1], plane);
			if (d0 < 0.0f && d1 < 0.0f)
				return;
			if (d0 < 0.0f)
				t0 = std::max(t0, d0 / (d0 - d1));
			else if (d1 < 0.0f)
				t1 = std::min(t1, d0 / (d0 - d1));
		}
		if (t0 >= t1)
			return;
		ClipVertex ends[2];
		Lerp(c[0], c[1], t0, ends[0]);
		Lerp(c[0], c[1], t1, ends[1]);

		ScreenVertex s[2];
		if (!ToScreen(state, draw, ends[0], s[0]) || !ToScreen(state, draw, ends[1], s[1]))
			return;

		SoftwarePrimitive prim;
		prim.Type = SoftwarePrimitiveType::Line;
		prim.Draw = drawIndex;
		for (int i = 0; i < 2; i++)
		{
			prim.LineX[i] = s[i].X / (float)Subpixels;
			prim.LineY[i] = s[i].Y / (float)Subpixels;
			prim.LineZ[i] = s[i].Z;
		}
		prim.MinX = (int)std::floor(std::min(prim.LineX[0], prim.LineX[1]));
		prim.MinY = (int)std::floor(std::min(prim.LineY[0], prim.LineY[1]));
		prim.MaxX = (int)std::floor(std::max(prim.LineX[0], prim.LineX[1])) + 1;
		prim.MaxY = (int)std::floor(std::max(prim.LineY[0], prim.LineY[1])) + 1;
		if (!ClipBounds(prim, state, frame))
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

void SoftwareSetup::SetUp(const SoftwareCommands& commands, const SoftwareChunk& chunk, const SoftwareFrame& frame, SoftwareBins& bins, bool generalOnly)
{
	bins.Reset();
	for (uint32_t i = chunk.FirstDraw; i < chunk.LastDraw; i++)
	{
		CurrentPipelines = DrawPipelines();
		CurrentPipelines.Draw = &commands.Draws[i];
		CurrentPipelines.GeneralOnly = generalOnly;
		switch (commands.Draws[i].Type)
		{
		case SoftwareDrawType::Polygon:
		case SoftwareDrawType::Triangles: SetUpTriangles(commands, i, frame, bins); break;
		case SoftwareDrawType::Tile: SetUpTile(commands, i, frame, bins); break;
		case SoftwareDrawType::Line: SetUpLine(commands, i, frame, bins); break;
		case SoftwareDrawType::ClearZ: SetUpClearZ(i, frame, bins); break;
		case SoftwareDrawType::Flash: SetUpFlash(commands, i, frame, bins); break;
		}
	}
	bins.Finish(frame.TilesX * frame.TilesY);
}
