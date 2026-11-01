
#include "Precomp.h"
#include "TraceTest.h"
#include "Collision/BottomLevel/TraceAABBModel.h"
#include "Collision/BottomLevel/OverlapAABBModel.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Engine.h"

CollisionHitList TraceTester::Trace(const vec3& from, const vec3& to, float height, float radius, bool traceActors, bool traceWorld, bool visibilityOnly)
{
	if (from == to || (!traceActors && !traceWorld))
		return {};
	if (radius == 0.0f && height == 0.0f)
		return TraceLine(from, to, traceActors, traceWorld, visibilityOnly);

	dvec3 origin = to_dvec3(from);
	dvec3 direction = to_dvec3(to) - origin;
	double tmin = 0.0f; // if this goes above 0.0, you will be able to walk through walls!
	double tmax = length(direction);
	if (tmax < tmin)
		return {};
	direction *= 1.0f / tmax;

	// Deus Ex's hits are given short of what they hit by the original's
	// backoffs (below).

	CollisionHitList hits;

	double dradius = radius;
	double dheight = height;
	vec3 extents = { radius, radius, height };

	// A box asks the cells of its sweep
	auto traceActor = [&](UActor* actor) {
		TraceActor(actor, origin, tmin, direction, tmax, dheight, dradius, traceActors, traceWorld, visibilityOnly, hits);
		return false;
	};
	VisitActorsInBox(GetSweepStartExtents(from, to, extents), GetSweepEndExtents(from, to, extents), traceActor);

	if (traceWorld)
	{
		// AABB/Triangle intersect
		TraceAABBModel tracemodel;
		dvec3 extents = { (double)radius, (double)radius, (double)height };
		CollisionHitList worldHits = tracemodel.Trace(GetLevel()->Model, origin, tmin, direction, tmax, extents, true, false);
		hits.push_back(worldHits);
	}

	// Deus Ex's hits come out as the original's MultiLineCheck hands them on
	// (dx-reverse-info engine-dll.md, traces), backed off as UModel::LineCheck
	// and UPrimitive::LineCheck back them off: a hit on the level or a mover's
	// brush a tenth of the trace short (a tenth of a unit for a trace under
	// one long), a hit on an actor's cylinder a thousandth of the cut line
	// short, held to the line; one still at the end or past it is none. The
	// level's hit is one, the nearest of its own; the actors are asked along
	// the line cut to 5 units past it, so their hits beyond that are none;
	// with the level met within a hundredth of the line and 30 units, every
	// actor's hit but a mover's is dropped. A line's are TraceLine's.
	double brushBackoff = std::max(0.1, 0.1 * tmax);

	// The level's one hit, the nearest, and the line cut to 5 units past it
	int levelIndex = -1;
	double levelDist = 0.0;
	for (size_t i = 0; i < hits.size(); i++)
	{
		const CollisionHit& hit = hits.begin()[i];
		if (!hit.Actor && (levelIndex < 0 || hit.Fraction < hits.begin()[levelIndex].Fraction))
			levelIndex = (int)i;
	}
	double cutLen = tmax;
	bool nearStart = false;
	if (levelIndex >= 0)
	{
		const CollisionHit& level = hits.begin()[levelIndex];
		levelDist = std::max((double)level.Fraction - brushBackoff, 0.0);
		cutLen = std::min(tmax, levelDist * (levelDist + 5.0) / (levelDist + 0.0001));
		nearStart = levelDist < 0.01 * tmax && levelDist < 30.0;
	}
	// The actors' checks run along the cut line: a cylinder's thousandth is of it
	double actorBackoff = 0.001 * cutLen;

	CollisionHitList given;
	for (size_t i = 0; i < hits.size(); i++)
	{
		CollisionHit hit = hits.begin()[i];
		double dist;
		if (!hit.Actor)
		{
			if ((int)i != levelIndex)
				continue;
			dist = levelDist;
		}
		else
		{
			UMover* mover = UObject::TryCast<UMover>(hit.Actor);
			if (nearStart && !mover)
				continue;
			if (hit.Node)
			{
				// A brush: its hit backed off, none at the cut line's end or
				// past it but one at its start, in the brush's solid: the
				// original's check along a line the level cut to nothing,
				// its start in solid, still meets that brush (a door's leaf
				// traced from inside it, the door filed in the BSP too:
				// DoorConsole)
				dist = std::max((double)hit.Fraction - brushBackoff, 0.0);
				if (dist >= cutLen && hit.Fraction > 0.0f)
					continue;
			}
			else
			{
				// A cylinder: a hit within the cut line, a thousandth short
				if (hit.Fraction > cutLen)
					continue;
				dist = std::max((double)hit.Fraction - actorBackoff, 0.0);
			}
		}
		if (dist >= tmax)
			continue;
		hit.Fraction = (float)(dist / tmax);
		given.push_back(hit);
	}
	return SortedFirstHits(given);
}

// Sorted by time, nearest first, equal times keeping the order they were
// given in; only the first hit for each actor
CollisionHitList TraceTester::SortedFirstHits(CollisionHitList& hits)
{
	hits.SortByFraction();
	CollisionHitList uniqueHits;
	for (auto& hit : hits)
	{
		// The hits kept are few: a scan of them, not a std::set allocated per trace
		bool seen = false;
		if (hit.Actor)
		{
			for (const CollisionHit& kept : uniqueHits)
			{
				if (kept.Actor == hit.Actor)
				{
					seen = true;
					break;
				}
			}
		}
		if (!seen)
			uniqueHits.push_back(hit);
	}

	return uniqueHits;
}

// UModel::LineCheck's time for a line's hit (Engine.dll 0x103f3c20), in its
// float order: the place the walk met projected on the line, rounded to a
// float, less half a unit of the line, held to 0..1; the hit's location
// given again along the line from that time unrounded. The x87's sums are
// doubles here (the original's control word keeps 53 bits), its stores
// floats. A line of no length (cut at a start in solid) gives 0 at its start,
// as the original's 0/0 compares below 0 there.
static float LineCheckTime(const vec3& start, const vec3& end, const vec3& place, vec3& location)
{
	float dx = (float)((double)end.x - start.x);
	float dy = (float)((double)end.y - start.y);
	float dz = (float)((double)end.z - start.z);
	double hx = (double)place.x - start.x;
	double hy = (double)place.y - start.y;
	double hz = (double)place.z - start.z;
	float size2 = (float)(((double)dx * dx + (double)dy * dy) + (double)dz * dz);
	double t = 0.0;
	if (size2 > 0.0f)
	{
		float time = (float)(((hx * dx + hy * dy) + hz * dz) / size2);
		t = std::clamp(time - 0.5 / std::sqrt((double)size2), 0.0, 1.0);
	}
	location = vec3(
		(float)(start.x + (double)(float)(dx * t)),
		(float)(start.y + (double)(float)(dy * t)),
		(float)(start.z + (double)(float)(dz * t)));
	return (float)t;
}

// MultiLineCheck's pull (Engine.dll 0x1039b55d): a mover's hit set 2 units
// before the level's along the cut line, its time on that line redone from
// there, in the original's float order
static float PullTime(const vec3& start, const vec3& cutEnd, const vec3& levelLocation, float time)
{
	float dz = (float)((double)cutEnd.z - start.z);
	float dy = (float)((double)cutEnd.y - start.y);
	double dxr = (double)cutEnd.x - start.x;
	float dx = (float)dxr;
	double len = std::sqrt((dxr * dx + (double)dy * dy) + (double)dz * dz);
	if (!(len > 0.0))
		return time;
	float lenf = (float)len;
	double inv = 1.0 / len;
	float uz = (float)(dz * inv);
	float uy = (float)(dy * inv);
	double ux = inv * dx;
	vec3 location(
		(float)((double)levelLocation.x - (double)(float)(ux + ux)),
		(float)((double)levelLocation.y - (double)(float)((double)uy + uy)),
		(float)((double)levelLocation.z - (double)(float)((double)uz + uz)));
	float vx = (float)((double)location.x - start.x);
	float vy = (float)((double)location.y - start.y);
	float vz = (float)((double)location.z - start.z);
	return (float)(std::sqrt(((double)vz * vz + (double)vy * vy) + (double)vx * vx) / lenf);
}

// Deus Ex's line meets a mover's brush as the original's walk down its BSP
// does (UModel::LineCheck with an owner): one hit, where the segment enters
// its solid space, with the plane of the node crossed there as the mover's
// rotation turns it -- by the original's sine table, and not turned to face
// the line, as the level's hits are. Gives how far along the segment (0 to 1)
// and the node.
// The brush walked in the world along the line, its planes turned into it as
// the original's walk turns its owner's (UModel::LineCheck 0x103f3c20 with an
// owner; BrushPlanes): the hit where the line enters its solid, the crossed
// node's plane the normal, not turned to face the line
static bool MoverLineWalk(UMover* mover, const vec3& start, const vec3& end, bool visibilityOnly, double& fraction, BspNode*& crossed, vec3& normal)
{
	UModel* brush = mover->Brush();
	if (!brush || brush->Nodes.empty())
		return false;
	BrushPlanes planes(mover->Location(), mover->Rotation(), mover->MainScale().Scale, mover->PrePivot());
	Array<BspNode> world = brush->Nodes;
	for (BspNode& node : world)
	{
		vec4 plane = planes.Plane(vec4(node.PlaneX, node.PlaneY, node.PlaneZ, node.PlaneW));
		node.PlaneX = plane.x;
		node.PlaneY = plane.y;
		node.PlaneZ = plane.z;
		node.PlaneW = plane.w;
	}
	BspNode* hitNode;
	if (!TraceRayModel::WalkSolid(world, brush->RootOutside != 0, to_dvec3(start), to_dvec3(end), visibilityOnly ? NF_NotVisBlocking : 0, fraction, hitNode))
		return false;
	crossed = &brush->Nodes[hitNode - world.data()];
	normal = vec3(hitNode->PlaneX, hitNode->PlaneY, hitNode->PlaneZ);
	return true;
}

// Deus Ex's line, as the original's MultiLineCheck checks one with no extent
// (Engine.dll 0x1039b220; dx-reverse-info engine-dll.md, traces), in its
// float order. The level first: one hit, where the line enters solid space
// down its BSP with the movers filed into it -- at its start when that is in
// solid -- with the plane of the node crossed there, turned to face the
// start, and that node's surface; a polygon with empty space on both sides
// stops nothing; its time UModel::LineCheck's. The line is cut to 5 units
// past it, (its distance + 5) x its time / (its distance + 0.0001), the cut's
// end a float; with the level met within a hundredth of the line and 30 units
// every actor's hit but a mover's is dropped. Then the actors along the cut
// line: a mover's brush walked on it, its time UModel::LineCheck's on it, made
// the whole line's by the cut (cut x time, a float) -- a mover's hit within 2
// units of the level's, its normal the level hit's (a float compare), is set
// 2 units before the level's first --; a cylinder's hit a thousandth of the
// cut line short. The hits sorted by those times alone: a mover flush with a
// wall falls before or after it as the original's two times round (the
// wall's plane a float step off the door's, or the cut line's end rounded),
// equal times keeping the actors first, as the original's sort of two equal
// times (the CRT's qsort) swaps them.
CollisionHitList TraceTester::TraceLine(const vec3& from, const vec3& to, bool traceActors, bool traceWorld, bool visibilityOnly)
{
	dvec3 origin = to_dvec3(from);
	dvec3 rayEnd = to_dvec3(to);
	dvec3 direction = rayEnd - origin;
	double tmax = length(direction);
	direction *= 1.0 / tmax;

	CollisionHit level;
	bool levelHit = false;
	vec3 levelLocation(0.0f);
	float cut = 1.0f;
	vec3 cutEnd = to;
	bool nearStart = false;
	if (traceWorld)
	{
		double fraction;
		BspNode* crossed;
		CollisionSystem& collision = GetLevel()->Collision;
		if (TraceRayModel::WalkSolid(collision.LineNodes(), collision.LineRootOutside(), origin, rayEnd, visibilityOnly ? NF_NotVisBlocking : 0, fraction, crossed))
		{
			vec3 place = to_vec3(origin + (rayEnd - origin) * fraction);
			level = { LineCheckTime(from, to, place, levelLocation), vec3(crossed->PlaneX, crossed->PlaneY, crossed->PlaneZ), nullptr, crossed, crossed };
			if (dot(to_dvec3(level.Normal), direction) > 0.0)
				level.Normal = -level.Normal;
			levelHit = true;

			double ex = (double)levelLocation.x - from.x;
			double ey = (double)levelLocation.y - from.y;
			double ez = (double)levelLocation.z - from.z;
			double dist = std::sqrt((ex * ex + ey * ey) + ez * ez);
			double c = (dist + 5.0) * level.Fraction / (dist + (double)0.0001f);
			cut = c < 1.0 ? (float)c : 1.0f;
			float dx = (float)((double)to.x - from.x);
			float dy = (float)((double)to.y - from.y);
			cutEnd = vec3(
				(float)((double)(float)(dx * (double)cut) + from.x),
				(float)(from.y + (double)(float)(dy * (double)cut)),
				(float)(((double)to.z - from.z) * cut + from.z));
			nearStart = (double)level.Fraction < 0.01 && dist < 30.0;
		}
	}

	// The cells along the whole line, however long (a rifle's 48000 units);
	// each actor asked along the cut line
	double cutLen = cut * tmax;
	double actorBackoff = 0.001 * cutLen;
	CollisionHitList hits;
	VisitActorsOnRay(from, to, [&](UActor* actor) {
		UMover* mover = UObject::TryCast<UMover>(actor);
		if (nearStart && !mover)
			return false;
		if (mover && mover->Brush())
		{
			double fraction;
			BspNode* crossed;
			vec3 normal;
			if (!traceWorld || !MoverLineWalk(mover, from, cutEnd, visibilityOnly, fraction, crossed, normal))
				return false;
			vec3 place = to_vec3(origin + (to_dvec3(cutEnd) - origin) * fraction);
			vec3 location;
			float time = LineCheckTime(from, cutEnd, place, location);
			if (levelHit && normal == level.Normal)
			{
				double mx = (double)location.x - levelLocation.x;
				double my = (double)location.y - levelLocation.y;
				double mz = (double)location.z - levelLocation.z;
				if ((mx * mx + my * my) + mz * mz < 4.0)
					time = PullTime(from, cutEnd, levelLocation, time);
			}
			time = (float)((double)cut * time);
			if (time < 1.0f)
				hits.push_back({ time, normal, actor, crossed, crossed });
		}
		else
		{
			// A cylinder: a hit within the cut line, a thousandth of it short
			CollisionHitList cylinder;
			TraceActor(actor, origin, 0.0, direction, tmax, 0.0, 0.0, traceActors, traceWorld, visibilityOnly, cylinder);
			for (CollisionHit hit : cylinder)
			{
				if (hit.Fraction > cutLen)
					continue;
				double dist = std::max((double)hit.Fraction - actorBackoff, 0.0);
				if (dist >= tmax)
					continue;
				hit.Fraction = (float)(dist / tmax);
				hits.push_back(hit);
			}
		}
		return false;
	});
	if (levelHit)
		hits.push_back(level);

	return SortedFirstHits(hits);
}

// Calls visit once for each actor in the collision cells the segment passes
// through, until it returns true; then so does this.
template<typename Visit>
bool TraceTester::VisitActorsOnRay(const vec3& from, const vec3& to, Visit&& visit)
{
	int checkCounter = NextCheckCounter();
	ivec3 start = GetRayStartExtents(from, to);
	ivec3 end = GetRayEndExtents(from, to);

	// Only the cells the segment passes through, not every cell of its
	// box: a long sight line's box is mostly cells it never enters, and an
	// actor is in every cell its collision box touches, so these are all
	// the actors it can hit. Slab by slab along the segment's longest
	// axis, each slab's cells on the other two axes found from where the
	// segment enters and leaves it, a unit wider each way.
	vec3 delta = to - from;
	vec3 absDelta = { std::abs(delta.x), std::abs(delta.y), std::abs(delta.z) };
	int axis = absDelta.x >= absDelta.y ? (absDelta.x >= absDelta.z ? 0 : 2) : (absDelta.y >= absDelta.z ? 1 : 2);
	for (int slab = start[axis]; slab < end[axis]; slab++)
	{
		float tA = (slab * 256.0f - from[axis]) / delta[axis];
		float tB = ((slab + 1) * 256.0f - from[axis]) / delta[axis];
		float t0 = std::clamp(std::min(tA, tB), 0.0f, 1.0f);
		float t1 = std::clamp(std::max(tA, tB), 0.0f, 1.0f);
		ivec3 cellStart = start, cellEnd = end;
		cellStart[axis] = slab;
		cellEnd[axis] = slab + 1;
		for (int a = 0; a < 3; a++)
		{
			if (a == axis)
				continue;
			float c0 = from[a] + delta[a] * t0;
			float c1 = from[a] + delta[a] * t1;
			cellStart[a] = std::max((int)std::floor((std::min(c0, c1) - 1.0f) * (1.0f / 256.0f)), start[a]);
			cellEnd[a] = std::min((int)std::floor((std::max(c0, c1) + 1.0f) * (1.0f / 256.0f)) + 1, end[a]);
		}

		for (int z = cellStart.z; z < cellEnd.z; z++)
		{
			for (int y = cellStart.y; y < cellEnd.y; y++)
			{
				for (int x = cellStart.x; x < cellEnd.x; x++)
				{
					for (UActor* actor : GetActors(x, y, z))
					{
						if (actor->Collision.CheckCounter != checkCounter)
						{
							actor->Collision.CheckCounter = checkCounter;
							if (visit(actor))
								return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool TraceTester::TraceAnyHit(vec3 from, vec3 to, UActor* tracingActor, bool traceActors, bool traceWorld, bool visibilityOnly)
{
	if (from == to || (!traceActors && !traceWorld))
		return false;

	dvec3 origin = to_dvec3(from);
	dvec3 direction = to_dvec3(to) - origin;
	double tmin = 0.01f;
	double tmax = length(direction);
	if (tmax < tmin)
		return false;
	direction *= 1.0f / tmax;

	// Deus Ex's asks along the line itself, as the original's FastLineCheck
	// does.

	// Deus Ex's movers block where the level's BSP filed them, whatever they
	// collide with (below), not where they are; the other actors only when
	// asked for
	if (traceActors)
	{
		CollisionHitList hits;
		bool actorHit = VisitActorsOnRay(from, to, [&](UActor* actor) {
			if (actor != tracingActor && actor->bBlockActors() && !UObject::TryCast<UMover>(actor))
			{
				TraceActor(actor, origin, tmin, direction, tmax, 0.0, 0.0, traceActors, traceWorld, visibilityOnly, hits);
				if (!hits.empty())
					return true;
			}
			return false;
		});
		if (actorHit)
			return true;
	}

	if (traceWorld)
	{
		// Deus Ex's is the original's FastLineCheck: its walk down the
		// level's BSP, the movers filed into it, blocked by any solid leaf
		// the segment touches, the start's included -- behind a mover's
		// invisible and portal polygons too, as the walk is stopped by a
		// node that does not block visibility (engine-dll.md, FastTrace),
		// so a mesh light's shadow line passes neither
		CollisionSystem& collision = GetLevel()->Collision;
		return !TraceRayModel::FastWalk(collision.LineNodes(), collision.LineRootOutside(), to_dvec3(from), to_dvec3(to));
	}

	return false;
}

// As UE1's MultiLineCheck with the NF_NotVisBlocking node flags, as far as a
// yes or no: world and mover surfaces that block visibility, and each actor
// crossed that blocksSight says blocks, whatever it collides with. The end is
// often a point on a surface (the foot of a pawn standing on it), which a
// segment ending there does not cross: no margin past it.
bool TraceTester::SightBlocked(const vec3& from, const vec3& to, const std::function<bool(UActor* actor)>& blocksSight)
{
	if (from == to)
		return false;

	dvec3 origin = to_dvec3(from);
	dvec3 direction = to_dvec3(to) - origin;
	double tmin = 0.01;
	double tmax = length(direction);
	if (tmax < tmin)
		return false;
	direction *= 1.0 / tmax;

	CollisionHitList hits;
	bool actorBlocks = VisitActorsOnRay(from, to, [&](UActor* actor) {
		hits.clear();
		TraceActor(actor, origin, tmin, direction, tmax, 0.0, 0.0, true, true, true, hits);
		return !hits.empty() && blocksSight(actor);
	});
	if (actorBlocks)
		return true;

	// Deus Ex's BSP holds the movers where it filed them, the original's
	// walk down it with the NF_NotVisBlocking nodes passed
	double fraction;
	BspNode* crossed;
	CollisionSystem& collision = GetLevel()->Collision;
	return TraceRayModel::WalkSolid(collision.LineNodes(), collision.LineRootOutside(), to_dvec3(from), to_dvec3(to), NF_NotVisBlocking, fraction, crossed);
}

// The hit lies on the cylinder the trace actually swept against: the actor's cylinder grown by the
// moving box's own extents. Center-to-hitpoint is the true surface normal only on the curved side - on
// the flat end caps it tilts outwards by however far the contact sits from the axis. That is how the
// flat top of a BlockAll came to report a normal of z=0.69, just under the 0.7071 walkable threshold,
// flipping a pawn standing on it between walking and falling every single frame.
//
// Decided by least penetration rather than by testing the hit point against the cap plane, because a
// sweep that begins in contact reports fraction 0 and a "hit point" that is simply where it started -
// which is exactly the case that matters here, a pawn resting on the surface. Least penetration gives
// the right answer for that and for a clean hit alike: on a true side contact the radial depth is zero,
// on a true cap contact the vertical depth is.
static vec3 CylinderHitNormal(const dvec3& hitpos, UActor* actor, double boxHeight, double boxRadius)
{
	dvec3 center = to_dvec3(actor->Location());
	double halfHeight = actor->CollisionHeight() + boxHeight;
	double radius = actor->CollisionRadius() + boxRadius;

	double dz = hitpos.z - center.z;
	dvec3 radial(hitpos.x - center.x, hitpos.y - center.y, 0.0);
	double radialDist = std::sqrt(dot(radial, radial));

	double verticalDepth = halfHeight - std::abs(dz);
	double radialDepth = radius - radialDist;

	if (verticalDepth <= radialDepth || radialDist < 0.000001)
		return vec3(0.0f, 0.0f, dz >= 0.0 ? 1.0f : -1.0f);

	radial /= radialDist;
	return vec3((float)radial.x, (float)radial.y, 0.0f);
}

void TraceTester::TraceActor(UActor* actor, const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, double height, double radius, bool traceActors, bool traceWorld, bool visibilityOnly, CollisionHitList& hits)
{
	// A mover with no brush (09_NYC_ShipBelow has one) collides as its
	// cylinder: the original's primitive is the brush, else the mesh, else
	// the engine's cylinder.
	UMover* mover = UObject::TryCast<UMover>(actor);
	if (mover && mover->Brush())
	{
		if (!traceWorld)
			return;

		CollisionHitList brushHits;

		if (radius == 0.0 && height == 0.0)
		{
			// A line (the AI's sight line): MoverLineWalk's hit, its distance
			double fraction;
			BspNode* crossed;
			vec3 normal;
			if (MoverLineWalk(mover, vec3(origin), vec3(origin + dirNormalized * tmax), visibilityOnly, fraction, crossed, normal))
				hits.push_back({ (float)(fraction * tmax), normal, actor, crossed, crossed });
			return;
		}
		else
		{
			// A box meets the brush in the world's axes, as the original's box
			// walk turns the brush's planes into the world by its owner's
			// coordinates (Engine.dll 0x103f42f0): taken into the brush's own
			// axes, the box would turn with it, a grate pitched on its side
			// meeting a box as deep as the cylinder is tall
			TraceAABBModel tracemodel;
			mat4 objectToWorld = mat4::translate(mover->Location()) * Coords::Rotation(mover->Rotation()).ToMatrix() * mat4::scale(mover->MainScale().Scale) * mat4::translate(-mover->PrePivot());
			dvec3 extents = { (double)radius, (double)radius, (double)height };
			brushHits = tracemodel.Trace(mover->Brush(), origin, tmin, dirNormalized, tmax, extents, true, true, &objectToWorld);
		}

		for (auto& hit : brushHits)
		{
			hit.Actor = actor;
			hits.push_back(hit);
		}
	}
	else
	{
		if (!traceActors)
			return;

		double t = CylinderActorTrace(origin, tmin, dirNormalized, tmax, height, radius, actor);
		if (t < tmax)
		{
			dvec3 hitpos = origin + dirNormalized * t;
			vec3 normal = CylinderHitNormal(hitpos, actor, height, radius);

			// A line or a box stopped at its start inside the cylinder takes
			// the original's normal there: out from the axis, level
			// (UPrimitive::LineCheck; TraceConsole).
			dvec3 offset(origin.x - actor->Location().x, origin.y - actor->Location().y, 0.0);
			if (t == 0.0 && dot(offset, offset) > 0.0)
			{
				offset /= std::sqrt(dot(offset, offset));
				normal = vec3((float)offset.x, (float)offset.y, 0.0f);
			}
			hits.push_back({ (float)t, normal, actor, nullptr, nullptr });
		}
	}
}

double TraceTester::CylinderActorTrace(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, double cylinderHeight, double cylinderRadius, UActor* actor)
{
	if (actor->Brush()) // Ignore brushes for now
		return tmax;

	dvec3 center = to_dvec3(actor->Location());
	double height = actor->CollisionHeight();
	double radius = actor->CollisionRadius();

	if (cylinderHeight > 0.0f && cylinderRadius > 0.0f)
	{
		return CylinderCylinderTrace(origin, dirNormalized, tmin, tmax, center, height, radius, cylinderHeight, cylinderRadius);
	}
	else
	{
		// Downgrade to a ray test if the cylinder is a point
		return RayCylinderTrace(origin, dirNormalized, tmin, tmax, center, height, radius);
	}
}

double TraceTester::CylinderCylinderTrace(const dvec3& origin, const dvec3& dirNormalized, double tmin, double tmax, const dvec3& cylinderCenterA, double cylinderHeightA, double cylinderRadiusA, double cylinderHeightB, double cylinderRadiusB)
{
	// A cylinder/cylinder trace is the same as a ray/cylinder trace, except the cylinder's radius and height has been extended by the other cylinder

	return RayCylinderTrace(origin, dirNormalized, tmin, tmax, cylinderCenterA, cylinderHeightA + cylinderHeightB, cylinderRadiusA + cylinderRadiusB);
}

#if 0 // Shock rifle combo blasts does not work with this version. Why? Does this mean that CylinderCylinderOverlap is broken as it is built upon the same idea?

double CollisionSystem::RayCylinderTrace(const dvec3& origin, const dvec3& dirNormalized, double tmin, double tmax, const dvec3& cylinderCenter, double cylinderHeight, double cylinderRadius)
{
	// Find the trace range where the ray can hit the cylinders (ray/planes test):
	double t0, t1;
	if (dirNormalized.z > FLT_EPSILON || dirNormalized.z < -FLT_EPSILON)
	{
		t0 = (cylinderCenter.z - origin.z - cylinderHeight) / dirNormalized.z;
		t1 = (cylinderCenter.z - origin.z + cylinderHeight) / dirNormalized.z;
		if (t1 < t0) std::swap(t0, t1);

		// If the trace range is outside the planes then there is no hit
		if (t1 < tmin || t0 >= tmax)
			return tmax;
	}
	else // trace is parallel to the plane - either we are always inside or we are always outside
	{
		if (std::abs(cylinderCenter.z - origin.z) >= cylinderHeight)
			return tmax;
		t0 = 0.0;
		t1 = tmax;
	}

	// Test if the first possible hit point is already overlapping. If it is, we hit the top or bottom of the cylinder.
	if (t0 >= tmin)
	{
		dvec2 dist2d = cylinderCenter.xy() + origin.xy() + dirNormalized.xy() * t0;
		if (dot(dist2d, dist2d) < cylinderRadius * cylinderRadius)
			return t0;
	}

	// Find the when the ray might hit the side in the XY plane (ray/circle test):
	dvec2 l = cylinderCenter.xy() - origin.xy();
	double s = dot(l, dirNormalized.xy());
	double l2 = dot(l, l);
	double r2 = cylinderRadius * cylinderRadius;
	if (s < 0 && l2 > r2)
		return tmax;
	double s2 = s * s;
	double m2 = l2 - s2;
	if (m2 > r2)
		return tmax;
	double q = std::sqrt(r2 - m2);
	double t = (l2 > r2) ? s - q : s + q;
	return (t >= tmin && t >= t0 && t <= t1) ? t : tmax;
}

#else

static int GetQuadraticRoots(double a, double b, double c, double& root_lower, double& root_upper)
{
	double discriminant = (b * b) - (4.0 * a * c);
	if (discriminant > FLT_EPSILON)
	{
		double b_term = b < FLT_EPSILON ? -b + sqrt(discriminant) : -b - sqrt(discriminant);

		root_lower = b_term / (2.0 * a); // quadratic formula
		root_upper = (2.0 * c) / b_term; // citardauq formula

		if (root_lower > root_upper)
			std::swap(root_lower, root_upper); // use of both formulae, plus this, avoids catastrophic cancellation due to floating-point limits

		return 2;
	}
	else if (discriminant > -FLT_EPSILON && discriminant <= FLT_EPSILON)
	{
		root_lower = -b / (2.0 * a); // quadratic formula's double root
		root_upper = root_lower;
		return 1;
	}
	root_lower = NAN;
	root_upper = NAN;
	return 0;
}

double TraceTester::RayCylinderTrace(const dvec3& rayOrigin, const dvec3& rayDirNormalized, double tmin, double tmax, const dvec3& cylinderCenter, double cylinderHeight, double cylinderRadius)
{
	// A line that starts inside the cylinder -- within its height, and its
	// radius but for a unit's slack -- is stopped at once if it heads in
	// toward the axis and leaves freely otherwise, as the original's
	// cylinder check (UPrimitive::LineCheck) counts only a line coming in:
	// a trace straight down from inside a pawn does not hit the pawn, where
	// this found where it left through the bottom.
	double dx = rayOrigin.x - cylinderCenter.x;
	double dy = rayOrigin.y - cylinderCenter.y;
	double dz = rayOrigin.z - cylinderCenter.z;
	if (dx * dx + dy * dy - cylinderRadius * cylinderRadius < 1.0 && dz > -cylinderHeight && dz < cylinderHeight)
		return (rayDirNormalized.x * dx + rayDirNormalized.y * dy) * tmax < -0.1 ? 0.0 : tmax;

	//
	// First, identify intersections between a line and an infinite cylinder. An infinite
	// cylinder has no base and extends in both directions.
	//
	dvec3 ct = cylinderCenter;
	dvec3 cb = cylinderCenter;

	ct.z += cylinderHeight;
	cb.z -= cylinderHeight;

	dvec3 rl = rayOrigin - cb;  // Ray origin local to centerpoint
	dvec3 cs = ct - cb;         // Cylinder spine
	double ch = length(cs);     // Cylinder height
	dvec3 ca = cs / ch;         // Cylinder axis

	auto caDotRd = dot(ca, rayDirNormalized);
	auto caDotRl = dot(ca, rl);
	auto rlDotRl = dot(rl, rl);

	double a = 1 - (caDotRd * caDotRd);
	double b = 2 * (dot(rayDirNormalized, rl) - caDotRd * caDotRl);
	double c = rlDotRl - caDotRl * caDotRl - (cylinderRadius * cylinderRadius);

	if (std::abs(a) <= FLT_EPSILON)
	{
		// Ray direction is parallel to the cylinder's axis (a == 0 exactly, e.g. a
		// vertical step-up/step-down trace against a Z-axis-aligned actor): it can never
		// cross the curved side, only the end caps, and only if it's already within the
		// radius (c <= 0). Handled directly here since the quadratic degenerates and
		// carries no usable root in this case.
		if (c > FLT_EPSILON)
			return tmax;

		double dTop, dBottom;
		bool hitTop = RayCircleTrace(rayOrigin, rayDirNormalized, ct, ca, cylinderRadius, dTop);
		bool hitBottom = RayCircleTrace(rayOrigin, rayDirNormalized, cb, -ca, cylinderRadius, dBottom);
		if (hitTop && hitBottom)
			return std::min(dTop, dBottom);
		else if (hitTop)
			return dTop;
		else if (hitBottom)
			return dBottom;
		else
			return tmax;
	}

	double t0;
	double t1;
	int numRoots = GetQuadraticRoots(a, b, c, t0, t1);
	if (numRoots == 0)
	{
		//
		// There is no intersection between a line (i.e. a "double-sided" ray) and the 
		// infinite cylinder that matches our finite cylinder. This means that we cannot 
		// be hitting any part of the cylinder: if we were hitting the base from the 
		// inside, for example, then the "back of our ray" would be hitting the upper 
		// part of the infinite cylinder.
		//
		return tmax;
	}

	bool valid1 = true;
	bool valid2 = true;
	//
	dvec3 hp1 = rayOrigin + rayDirNormalized * t0;
	dvec3 hp2 = rayOrigin + rayDirNormalized * t1;
	double ho1 = dot(ct - hp1, ca); // height offset
	double ho2 = dot(ct - hp2, ca);
	//
	int validRoots = numRoots;
	if (t0 < 0.0 || ho1 < DBL_EPSILON || ho1 > ch)
	{
		valid1 = false;
		--validRoots;
	}
	if (t1 < 0.0 || ho2 < DBL_EPSILON || ho2 > ch)
	{
		valid2 = false;
		if (numRoots > 1)
			--validRoots;
	}
	double t = tmax;
	if (validRoots == 0)
	{
		//
		// The ray never hits the bounded cylinder's curved surface. If we're looking 
		// along the cylinder's axis -- whether from inside or outside -- then the ray 
		// could still hit an endcap.
		// 
		// Let's project the ray origin onto the cylinder's axis, and figure out which
		// endcap we're nearer to. (Well, actually, we already have that value: it's
		// Ca_dot_Rl.)
		//
		// Rl is measured from cb, the *bottom* of the cylinder, so Ca_dot_Rl is how far the
		// ray origin sits above the bottom cap: <= 0 means it starts below the cylinder, and
		// >= ch means it starts above it. The only cap such a ray can enter through is the
		// one nearest it, so trace that one and not the far one.
		//
		if (caDotRl <= 0.0)
		{
			// below
			valid1 = RayCircleTrace(rayOrigin, rayDirNormalized, cb, -ca, cylinderRadius, t0);
		}
		else if (caDotRl >= ch)
		{
			// above
			valid2 = RayCircleTrace(rayOrigin, rayDirNormalized, ct, ca, cylinderRadius, t1);
		}

		if (valid1)
			return t0;
		else if (valid2)
			return t1;
		else
			return tmax;
	}
	if (validRoots == 1)
	{
		//
		// The ray hits the cylinder's curved surface only once. This can only happen under 
		// two cases: the ray originates from inside the cylinder, and points outward; or 
		// the ray passes through the bounded cylinder once and then through an endcap.
		//
		if (valid2)
		{
			hp1 = hp2;
			ho1 = ho2;
			valid1 = true;
			t0 = t1;
		}

		double d0;
		double d1;
		bool disc1 = RayCircleTrace(rayOrigin, rayDirNormalized, ct, ca, cylinderRadius, d0);
		bool disc2 = RayCircleTrace(rayOrigin, rayDirNormalized, cb, -ca, cylinderRadius, d1);
		if (disc1)
		{
			if (disc2)
			{
				if (d1 < d0)
					d0 = d1;
			}
		}
		else if (disc2)
		{
			d0 = d1; // the bottom disc is the only one we hit, so that is the distance to compare
			disc1 = disc2;
		}

		if (disc1)
		{
			if (d0 < t0)
			{
				t = d0;
				return t;
			}
		}
		else
		{
			return tmax;
		}
	}
	t = std::min(t0, t1);
	return t;
}

#endif

double TraceTester::RayActorTrace(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, UActor* actor)
{
	if (actor->Brush()) // Ignore brushes for now
		return tmax;

	dvec3 center = to_dvec3(actor->Location());
	double height = actor->CollisionHeight();
	double radius = actor->CollisionRadius();
	return RayCylinderTrace(origin, dirNormalized, tmin, tmax, center, height, radius);
}

double TraceTester::RaySphereTrace(const dvec3& rayOrigin, double tmin, const dvec3& rayDirNormalized, double tmax, const dvec3& sphereCenter, double sphereRadius)
{
	dvec3 l = sphereCenter - rayOrigin;
	double s = dot(l, rayDirNormalized);
	double l2 = dot(l, l);
	double r2 = sphereRadius * sphereRadius;
	if (s < 0 && l2 > r2)
		return tmax;
	double s2 = s * s;
	double m2 = l2 - s2;
	if (m2 > r2)
		return tmax;
	double q = std::sqrt(r2 - m2);
	double t = (l2 > r2) ? s - q : s + q;
	return (t >= tmin) ? t : tmax;
}

// Compute the intersection of a ray and a plane with infinite bounds. The ray direction 
// must be normalized. Returns the hit distance, from the ray's origin; to get the hit 
// position, multiply that by the ray's direction and then add the ray's origin.
bool TraceTester::RayPlaneTrace(const dvec3& rayOrigin, const dvec3& rayDirNormalized, const dvec3& planeOrigin, const dvec3& planeNormal, double& t)
{
	double denom = dot(planeNormal, rayDirNormalized);
	//
	// Non-zero check (accounting for floating-point imprecision):
	//
	if (denom > DBL_EPSILON || denom < -DBL_EPSILON)
	{
		double hd = dot(planeOrigin - rayOrigin, planeNormal) / denom;
		if (hd >= 0)
		{
			t = hd;
			return true;
		}
	}
	return false;
}

// Compute the intersection of a ray and a disc. The ray direction must be normalized. 
// Returns the hit distance, from the ray's origin.
bool TraceTester::RayCircleTrace(const dvec3& rayOrigin, const dvec3& rayDirNormalized, const dvec3& circleCenter, const dvec3& circleNormal, double radius, double& t)
{
	double hd;
	bool plane = RayPlaneTrace(rayOrigin, rayDirNormalized, circleCenter, circleNormal, hd);
	if (!plane)
		return false;
	dvec3 hp = rayOrigin + rayDirNormalized * hd;
	dvec3 dd = hp - circleCenter;
	if (dot(dd, dd) > radius * radius)
		return false;
	//if (Hd < 0) // redundant with checks done in ray/plane
	//   return false;
	t = hd;
	return true;
}
