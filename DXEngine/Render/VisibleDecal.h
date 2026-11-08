#pragma once

#include "Math/vec.h"

class VisibleFrame;
class BspNode;
class UMover;
struct LevelDecal;

class VisibleDecal
{
public:
	// A level node's decals, drawn over it: its polygon as drawn
	void DrawDecals(VisibleFrame* frame, BspNode* node, const vec3* points, int count);

	// A mover's polygon's decals, drawn over it: the polygon's surface, the
	// polygon in the world as drawn
	void DrawMoverDecals(VisibleFrame* frame, UMover* mover, int surf, const vec3* points, int count);

	// Each of the decals counted as drawn that shows on a polygon of their
	// surface the walk finds a span of
	static void StampDecals(VisibleFrame* frame, const Array<LevelDecal>& decals, const vec3& base, const vec3& normal, UMover* mover, const vec3* points, int count);

	// A decal as the original's ClipDecal leaves it on one of its surface's
	// polygons, the polygon as the walk saves it (VisibleFrame::
	// ViewPolygon): false when nothing is left
	static bool Clip(const vec3& eye, const LevelDecal& decal, const vec3& base, const vec3& normal, UMover* mover, const Array<vec3>& poly, Array<vec3>& points, Array<vec2>* uvs);

private:
	void Draw(VisibleFrame* frame, const Array<LevelDecal>& decals, const vec3& base, const vec3& normal, UMover* mover, const vec3* points, int count);
};
