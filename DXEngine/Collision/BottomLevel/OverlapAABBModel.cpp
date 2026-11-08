#include "Precomp.h"
#include "OverlapAABBModel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Math/coords.h"
#include <cmath>
#include <cstring>

// The original's box check (dx-reverse-info engine-dll.md, traces). Its sums
// and products are floats', in its order: its x87 runs at 24-bit precision
// (NObject.cpp), so a box resting on a plane meets it, or misses it, as the
// original's does. A sum of two comes out the same either way round; one of
// three is kept as the original adds it.

// ToWorld's axes are the turn's rows -- the unit coords turned by the
// rotator, as InverseRotation turns them --, each times the scale for a
// vector and over it for a plane, alike for the stock brushes' MainScale of
// 1 or -1
BrushPlanes::BrushPlanes(const vec3& location, const Rotator& rotation, const vec3& scale, const vec3& postScale, const vec3& prePivot) : Location(location), PostScale(postScale), PrePivot(prePivot)
{
	Coords turn = Coords::InverseRotation(rotation);
	XAxis = turn.XAxis / scale;
	YAxis = turn.YAxis / scale;
	ZAxis = turn.ZAxis / scale;
	TurnX = turn.XAxis * scale;
	TurnY = turn.YAxis * scale;
	TurnZ = turn.ZAxis * scale;
}

// The brush's plane where the brush stands: the normal dotted with each axis
// and over PostScale along the world's axes, W the place's dot with the new
// normal less PrePivot's with the old -- the plane the brush's points on it
// go to. With a MainScale of 1 or -1 and no PostScale it is the original's
// TransformPlaneByOrtho (Core.dll 0x1011a000) with the owner's ToWorld. The
// original's checks meet a brush with a PostScale where its points are, as
// its filing puts them: on DXMP_CMD, where the ortho plane PostScale
// stretches would fill a box 2,300 units across round DeusExMover10, a bot
// door scaled to 6 by 80 by 80, the original's lines from within it meet
// RepairBot2 and the walls beyond and never the door (PostScaleConsole), as
// the door's own place gives. W in doubles, within a bit of the original's
// chain of floats.
vec4 BrushPlanes::Plane(const vec4& p) const
{
	vec4 world;
	world.x = (XAxis.y * p.y + XAxis.z * p.z) + p.x * XAxis.x;
	world.y = (YAxis.y * p.y + YAxis.z * p.z) + p.x * YAxis.x;
	world.z = (ZAxis.y * p.y + ZAxis.z * p.z) + p.x * ZAxis.x;
	world = vec4(world.x / PostScale.x, world.y / PostScale.y, world.z / PostScale.z, 0.0f);
	double moved = (double)Location.x * world.x + (double)Location.y * world.y + (double)Location.z * world.z;
	double pivot = (double)PrePivot.x * p.x + (double)PrePivot.y * p.y + (double)PrePivot.z * p.z;
	world.w = (float)((double)p.w + moved - pivot);
	return world;
}

// UModel::LineCheck's hit normal with an owner (0x103f3e41): the walk's
// node's own normal turned by the owner's ToWorld as a vector, PostScale
// times the turned and mirrored normal along the world's axes
vec3 BrushPlanes::HitNormal(const vec3& n) const
{
	vec3 turned;
	turned.x = (TurnX.y * n.y + TurnX.z * n.z) + n.x * TurnX.x;
	turned.y = (TurnY.y * n.y + TurnY.z * n.z) + n.x * TurnY.x;
	turned.z = (TurnZ.y * n.y + TurnZ.z * n.z) + n.x * TurnZ.x;
	return vec3(turned.x * PostScale.x, turned.y * PostScale.y, turned.z * PostScale.z);
}

// UModel::PointCheck (Engine.dll 0x103f1570): a model of no nodes solid or
// empty as its root is; no extent, the point; else the box down the tree.
// The original's hit also carries a place and a normal (the plane the box's
// middle lies nearest in front of, the box pushed out 1.02 times the
// overlap), which no caller reads: FindSpot and the encroachment ask only
// whether it hit. No caller passes node flags (the walk would read them).
bool OverlapAABBModel::TestOverlap(UModel* model, const vec3& location, const vec3& extent, const BrushPlanes* brush)
{
	Model = model;
	Brush = brush;
	Location = location;
	Extent = extent;
	if (!Model)
		return false;
	if (Model->Nodes.empty())
		return Model->RootOutside == 0;
	if (extent.x == 0.0f && extent.y == 0.0f && extent.z == 0.0f)
		return PointInSolid();
	return Walk(0, 0, Model->RootOutside != 0);
}

// With no extent (0x103f16c5): the point down its side of each plane, on the
// plane counting as behind, the front of a solid boundary empty and its back
// solid, any other node leaving the space as it was; in solid where it ends.
bool OverlapAABBModel::PointInSolid()
{
	bool outside = Model->RootOutside != 0;
	int iNode = 0;
	while (iNode >= 0)
	{
		const BspNode& node = Model->Nodes[iNode];
		vec4 plane = NodePlane(node);
		float dist = ((plane.z * Location.z + plane.y * Location.y) + plane.x * Location.x) - plane.w;
		bool csg = node.NumVertices > 0 && (node.NodeFlags & (NF_NotCsg | NF_IsNew)) == 0;
		if (dist > 0.0f)
		{
			outside = outside || csg;
			iNode = node.Front;
		}
		else
		{
			outside = outside && !csg;
			iNode = node.Back;
		}
	}
	return !outside;
}

// The walk (0x103f19b0): down a node's front when the box, grown by a tenth
// on each axis, reaches in front of its plane, then on down its back unless
// the box lies wholly in front of it; the space kept as the point's. Where a
// way ends at no child in solid, the hull of the node it hangs from is
// tried, and no other: a hull the walk passes on its way is not.
bool OverlapAABBModel::Walk(int iParent, int iNode, bool outside)
{
	vec3 grown = Extent * 1.1f;
	while (iNode >= 0)
	{
		const BspNode& node = Model->Nodes[iNode];
		vec4 plane = NodePlane(node);
		float reach = (std::abs(plane.z * grown.z) + std::abs(plane.y * grown.y)) + std::abs(grown.x * plane.x);
		float dist = ((plane.y * Location.y + plane.x * Location.x) + plane.z * Location.z) - plane.w;
		bool csg = node.NumVertices > 0 && (node.NodeFlags & (NF_NotCsg | NF_IsNew)) == 0;
		if (!(dist <= -reach) && Walk(iNode, node.Front, outside || csg))
			return true;
		outside = outside && !csg;
		iParent = iNode;
		iNode = node.Back;
		if (dist > reach)
			return false;
	}
	return !outside && HullOverlap(iParent);
}

// A solid leaf's hull, its node's iCollisionBound: missed when any plane has
// the box wholly in front. First the hull's own planes, at most 64, each
// flipped where its index carries 0x40000000; for the level's (no brush) the
// six faces of the box stored after them, a tenth of a unit out at both ends
// in Z and toward -X and -Y and as much in at +X and +Y, which a brush's hull
// is not given; then the bevels, for each pair of the hull's planes on each
// axis in turn.
bool OverlapAABBModel::HullOverlap(int iNode)
{
	const BspNode& node = Model->Nodes[iNode];
	if (node.CollisionBound < 0)
		return false;

	const int32_t* hull = &Model->LeafHulls[node.CollisionBound];
	vec4 planes[64];
	int signs[64];
	int count = 0;
	while (count < 64 && hull[count] >= 0)
	{
		vec4 plane = NodePlane(Model->Nodes[hull[count] & ~0x4000'0000]);
		if (hull[count] & 0x4000'0000)
			plane = -plane;
		planes[count] = plane;
		signs[count] =
			(plane.x < 0.0f ? 1 : plane.x > 0.0f ? 2 : 0) |
			(plane.y < 0.0f ? 4 : plane.y > 0.0f ? 8 : 0) |
			(plane.z < 0.0f ? 16 : plane.z > 0.0f ? 32 : 0);
		count++;
	}

	for (int i = 0; i < count; i++)
	{
		if (!PlaneOverlap(planes[i]))
			return false;
	}

	if (!Brush)
	{
		// Read after the 64th plane for a longer hull, as the original reads it
		float box[6];
		memcpy(box, &hull[count + 1], sizeof(box));
		vec4 faces[6] =
		{
			{ 0.0f, 0.0f, -1.0f, (float)(0.1 - box[2]) },
			{ 0.0f, 0.0f, 1.0f, (float)(box[5] + 0.1) },
			{ -1.0f, 0.0f, 0.0f, (float)(0.1 - box[0]) },
			{ 1.0f, 0.0f, 0.0f, (float)(box[3] - 0.1) },
			{ 0.0f, -1.0f, 0.0f, (float)(0.1 - box[1]) },
			{ 0.0f, 1.0f, 0.0f, (float)(box[4] - 0.1) }
		};
		for (const vec4& face : faces)
		{
			if (!PlaneOverlap(face))
				return false;
		}
	}

	for (int i = 0; i < count; i++)
	{
		for (int j = 0; j < i; j++)
		{
			int both = signs[i] | signs[j];
			if ((both & 3) == 3 && !Bevel(planes[i], planes[j], 0))
				return false;
			if ((both & 12) == 12 && !Bevel(planes[i], planes[j], 1))
				return false;
			if ((both & 48) == 48 && !Bevel(planes[i], planes[j], 2))
				return false;
		}
	}
	return true;
}

// A bevel: two planes whose normals point opposite ways along the axis while
// they turn the same way round it (their crosses with the axis dotted over
// 0.001) meet at a line (FIntersectPlanes2, 0x103f6600); the plane through
// that line along the axis, facing as the first plane does, is tried as the
// others are. Planes too nearly parallel to meet, or a line along the axis,
// leave the original's bevel NaN, which has nothing wholly in front.
bool OverlapAABBModel::Bevel(const vec4& p1, const vec4& p2, int axis) const
{
	float turn = axis == 0 ? p1.y * p2.y + p1.z * p2.z : axis == 1 ? p1.x * p2.x + p1.z * p2.z : p1.x * p2.x + p1.y * p2.y;
	if (!((double)turn > 0.001))
		return true;

	vec3 d(p1.y * p2.z - p1.z * p2.y, p1.z * p2.x - p2.z * p1.x, p1.x * p2.y - p1.y * p2.x);
	float dd = (d.x * d.x + d.y * d.y) + d.z * d.z;
	if ((double)dd < 1e-6)
		return true;
	float inv = 1.0f / dd;
	vec3 point(
		(d.z * p2.y - d.y * p2.z) * p1.w + (d.y * p1.z - d.z * p1.y) * p2.w,
		(d.x * p2.z - d.z * p2.x) * p1.w + (d.z * p1.x - d.x * p1.z) * p2.w,
		(p1.y * d.x - d.y * p1.x) * p2.w + (d.y * p2.x - d.x * p2.y) * p1.w);
	point = point * inv;
	vec3 line = d * (1.0f / std::sqrt(dd));

	vec3 n = axis == 0 ? vec3(0.0f, -line.z, line.y) : axis == 1 ? vec3(line.z, 0.0f, -line.x) : vec3(-line.y, line.x, 0.0f);
	float nn = (n.x * n.x + n.y * n.y) + n.z * n.z;
	if (!(nn > 0.0f))
		return true;
	n = n * (1.0f / std::sqrt(nn));
	if ((n.z * p1.z + n.y * p1.y) + n.x * p1.x < 0.0f)
		n = -n;
	return PlaneOverlap(vec4(n, (point.y * n.y + point.z * n.z) + point.x * n.x));
}

// The box not wholly in front of the plane: its middle less far in front
// than the box reaches along the normal (FPlane::PlaneDot, Core.dll
// 0x10118520; the reach, Engine.dll 0x103f6590)
bool OverlapAABBModel::PlaneOverlap(const vec4& plane) const
{
	float reach = (std::abs(Extent.z * plane.z) + std::abs(Extent.y * plane.y)) + std::abs(Extent.x * plane.x);
	float dist = ((Location.z * plane.z + Location.y * plane.y) + Location.x * plane.x) - plane.w;
	return !(dist >= reach);
}

vec4 OverlapAABBModel::NodePlane(const BspNode& node) const
{
	vec4 plane(node.PlaneX, node.PlaneY, node.PlaneZ, node.PlaneW);
	return Brush ? Brush->Plane(plane) : plane;
}
