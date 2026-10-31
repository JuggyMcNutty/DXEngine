#pragma once

#include "Packages/Engine/Resources/Level/ULevel.h"

// A brush's planes in the world where it stands, as the original's box check
// takes an owner's (UModel::PointCheck, Engine.dll 0x103f1570): by
// FPlane::TransformPlaneByOrtho (Core.dll 0x1011a000) with the owner's
// ToWorld coords (ABrush::ToWorld, Engine.dll 0x1030f310). From what
// DXEngine's other brush transforms take (BrushTracker, the traces): the
// place, rotation, MainScale and PrePivot.
class BrushPlanes
{
public:
	BrushPlanes(const vec3& location, const Rotator& rotation, const vec3& scale, const vec3& prePivot);

	// A plane of the brush's own space (x, y, z, w), in the world
	vec4 Plane(const vec4& plane) const;

private:
	vec3 XAxis, YAxis, ZAxis;
	vec3 Location, Scale, PrePivot;
};

class OverlapAABBModel
{
public:
	// The original's box check (UModel::PointCheck): whether a box, its sides
	// along the world's axes, overlaps the model's solid; with no extent,
	// whether the point lies in it. brush: the model is a brush standing
	// there, its planes taken into the world; none, the level's own.
	bool TestOverlap(UModel* model, const vec3& location, const vec3& extent, const BrushPlanes* brush = nullptr);

private:
	bool PointInSolid();
	bool Walk(int iParent, int iNode, bool outside);
	bool HullOverlap(int iNode);
	bool Bevel(const vec4& plane1, const vec4& plane2, int axis) const;
	bool PlaneOverlap(const vec4& plane) const;
	vec4 NodePlane(const BspNode& node) const;

	UModel* Model = nullptr;
	const BrushPlanes* Brush = nullptr;
	vec3 Location = vec3(0.0f);
	vec3 Extent = vec3(0.0f);
};
