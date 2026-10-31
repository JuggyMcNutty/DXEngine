
#include "Precomp.h"
#include "TraceAABBModel.h"
#include "Packages/Engine/Resources/Level/UModel.h"

// A box takes no node flags, as the original's box check reads none (Engine.dll
// 0x103f42f0; dx-reverse-info/engine-dll.md, traces): an NF_NotVisBlocking
// surface stops it as any other, a line's trace alone keeping its flag
CollisionHitList TraceAABBModel::Trace(UModel* model, const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, const dvec3& extents, bool original, bool actorBrush, const mat4* objectToWorld)
{
	Model = model;
	Original = original;
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
	CollisionHitList hits;
	if (!Model || Model->Nodes.empty())
		return hits;
	Trace(origin, tmin, dirNormalized, tmax, extents, 0, Model->RootOutside != 0, hits);
	hits.SortByFraction();
	return hits;
}

// The original's walk down the tree (Engine.dll 0x103f42f0): each side of a
// node that either end's box, grown by a tenth, reaches, the space kept as a
// line's walk keeps it (TraceRayModel: the front of a solid boundary empty,
// its back solid); at a solid leaf, the hull its node bounds. A hull whose
// solid the box does not reach is never tried, though the box would enter it
// within twice the trace's length.
void TraceAABBModel::Trace(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, const dvec3& extents, int iNode, bool outside, CollisionHitList& hits)
{
	BspNode* node = &Model->Nodes[iNode];
	dvec3 extentspadded = extents * 1.1;
	int startSide = NodeAABBOverlap(origin, extentspadded, node);
	int endSide = NodeAABBOverlap(origin + dirNormalized * tmax, extentspadded, node);
	bool csg = node->NumVertices > 0 && (node->NodeFlags & (NF_NotCsg | NF_IsNew)) == 0;

	if (startSide <= 0 || endSide <= 0)
	{
		bool frontOutside = outside || csg;
		if (node->Front >= 0)
			Trace(origin, tmin, dirNormalized, tmax, extents, node->Front, frontOutside, hits);
		else if (!frontOutside)
			TraceHull(origin, tmin, dirNormalized, tmax, extents, node, hits);
	}

	if (startSide >= 0 || endSide >= 0)
	{
		bool backOutside = outside && !csg;
		if (node->Back >= 0)
			Trace(origin, tmin, dirNormalized, tmax, extents, node->Back, backOutside, hits);
		else if (!backOutside)
			TraceHull(origin, tmin, dirNormalized, tmax, extents, node, hits);
	}
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

void TraceAABBModel::TraceHull(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, const dvec3& extents, BspNode* node, CollisionHitList& hits)
{
	if (node->CollisionBound >= 0)
	{
		int32_t* hullIndexList = &Model->LeafHulls[node->CollisionBound];
		int hullPlanesCount = 0;
		while (hullIndexList[hullPlanesCount] >= 0)
			hullPlanesCount++;

		vec3* bboxStart = (vec3*)(&hullIndexList[hullPlanesCount + 1]);

		BBox bbox;
		bbox.min = bboxStart[0];
		bbox.max = bboxStart[1];

		// Shave off part of the box, or ammo pickups can fall through the floor
		if (!Original)
		{
			float boxEpsilon = 0.1f;
			bbox.min += boxEpsilon;
			bbox.max -= boxEpsilon;
		}

		// The original's box check bounds only the level's hulls by their
		// boxes, after their own planes (below); an actor's brush has none.
		SweepCursor cursor(origin, dirNormalized, tmax, extents);
		if (Original || cursor.ClipBoxPlanes(bbox))
		{
			// Grab the hull planes and flip the plane direction if the plane points in the wrong direction.
			// On the stack for the usual hull: this runs for every hull a sweep reaches.
			dvec4 planesInline[32];
			Array<dvec4> planesHeap;
			dvec4* planes = planesInline;
			if (hullPlanesCount > 32)
			{
				planesHeap.resize(hullPlanesCount);
				planes = planesHeap.data();
			}
			for (int i = 0; i < hullPlanesCount; i++)
			{
				int32_t hullIndex = hullIndexList[i];
				bool hullFlip = !!(hullIndex & 0x4000'0000);
				hullIndex = hullIndex & ~0x4000'0000;
				dvec4 hullplane = NodePlane(&Model->Nodes[hullIndex]);
				planes[i] = hullFlip ? -hullplane : hullplane;
			}

			// AABB/hull sweep test.
			//
			// This is the same as a ray/hull sweep test, except with extended and bevel planes so that it works for AABB.
			//
			// The basic idea here is that you can find the solid line segment of a ray passing through the planes of a convex hull.
			// While we are not interested in the line segment itself, the start of the line segment will give us the the hit point.
			//
			// We can sweep with an AABB instead of a ray by moving the planes outwards by the extents of the AABB. This will produce
			// inaccuracies in the result, which we can reduce by adding bevel planes when the angle between the planes passes a threshold.

			// Check for collision for each hull plane
			for (int i = 0; i < hullPlanesCount; i++)
			{
				if (!cursor.ClipPlane(planes[i]))
				{
					break;
				}
			}

			if (Original && !ActorBrush)
				cursor.ClipOriginalBoxPlanes(bbox);

			// Check for collision for any bevel plane we need to insert at the hull edges
			for (int i = 0; i < hullPlanesCount; i++)
			{
				dvec4 plane0 = planes[i];
				for (int j = 0; j < i; j++)
				{
					dvec4 plane1 = planes[j];

					if ((plane0.x < 0.0 && plane1.x > 0.0) || (plane0.x > 0.0 && plane1.x < 0.0))
					{
						cursor.ClipBevel(plane0, plane1, dvec3(1.0, 0.0, 0.0));
					}
					if ((plane0.y < 0.0 && plane1.y > 0.0) || (plane0.y > 0.0 && plane1.y < 0.0))
					{
						cursor.ClipBevel(plane0, plane1, dvec3(0.0, 1.0, 0.0));
					}
					if ((plane0.z < 0.0 && plane1.z > 0.0) || (plane0.z > 0.0 && plane1.z < 0.0))
					{
						cursor.ClipBevel(plane0, plane1, dvec3(0.0, 0.0, 1.0));
					}
				}
			}

			// Did we hit anything? The original's hit is where the box
			// touches, and it counts a hull entered up to twice the trace's
			// length: its caller's backoff may bring that within the trace.
			double t;
			if (cursor.Hit(t))
			{
				if (Original)
					t = std::max(t, 0.0);
				else
					t = std::max(t - 0.1, 0.0);
				if (t >= tmin && (Original || t < tmax))
				{
					CollisionHit hit = { (float)t, vec3(cursor.HitNormal()), nullptr, node, node };
					hits.push_back(hit);
				}
			}
		}
	}
}

double TraceAABBModel::TriangleAABBIntersect(const dvec3& from, const dvec3& to, const dvec3& extents, const dvec3* points)
{
	// Simplify check by re-orienting to center
	dvec3 v[3] =
	{
		points[0] - to,
		points[1] - to,
		points[2] - to
	};

	dvec3 f[3] =
	{
		v[1] - v[0],
		v[2] - v[1],
		v[0] - v[2]
	};

	// TODO: Optimize
	dvec3 a[9] =
	{
		{0, -f[0].z, f[0].y}, // a00
		{0, -f[1].z, f[1].y}, // a01
		{0, -f[2].z, f[2].y}, // a02
		{f[0].z, 0, -f[0].x}, // a10
		{f[1].z, 0, -f[1].x}, // a11
		{f[2].z, 0, -f[2].x}, // a12
		{-f[0].y, f[0].x, 0}, // a20
		{-f[1].y, f[1].x, 0}, // a21
		{-f[2].y, f[2].x, 0}  // a22
	};

	dvec3 p;
	double r;

	for (int i = 0; i < 9; i++)
	{
		p[0] = dot(v[0], a[i]);
		p[1] = dot(v[1], a[i]);
		p[2] = dot(v[2], a[i]);
		r = extents.x * std::abs(a[i].x) + extents.y * std::abs(a[i].y) + extents.z * std::abs(a[i].z);
	
		double m[2];
		m[0] = std::max(std::max(p[0], p[1]), p[2]);
		m[1] = std::min(std::min(p[0], p[1]), p[2]);
	
		if (std::max(-m[0], m[1]) > r)
			return 1.0;
	}

	// Test the three axes corresponding to the face normals of the AABB (category 1)
	if (std::max(std::max(v[0].x, v[1].x), v[2].x) < -extents[0] || std::min(std::min(v[0].x, v[1].x), v[2].x) > extents[0])
		return 1.0;

	if (std::max(std::max(v[0].y, v[1].y), v[2].y) < -extents[1] || std::min(std::min(v[0].y, v[1].y), v[2].y) > extents[1])
		return 1.0;

	if (std::max(std::max(v[0].z, v[1].z), v[2].z) < -extents[2] || std::min(std::min(v[0].z, v[1].z), v[2].z) > extents[2])
		return 1.0;

	// Test separating axis corresponding to triangle face normal (category 2)
	dvec3 pn = cross(f[0], f[1]);
	double pd = std::abs(dot(pn, v[0]));

	r = extents.x * std::abs(pn.x) + extents.y * std::abs(pn.y) + extents.z * std::abs(pn.z);
	if (pd > r)
		return 1.0;

	// Get intersection time
	double t = (pd / r);
	return t;
}

double TraceAABBModel::NodeAABBIntersect(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, const dvec3& extents, BspNode* node)
{
	if (node->NumVertices < 3 || (node->Surf >= 0 && Model->Surfaces[node->Surf].PolyFlags & PF_NotSolid))
		return tmax;

	dvec3 target = origin + dirNormalized * tmax;

	// Test if plane is actually crossed
	dvec3 extentspadded = extents * 1.1; // For numerical stability
	int startSide = NodeAABBOverlap(origin, extentspadded, node);
	int endSide = NodeAABBOverlap(origin + dirNormalized * tmax, extentspadded, node);
	if (startSide == 1 && endSide == 1)
		return tmax;

	BspVert* v = &Model->Vertices[node->VertPool];
	vec3* points = Model->Points.data();

	dvec3 p[3];
	p[0] = to_dvec3(points[v[0].Vertex]);
	p[1] = to_dvec3(points[v[1].Vertex]);

	double t = tmax;
	int count = node->NumVertices;
	for (int i = 2; i < count; i++)
	{
		p[2] = to_dvec3(points[v[i].Vertex]);
		double tval = TriangleAABBIntersect(origin, target, extents, p) * tmax;
		if (tval >= tmin)
			t = std::min(tval, t);
		p[1] = p[2];
	}
	return t;
}

// -1 = inside, 0 = intersects, 1 = outside
int TraceAABBModel::NodeAABBOverlap(const dvec3& center, const dvec3& extents, BspNode* node)
{
	dvec4 plane = NodePlane(node);
	double e = extents.x * std::abs(plane.x) + extents.y * std::abs(plane.y) + extents.z * std::abs(plane.z);
	double s = center.x * plane.x + center.y * plane.y + center.z * plane.z - plane.w;
	if (s - e > 0)
		return -1;
	else if (s + e < 0)
		return 1;
	else
		return 0;
}
