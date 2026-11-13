#pragma once

#include "Math/vec.h"
#include "Math/FrustumPlanes.h"

class BBox;

class ClipSpan
{
public:
	int16_t x0;
	int16_t x1;
};

class PortalSpan
{
public:
	int64_t y;
	int16_t x0;
	int16_t x1;
};

class BspClipper
{
public:
	BspClipper();
	~BspClipper();

	// The occlusion grid's size. Its cost is per row, so it follows the image
	// being drawn (VisibleFrame::Process) rather than a fixed 1080 rows:
	// capped at MaxWidth x MaxHeight, a 4K frame, so that every image row and
	// column up to it has its sample line (capped at 2048 x 1080, a 1440p or
	// 4K frame was scaled into the grid, and a surface or an actor shown only
	// between two sample lines was culled).
	void SetViewportSize(int width, int height);
	enum { MaxWidth = 4096, MaxHeight = 2160 };

	void Setup(const mat4& world_to_projection, const Array<PortalSpan>& portalSpans, const vec4& portalPlane);

	// Where in a pixel the grid samples a polygon that covers it, from its
	// upper left corner, in the frame's coordinates about its middle. The
	// original's span buffer takes a surface's pixel by its lower right
	// corner in coordinates about FX15, half a pixel past the middle: at its
	// centre, 0.5. An occlusion proxy's rectangle, a sprite's rounded up, by
	// its upper left corner, 0; a mesh's box rounded down, by its lower
	// right, 1 (dx-reverse-info render-dll.md, a frame; which actors are
	// drawn).
	void SetPixelSample(float sample) { PixelSample = sample; }

	bool CheckSurface(const vec3* vertices, uint32_t count, bool solid);

	// A box's rectangle, each side the pixel it falls in, the right and the
	// bottom left out, as the original's BoundVisible makes it.
	bool IsAABBVisible(const BBox& bbox);

	// Whether any of the polygon shows through the spans, with nothing
	// filled or counted: the occlusion proxies' test.
	bool IsPolygonVisible(const vec3* vertices, uint32_t count);

	Array<PortalSpan> CheckPortal(const vec3* vertices, uint32_t count);

	// What of a polygon shows through the spans left, with nothing filled: a
	// mover's mirror, which is no part of the BSP walk (VisibleFrame::
	// ProcessMoverMirrors).
	Array<PortalSpan> ProjectPortal(const vec3* vertices, uint32_t count);

	int numDrawSpans;
	int numSurfs;
	int numTris;

	// STAT OCCLUSION's figures of a walk (VisibleFrame::Process), as the
	// original's ClipBspSurf, its raster setup and BoundVisible count them
	// (Render.dll 0x10b14d05-0x10b14f88, 0x10b19e31-0x10b19e88,
	// 0x10b1631e-0x10b17049): the polygons transformed and their points;
	// those not wholly beyond one side of the view; those given to the rows
	// and those of them over a row of the grid; the boxes tested, those
	// wholly behind the view, those the near plane cuts, those wholly beyond
	// a side of the view, and those the spans hide; and, while Timed, the
	// time clipping, projecting and stepping the rows, and testing the
	// boxes. An occlusion proxy's test counts none (IsPolygonVisible).
	struct ClipperStats
	{
		int Transforms = 0, Points = 0, Clips = 0, Rasters = 0, RasterAccepts = 0;
		int Boxes = 0, BoxBacks = 0, BoxIns = 0, BoxOuts = 0, BoxHidden = 0;
		double ClipTime = 0.0, RasterTime = 0.0, SpanTime = 0.0, BoxTime = 0.0;
		void Add(const ClipperStats& other);
	} Stats;
	bool Timed = false;

private:
	bool IsVisible(int16_t y, int16_t x0, int16_t x1);

	// A convex polygon clipped once and drawn a row at a time, one span to a
	// row, each edge stepped as DrawClippedTriangle steps it: a fan of
	// triangles drew as many spans to a row as it had triangles there, and
	// clipped each triangle on its own. One of more vertices goes by the fan.
	enum { MaxPolygonVertices = 64 };
	bool DrawPolygon(const vec3* vertices, uint32_t count, bool solid);
	bool DrawFan(const vec3* vertices, uint32_t count, bool solid);

	struct ShadedVertex
	{
		vec4 position;
		float clipDistance;
	};

	bool DrawTriangle(const ShadedVertex* const* vert, bool solid, bool ccw);
	int ClipEdge(const ShadedVertex* const* verts);

	bool DrawClippedTriangle(const vec4* const* vertices, bool solid);
	static int16_t PixelFloor(float v, int limit);
	bool DrawSpan(int16_t y, int16_t x0, int16_t x1, bool solid);

	static bool IsDegenerate(const vec4* const* vert);
	static bool IsFrontfacing(const vec4* const* vert);
	static void SortVertices(const vec4* const* vertices, const vec4** sortedVertices);

	// A row of the grid, its spans in order with a sentinel at each end: in
	// a cache line of its own while they fit, else in Overflow[y]
	struct alignas(64) ClipRow
	{
		enum { InlineSpans = 15, Spilled = 0xffff };
		uint16_t Count = 0;
		ClipSpan Spans[InlineSpans];
	};
	ClipSpan* RowSpans(int y, size_t& count);
	void RowInsert(int y, size_t pos, ClipSpan span);
	void RowErase(int y, size_t pos);

	Array<ClipRow> Rows;
	Array<Array<ClipSpan>> Overflow;
	FrustumPlanes FrustumClip;
	mat4 WorldToProjection;
	vec4 PortalPlane = vec4(0.0f);

	Array<PortalSpan> VisibleSpans;
	bool CollectSpans = false;
	bool CollectOnly = false;

	enum { max_additional_vertices = 16 };
	float weightsbuffer[max_additional_vertices * 3 * 2];
	float* weights = nullptr;

	int ViewportWidth = MaxWidth;
	int ViewportHeight = MaxHeight;
	float PixelSample = 0.5f;
};
