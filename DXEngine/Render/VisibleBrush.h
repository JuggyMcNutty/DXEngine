#pragma once

#include "Math/mat.h"
#include "Collision/TopLevel/BrushTracker.h"

class VisibleFrame;
class UActor;
class UModel;
class UMover;
struct Poly;

class VisibleBrush
{
public:
	bool Draw(VisibleFrame* frame, UActor* actor, bool translucentPass);

	// A mover's mirror sealed after its frame: the polygon's depth alone
	void DrawMirrorSeal(VisibleFrame* frame, UMover* mover, const Poly& poly);

	static mat4 MoverToWorld(UMover* mover);

private:
	// The polygon's fragments as the brush tracker last filed them, null for
	// a mover it does not file (BrushTracker::Fragments)
	using Fragments = Array<BrushTracker::FiledFragment>;

	bool DrawBrushPoly(VisibleFrame* frame, UModel* model, const Poly& poly, UMover* mover, bool translucentPass, const Fragments* fragments, bool seal = false);

	mat4 ToWorld;

	// The polygons drawn that carry decals, each with its filed surface:
	// their decals are drawn over them once the mover's polygons are
	struct DecalPoly
	{
		int Surf;
		const Poly* Polygon;
	};
	Array<DecalPoly> DecalPolys;
};
