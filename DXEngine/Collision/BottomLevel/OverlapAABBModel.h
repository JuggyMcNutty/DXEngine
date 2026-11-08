#pragma once

#include "Packages/Engine/Resources/Level/ULevel.h"

// A brush's planes in the world where it stands, as the original's checks
// take an owner's (UModel::PointCheck, Engine.dll 0x103f1570; LineCheck
// 0x103f3c20) through the owner's coordinates (ABrush::ToWorld, 0x1030f310):
// the place, rotation, MainScale and PrePivot, and PostScale after the
// rotation, along the world's axes.
class BrushPlanes
{
public:
	BrushPlanes(const vec3& location, const Rotator& rotation, const vec3& scale, const vec3& postScale, const vec3& prePivot);

	// A plane of the brush's own space (x, y, z, w), in the world
	vec4 Plane(const vec4& plane) const;

	// The normal a line's hit on a plane of the brush's own space is given
	// in the world (0x103f3e41): turned by the coordinates as a vector,
	// PostScale's too, so not a unit where PostScale is not 1
	vec3 HitNormal(const vec3& normal) const;

private:
	vec3 XAxis, YAxis, ZAxis; // the turn's rows over MainScale, for a plane
	vec3 TurnX, TurnY, TurnZ; // and times it, for a vector
	vec3 Location, PostScale, PrePivot;
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
