
#include "Precomp.h"
#include "TraceAABBModel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include <cmath>

// A box takes no node flags, as the original's box check reads none (Engine.dll
// 0x103f42f0; dx-reverse-info/engine-dll.md, traces): an NF_NotVisBlocking
// surface stops it as any other, a line's trace alone keeping its flag
CollisionHitList TraceAABBModel::Trace(UModel* model, const vec3& start, const vec3& end, const vec3& extents, bool actorBrush, const mat4* objectToWorld)
{
	Model = model;
	ActorBrush = actorBrush;
	Placed = objectToWorld != nullptr;
	if (Placed)
	{
		const mat4& m = *objectToWorld;
		dvec3 a0(m.matrix[0], m.matrix[1], m.matrix[2]);
		dvec3 a1(m.matrix[4], m.matrix[5], m.matrix[6]);
		dvec3 a2(m.matrix[8], m.matrix[9], m.matrix[10]);
		double det = dot(a0, cross(a1, a2));
		PlaneAxes[0] = cross(a1, a2) / det;
		PlaneAxes[1] = cross(a2, a0) / det;
		PlaneAxes[2] = cross(a0, a1) / det;
		Translation = dvec3(m.matrix[12], m.matrix[13], m.matrix[14]);
	}
	// The trace as the original's check is given it, in floats: the start,
	// the end the caller asked for (the multi-hit check's cut end for an
	// actor's brush), and the box
	Start = start;
	End = end;
	Extents = extents;
	HitTime = 2.0f;
	CollisionHitList hits;
	if (!Model || Model->Nodes.empty())
		return hits;
	Walk(0, 0, Model->RootOutside != 0, hits);
	return hits;
}

// The original's walk down the tree (Engine.dll 0x103f42f0): each side of a
// node that either end's box, grown by a tenth, reaches -- the side the start
// lies toward first, then the other -- the space kept as a line's walk keeps
// it (TraceRayModel: the front of a solid boundary empty, its back solid);
// where a way ends at no child in solid, the hull of the node it hangs from,
// and no other. A hull whose solid the box does not reach is never tried,
// though the box would enter it within twice the trace's length. Its sums in
// floats, in the original's order.
void TraceAABBModel::Walk(int iParent, int iNode, bool outside, CollisionHitList& hits)
{
	vec3 grown = Extents * 1.1f;
	while (iNode >= 0)
	{
		BspNode* node = &Model->Nodes[iNode];
		vec4 plane = WalkPlane(node);
		float dist0 = ((plane.z * Start.z + plane.y * Start.y) + plane.x * Start.x) - plane.w;
		float dist1 = ((plane.x * End.x + plane.y * End.y) + plane.z * End.z) - plane.w;
		float reach = (std::abs(grown.x * plane.x) + std::abs(grown.z * plane.z)) + std::abs(grown.y * plane.y);
		bool reachesFront = dist0 >= -reach || dist1 >= -reach;
		bool reachesBack = dist0 <= reach || dist1 <= reach;
		bool csg = node->NumVertices > 0 && (node->NodeFlags & (NF_NotCsg | NF_IsNew)) == 0;

		iParent = iNode;
		if (dist0 >= dist1)
		{
			if (reachesFront)
				Walk(iParent, node->Front, outside || csg, hits);
			if (!reachesBack)
				return;
			outside = outside && !csg;
			iNode = node->Back;
		}
		else
		{
			if (reachesBack)
				Walk(iParent, node->Back, outside && !csg, hits);
			if (!reachesFront)
				return;
			outside = outside || csg;
			iNode = node->Front;
		}
	}

	if (!outside)
		TraceHull(&Model->Nodes[iParent], hits);
}

dvec4 TraceAABBModel::NodePlane(const BspNode* node) const
{
	dvec4 plane((double)node->PlaneX, (double)node->PlaneY, (double)node->PlaneZ, (double)node->PlaneW);
	if (!Placed)
		return plane;
	dvec3 normal = PlaneAxes[0] * plane.x + PlaneAxes[1] * plane.y + PlaneAxes[2] * plane.z;
	double length = std::sqrt(dot(normal, normal));
	return dvec4(normal / length, (plane.w + dot(normal, Translation)) / length);
}

// A node's plane as the walk and the hulls test it, in floats: the level's
// as its BSP keeps it, a brush's turned into the world
vec4 TraceAABBModel::WalkPlane(const BspNode* node) const
{
	if (!Placed)
		return vec4(node->PlaneX, node->PlaneY, node->PlaneZ, node->PlaneW);
	return vec4(NodePlane(node));
}

// A solid leaf's hull (0x103f42f0): its planes, at most 64, each flipped
// where its index carries 0x40000000; for the level's hulls the faces of the
// box stored after them; then the bevels each pair of planes' signs call
// for. Any of them that leaves the box no time in the hull misses it, and the
// hull is hit only when the box enters it sooner than the nearest hit so far
// and is still in it after the start.
void TraceAABBModel::TraceHull(BspNode* node, CollisionHitList& hits)
{
	if (node->CollisionBound < 0)
		return;

	const int32_t* hullIndexList = &Model->LeafHulls[node->CollisionBound];
	vec4 planes[64];
	int signs[64];
	int hullPlanesCount = 0;
	while (hullPlanesCount < 64 && hullIndexList[hullPlanesCount] >= 0)
	{
		int32_t hullIndex = hullIndexList[hullPlanesCount];
		vec4 plane = WalkPlane(&Model->Nodes[hullIndex & ~0x4000'0000]);
		if (hullIndex & 0x4000'0000)
			plane = -plane;
		planes[hullPlanesCount] = plane;
		signs[hullPlanesCount] =
			(plane.x < 0.0f ? 1 : plane.x > 0.0f ? 2 : 0) |
			(plane.y < 0.0f ? 4 : plane.y > 0.0f ? 8 : 0) |
			(plane.z < 0.0f ? 16 : plane.z > 0.0f ? 32 : 0);
		hullPlanesCount++;
	}

	// Read after the 64th plane for a longer hull, as the original reads it
	const vec3* bboxStart = (const vec3*)(&hullIndexList[hullPlanesCount + 1]);

	BBox bbox;
	bbox.min = bboxStart[0];
	bbox.max = bboxStart[1];

	// AABB/hull sweep test: the segment of the trace inside the hull's
	// planes, each pushed out by the box, its start the hit. The original's
	// box check bounds only the level's hulls by their boxes, after their own
	// planes; an actor's brush has none.
	SweepCursor cursor(Start, End, Extents, HitTime);
	for (int i = 0; i < hullPlanesCount; i++)
	{
		if (!cursor.ClipPlane(planes[i]))
			return;
	}

	if (!ActorBrush && !cursor.ClipOriginalBoxPlanes(bbox))
		return;

	// Bevels at the hull's edges, for each pair of planes on each axis in turn
	for (int i = 0; i < hullPlanesCount; i++)
	{
		for (int j = 0; j < i; j++)
		{
			int both = signs[i] | signs[j];
			if ((both & 3) == 3 && !cursor.ClipBevel(planes[i], planes[j], 0))
				return;
			if ((both & 12) == 12 && !cursor.ClipBevel(planes[i], planes[j], 1))
				return;
			if ((both & 48) == 48 && !cursor.ClipBevel(planes[i], planes[j], 2))
				return;
		}
	}

	// Did we hit anything? The original's hit is where the box touches, and
	// it counts a hull entered up to twice the trace's length: its caller's
	// backoff may bring that within the trace. Only the nearest is kept, its
	// time of the trace, one under 0 (a start inside) held to 0.
	float time;
	if (cursor.Hit(time))
	{
		HitTime = time;
		hits.clear();
		CollisionHit hit = { std::max(time, 0.0f), cursor.HitNormal(), nullptr, node, node };
		hits.push_back(hit);
	}
}
