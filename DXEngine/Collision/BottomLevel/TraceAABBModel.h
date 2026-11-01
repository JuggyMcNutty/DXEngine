#pragma once

#include "Packages/Engine/Resources/Level/ULevel.h"

class TraceAABBModel
{
public:
	// With original, the box check as Deus Ex's UModel::LineCheck makes it,
	// for the backoff its caller then takes off: each hit where the box
	// touches, a hull entered up to twice the trace's length counted, and
	// the level's hulls (not an actorBrush's) bounded by their boxes as the
	// original's are. Without, the hits a tenth of a unit short, within the
	// trace, every hull's box shaved.
	// With objectToWorld, the model is an actor's brush placed in the world
	// by it: each plane is turned into the world, as the original's walk
	// turns its owner's by the owner's coordinates (Engine.dll 0x103f42f0),
	// and the box, the trace and the hits' normals are the world's.
	CollisionHitList Trace(UModel* model, const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, const dvec3& extents, bool original, bool actorBrush, const mat4* objectToWorld = nullptr);

private:
	dvec4 NodePlane(const BspNode* node) const;
	vec4 WalkPlane(const BspNode* node) const;
	void Walk(int iParent, int iNode, bool outside, CollisionHitList& hits);
	void TraceHull(BspNode* node, CollisionHitList& hits);

	// The segment of the trace a hull's planes, each pushed out by the box,
	// leave: the times, of the whole trace, the box enters and leaves. Its
	// sums and products are floats', in the original's order (0x103f42f0's
	// own for the hull's planes and box faces, 0x103f5b80 for the bevels):
	// the original runs the x87 at 24-bit precision (NObject.cpp), so a box
	// whose middle lies on a plane is on it, where doubles can put it a
	// hundred-trillionth behind, the plane's normal carrying a half turn's
	// cosine (1.2e-16) in another axis -- the player's box at the grates
	// under Castle Clinton, its middle on a wall's face (GrateConsole).
	struct SweepCursor
	{
		SweepCursor(const vec3& start, const vec3& end, const vec3& extents, float tend) : start(start), end(end), tend(tend), extents(extents)
		{
		}

		// A plane the box heads into raises the entry, one it heads out of
		// lowers the exit, and one it runs along with both ends' boxes
		// wholly in front of it misses the hull. A box whose middle lies on
		// or in front of the plane while the box reaches behind it, heading
		// in, enters at once (time 0); one whose middle is behind it entered
		// when its leading face crossed it, back along the trace, which
		// counts only when less than the trace's length back (the entry
		// starts at -1).
		bool ClipPlane(const vec4& plane)
		{
			float aabbpushout = (std::abs(extents.x * plane.x) + std::abs(extents.z * plane.z)) + std::abs(extents.y * plane.y);
			float dist0 = ((start.z * plane.z + start.y * plane.y) + start.x * plane.x) - plane.w;
			float dist1 = ((end.z * plane.z + end.y * plane.y) + end.x * plane.x) - plane.w;

			float aabbdist = dist0 - aabbpushout;
			if (dist0 > dist1 && aabbdist >= -aabbpushout && aabbdist < 0.0f)
				aabbdist = 0.0f;

			// Along the plane within a hundred-thousandth, doubles in its
			// constants (Engine.dll 0x103f48c4)
			float planeFrontFace = dist0 - dist1;
			float t = aabbdist / planeFrontFace;

			if ((double)planeFrontFace >= -0.00001)
			{
				if ((double)planeFrontFace <= 0.00001)
				{
					if (dist0 > aabbpushout && dist1 > aabbpushout) // Line segment never hits our plane
					{
						nohit = true;
						return false;
					}
				}
				else if (t > tstart) // Plane normal is pointing towards our movement direction
				{
					tstart = t;
					hitnormal = plane.xyz();
				}
			}
			else if (t < tend) // Plane normal is pointing away from our movement direction
			{
				tend = t;
			}

			if (tstart < tend)
			{
				return true;
			}
			else
			{
				nohit = true;
				return false;
			}
		}

		// A bevel where the two planes' normals point opposite ways along the
		// axis while they turn the same way round it (their crosses with the
		// axis dotted over a thousandth, a double: Engine.dll 0x103f5273),
		// through the line where they meet (FIntersectPlanes2, 0x103f6600),
		// along the axis and facing as the first plane does. Planes too
		// nearly parallel to meet make none here.
		bool ClipBevel(const vec4& plane0, const vec4& plane1, int axis)
		{
			float turn = axis == 0 ? plane0.y * plane1.y + plane0.z * plane1.z : axis == 1 ? plane0.x * plane1.x + plane0.z * plane1.z : plane0.x * plane1.x + plane0.y * plane1.y;
			if (!((double)turn > 0.001))
				return true;

			vec3 linedir(plane0.y * plane1.z - plane0.z * plane1.y, plane0.z * plane1.x - plane1.z * plane0.x, plane0.x * plane1.y - plane0.y * plane1.x);
			float length2 = (linedir.x * linedir.x + linedir.y * linedir.y) + linedir.z * linedir.z;
			if ((double)length2 < 1e-6)
				return true;
			float inv = 1.0f / length2;
			vec3 point(
				(linedir.z * plane1.y - linedir.y * plane1.z) * plane0.w + (linedir.y * plane0.z - linedir.z * plane0.y) * plane1.w,
				(linedir.x * plane1.z - linedir.z * plane1.x) * plane0.w + (linedir.z * plane0.x - linedir.x * plane0.z) * plane1.w,
				(plane0.y * linedir.x - linedir.y * plane0.x) * plane1.w + (linedir.y * plane1.x - linedir.x * plane1.y) * plane0.w);
			point = point * inv;
			linedir = linedir * (1.0f / std::sqrt(length2));

			vec3 normal = axis == 0 ? vec3(0.0f, -linedir.z, linedir.y) : axis == 1 ? vec3(linedir.z, 0.0f, -linedir.x) : vec3(-linedir.y, linedir.x, 0.0f);
			float nn = (normal.x * normal.x + normal.y * normal.y) + normal.z * normal.z;
			if (!(nn > 0.0f))
				return true;
			normal = normal * (1.0f / std::sqrt(nn));
			if ((normal.z * plane0.z + normal.y * plane0.y) + normal.x * plane0.x < 0.0f)
				normal = -normal;

			return ClipPlane(vec4(normal, (point.y * normal.y + point.z * normal.z) + point.x * normal.x));
		}

		bool ClipBoxPlanes(const BBox& box)
		{
			// Treat the sides of each AABB as a plane
			vec4 boxPlanes[] =
			{
				{ 1.0f,  0.0f,  0.0f,  box.max.x},
				{-1.0f,  0.0f,  0.0f, -box.min.x},
				{ 0.0f,  1.0f,  0.0f,  box.max.y},
				{ 0.0f, -1.0f,  0.0f, -box.min.y},
				{ 0.0f,  0.0f,  1.0f,  box.max.z},
				{ 0.0f,  0.0f, -1.0f, -box.min.z}
			};

			for (int i = 0; i < 6; i++)
			{
				if (!ClipPlane(boxPlanes[i]))
					return false;
			}

			return true;
		}

		// The original's bounds of a level hull (FBoxCheck; dx-reverse-info
		// engine-dll.md, traces): its box a tenth of a unit bigger at both
		// ends in Z and toward -X and -Y, and as much smaller at +X and +Y.
		bool ClipOriginalBoxPlanes(const BBox& box)
		{
			vec4 boxPlanes[] =
			{
				{ 0.0f,  0.0f, -1.0f, (float)(0.1 - box.min.z)},
				{ 0.0f,  0.0f,  1.0f, (float)(box.max.z + 0.1)},
				{-1.0f,  0.0f,  0.0f, (float)(0.1 - box.min.x)},
				{ 1.0f,  0.0f,  0.0f, (float)(box.max.x - 0.1)},
				{ 0.0f, -1.0f,  0.0f, (float)(0.1 - box.min.y)},
				{ 0.0f,  1.0f,  0.0f, (float)(box.max.y - 0.1)}
			};

			for (int i = 0; i < 6; i++)
			{
				if (!ClipPlane(boxPlanes[i]))
					return false;
			}

			return true;
		}

		// Whether the box enters the hull, and when, of the whole trace: less
		// than nought if it starts inside, up to the exit the cursor began
		// with (twice the trace, or an earlier hull's hit).
		bool Hit(float& t) const
		{
			if (nohit || tstart <= -1.0f || tstart >= tend || tend <= 0.0f)
				return false;
			t = tstart;
			return true;
		}

		vec3 HitNormal() const
		{
			return hitnormal;
		}

	private:
		vec3 start;
		vec3 end;
		float tstart = -1.0f;
		float tend;
		vec3 extents;
		vec3 hitnormal = vec3(0.0f);
		bool nohit = false;
	};

	UModel* Model = nullptr;
	bool Original = false;
	bool ActorBrush = false;

	// The trace in floats, as the original's check is given it: its start,
	// its end, the box's extent, and its length and least distance
	vec3 Start = vec3(0.0f);
	vec3 End = vec3(0.0f);
	vec3 Extents = vec3(0.0f);
	double TMin = 0.0;
	double TMax = 0.0;

	// The time of the nearest hit so far, of the whole trace, as the
	// original's walk keeps it in its result: a later hull counts only when
	// it is entered sooner, and none once a hull has the box inside at the
	// start
	float HitTime = 2.0f;

	// The placement's turn of a plane: its normal by the inverse transpose
	// of the linear part (one column each for X, Y and Z), its distance
	// moved by the translation
	bool Placed = false;
	dvec3 PlaneAxes[3];
	dvec3 Translation;
};
