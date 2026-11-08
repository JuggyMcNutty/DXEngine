
#include "Precomp.h"
#include "TraceTest.h"
#include "Collision/BottomLevel/TraceAABBModel.h"
#include "Collision/BottomLevel/OverlapAABBModel.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Engine.h"
#include <limits>

CollisionHitList TraceTester::Trace(const vec3& from, const vec3& to, float height, float radius, bool traceActors, bool traceWorld, bool visibilityOnly)
{
	if (from == to || (!traceActors && !traceWorld))
		return {};
	if (radius == 0.0f && height == 0.0f)
		return TraceLine(from, to, traceActors, traceWorld, visibilityOnly);
	return TraceBox(from, to, vec3(radius, radius, height), traceActors, traceWorld, visibilityOnly);
}

// The multiple line check's list (ULevel::MultiLineCheck, Engine.dll
// 0x1039b220): the level's hit first, then the actors' as the hash's line
// check gives them, each find put at its list's head
// (FCollisionHash::ActorLineCheck, 0x10359160), so the last found first; 64
// in all at most; sorted by time as MSVCRT's qsort sorts them
// (CollisionHitList::SortAsMultiLineCheck); only the first hit for each
// actor, as the hash gives each once
CollisionHitList TraceTester::SortedFirstHits(const CollisionHit* level, CollisionHitList& found)
{
	CollisionHitList hits;
	if (level)
		hits.push_back(*level);
	for (size_t i = found.size(); i-- > 0 && hits.size() < 64;)
		hits.push_back(found.begin()[i]);
	hits.SortAsMultiLineCheck();
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

// UModel::LineCheck's time for a box's hit (Engine.dll 0x103f403b), in its
// float order: the walk's time of the trace less a tenth, or less 0.1 over the
// trace's length where that is more (a trace under one unit long backed off a
// tenth of a unit), held to 0..1, a float; 1 is no hit (0x103f40f3). The hit's
// location along the trace from that time, in floats. The trace is the one
// the box check was given: the whole line for the level, the cut line for an
// actor's brush.
static float BoxCheckTime(const vec3& start, const vec3& end, float entry, vec3& location)
{
	float dx = (float)((double)end.x - start.x);
	float dy = (float)((double)end.y - start.y);
	float dz = (float)((double)end.z - start.z);
	float size = (float)std::sqrt(((double)dx * dx + (double)dy * dy) + (double)dz * dz);
	double backoff = 0.1f / (double)size;
	if (!(0.1f < backoff))
		backoff = 0.1f;
	float time = (float)std::clamp((double)entry - backoff, 0.0, 1.0);
	location = vec3(start.x + dx * time, start.y + dy * time, start.z + dz * time);
	return time;
}

// MultiLineCheck's pull (Engine.dll 0x1039b55d): a mover's hit set 2 units
// before the level's along the cut line, that place its location
// (0x1039b63b), its time on that line redone from there, in the original's
// float order
static float PullTime(const vec3& start, const vec3& cutEnd, const vec3& levelLocation, float time, vec3& location)
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
	location = vec3(
		(float)((double)levelLocation.x - (double)(float)(ux + ux)),
		(float)((double)levelLocation.y - (double)(float)((double)uy + uy)),
		(float)((double)levelLocation.z - (double)(float)((double)uz + uz)));
	float vx = (float)((double)location.x - start.x);
	float vy = (float)((double)location.y - start.y);
	float vz = (float)((double)location.z - start.z);
	return (float)(std::sqrt(((double)vz * vz + (double)vy * vy) + (double)vx * vx) / lenf);
}

// MultiLineCheck's cut (Engine.dll 0x1039b308 to 0x1039b3fc), a line's and a
// box's alike, in its float order: the line cut to 5 units past the level's
// hit, (its distance + 5) x its time / (its distance + 0.0001), 1 at most, a
// float (0x1039b35a); the cut's end a float (0x1039b386), the start itself for
// the level met at the start; and whether the level was met within a
// hundredth of the line and 30 units of its start, which drops every actor's
// hit but a mover's
static void CutLine(const vec3& from, const vec3& to, const vec3& levelLocation, float levelTime, float& cut, vec3& cutEnd, bool& nearStart)
{
	double ex = (double)levelLocation.x - from.x;
	double ey = (double)levelLocation.y - from.y;
	double ez = (double)levelLocation.z - from.z;
	double dist = std::sqrt((ex * ex + ey * ey) + ez * ez);
	double c = (dist + 5.0) * levelTime / (dist + (double)0.0001f);
	cut = c < 1.0 ? (float)c : 1.0f;
	float dx = (float)((double)to.x - from.x);
	float dy = (float)((double)to.y - from.y);
	cutEnd = vec3(
		(float)((double)(float)(dx * (double)cut) + from.x),
		(float)(from.y + (double)(float)(dy * (double)cut)),
		(float)(((double)to.z - from.z) * cut + from.z));
	nearStart = (double)levelTime < 0.01 && dist < 30.0;
}

// The original's cylinder check (UPrimitive::LineCheck, Engine.dll
// 0x103d8af0) of the line from start to end, the box's extent added to the
// actor's collision size, in its order and the x87's precision (X87). The
// ends are tested against the cylinder's bounds; the line is clipped to its
// caps, a cap it comes in through giving the normal, then to its side, from
// a quadratic whose terms are floats. A start inside -- within the caps, and
// the radius but for a unit's slack -- stops at once heading in toward the
// axis, its normal the start's level offset from the axis made a unit (Core's
// FVector::SafeNormal, 0x10144be0), and passes heading out. Gives the time, a
// thousandth of the line short of where the line comes in, held to 0..1, and
// that entry's own; the hit's location, the start moved along the line by
// the time; and the normal: a cap's, or the side's, level, out from the axis
// to where the line comes in, made a unit in floats.
static bool CylinderLineCheck(UActor* actor, const vec3& start, const vec3& end, const vec3& extent, float& time, float& entry, vec3& location, vec3& normal)
{
	auto R = X87::Round;
	vec3 loc = actor->Location();
	float netHeight = (float)R((double)extent.z + actor->CollisionHeight());
	float netY = (float)R((double)extent.y + actor->CollisionRadius());
	double netXSum = R((double)extent.x + actor->CollisionRadius());
	float netX = (float)netXSum;

	// Both ends past one of the cylinder's bounds
	double bound = R(netXSum + loc.x);
	if (start.x > bound && end.x > bound)
		return false;
	bound = R((double)loc.x - netX);
	if (start.x < bound && end.x < bound)
		return false;
	bound = R((double)netY + loc.y);
	if (start.y > bound && end.y > bound)
		return false;
	bound = R((double)loc.y - netY);
	if (start.y < bound && end.y < bound)
		return false;
	float top = (float)R((double)netHeight + loc.z);
	if (start.z > top && end.z > top)
		return false;
	float bottom = (float)R((double)loc.z - netHeight);
	if (start.z < bottom && end.z < bottom)
		return false;

	// The line clipped to the caps
	float t0 = 0.0f;
	float t1 = 1.0f;
	normal = vec3(0.0f);
	if (start.z > top && end.z < top)
	{
		double t = R(R((double)top - start.z) / R((double)end.z - start.z));
		if (t > 0.0)
		{
			t0 = (float)t;
			normal = vec3(0.0f, 0.0f, 1.0f);
		}
	}
	else if (start.z < top && end.z > top)
	{
		double t = R(R((double)top - start.z) / R((double)end.z - start.z));
		if (1.0 > t)
			t1 = (float)t;
	}
	if (start.z < bottom && end.z > bottom)
	{
		double t = R(R((double)bottom - start.z) / R((double)end.z - start.z));
		if (t > t0)
		{
			t0 = (float)t;
			normal = vec3(0.0f, 0.0f, -1.0f);
		}
	}
	else if (start.z > bottom && end.z < bottom)
	{
		double t = R(R((double)bottom - start.z) / R((double)end.z - start.z));
		if (t1 > t)
			t1 = (float)t;
	}
	if (!(t0 < t1))
		return false;

	// The side: the line's level run against the circle
	double kx = R((double)start.x - loc.x);
	double ky = R((double)start.y - loc.y);
	double runX = R((double)end.x - start.x);
	double runY = R((double)end.y - start.y);
	float vx = (float)runX;
	float vy = (float)runY;
	float a = (float)R(R(runY * runY) + R(runX * runX));
	double half = R(R(runY * ky) + R(runX * kx));
	float b = (float)R(half + half);
	float c = (float)R(R(R(ky * ky) + R(kx * kx)) - R((double)netX * netX));
	float discrim = (float)R(R((double)b * b) - R(R((double)c * a) * 4.0));

	if (c < 1.0 && start.z > bottom && start.z < top)
	{
		if (!(R(R((double)vx * kx) + R((double)vy * ky)) < -0.1))
			return false;
		float ox = (float)kx;
		float oy = (float)ky;
		double square = R(R((double)ox * ox) + R((double)oy * oy));
		normal = vec3(0.0f);
		if (!(square < 1e-8))
		{
			double scale = R(1.0 / R(std::sqrt(square)));
			normal = vec3((float)R(scale * ox), (float)R(scale * oy), 0.0f);
		}
		time = 0.0f;
		entry = 0.0f;
		location = start;
		return true;
	}

	if (discrim < 0.0f)
		return false;
	if (a < 1e-8)
	{
		// A line along the axis meets the caps alone
		if (c > 0.0f)
			return false;
	}
	else
	{
		double root = R(std::sqrt((double)discrim));
		double halfOverA = R(0.5 / a);
		double t = R(R(root - b) * halfOverA);
		if (t1 > t)
			t1 = (float)t;
		t = -R(R(root + b) * halfOverA);
		if (t > t0)
		{
			t0 = (float)t;
			float nx = (float)R((double)(float)R((double)(float)R(vx * t) + start.x) - loc.x);
			float ny = (float)R((double)(float)R((double)(float)R(vy * t) + start.y) - loc.y);
			double square = R(R((double)nx * nx) + R((double)ny * ny));
			normal = vec3(nx, ny, 0.0f);
			if (!(square < 1e-8))
			{
				double scale = R(1.0 / R(std::sqrt(square)));
				normal = vec3((float)R(scale * nx), (float)R(scale * ny), 0.0f);
			}
		}
		if (!(t0 < t1))
			return false;
	}

	double t = R(t0 - 0.001);
	if (t < 0.0)
		t = 0.0;
	else if (!(t < 1.0))
		t = 1.0;
	time = (float)t;
	entry = t0;
	float vz = (float)R((double)end.z - start.z);
	location = vec3(
		(float)R((double)(float)R(vx * t) + start.x),
		(float)R((double)(float)R(vy * t) + start.y),
		(float)R(R(vz * t) + start.z));
	return true;
}

// Deus Ex's line meets a mover's brush as the original's walk down its BSP
// does (UModel::LineCheck with an owner): one hit, where the segment enters
// its solid space, with the plane of the node crossed there as the mover's
// rotation turns it -- by the original's sine table, and not turned to face
// the line, as the level's hits are. Gives how far along the segment (0 to 1)
// and the node.
// The brush walked in the world along the line, its planes turned into it as
// the original's walk turns its owner's (UModel::LineCheck 0x103f3c20 with an
// owner; BrushPlanes, PostScale in them): the hit where the line enters its
// solid, the crossed node's own normal turned into the world as the
// original turns it after its walk (BrushPlanes::HitNormal), not turned to
// face the line
static bool MoverLineWalk(UMover* mover, const vec3& start, const vec3& end, bool visibilityOnly, double& fraction, BspNode*& crossed, vec3& normal)
{
	UModel* brush = mover->Brush();
	if (!brush || brush->Nodes.empty())
		return false;
	BrushPlanes planes(mover->Location(), mover->Rotation(), mover->MainScale().Scale, mover->PostScale().Scale, mover->PrePivot());
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
	normal = planes.HitNormal(vec3(crossed->PlaneX, crossed->PlaneY, crossed->PlaneZ));
	return true;
}

// Deus Ex's box meets a mover's brush as the original's box check meets an
// actor's (UModel::LineCheck 0x103f3c20 with an owner): the box swept from
// start to end along the world's axes, as the trace gives it, the brush's
// planes turned into the world by its owner's coordinates (0x103f42f0) --
// taken into the brush's own axes, the box would turn with it, a grate
// pitched on its side meeting a box as deep as the cylinder is tall. Gives
// the walk's time of that trace, the plane's normal and the node.
static bool MoverBoxCheck(UMover* mover, const vec3& start, const vec3& end, const vec3& extents, CollisionHit& hit)
{
	mat4 objectToWorld = mover->ToWorldMatrix(mover->Location(), mover->Rotation());
	TraceAABBModel tracemodel;
	CollisionHitList hits = tracemodel.Trace(mover->Brush(), start, end, extents, true, &objectToWorld);
	if (hits.empty())
		return false;
	hit = hits.front();
	return true;
}

// Hits in the order the original's hash finds their actors walking the
// cells from start (CollisionTester::SortAsFound), each actor's hit once
void TraceTester::OrderAsFound(CollisionHitList& hits, const ivec3& start)
{
	Array<UActor*> actors;
	for (const CollisionHit& hit : hits)
		actors.push_back(hit.Actor);
	SortAsFound(actors, start);
	CollisionHitList ordered;
	for (UActor* actor : actors)
	{
		for (const CollisionHit& hit : hits)
		{
			if (hit.Actor == actor)
			{
				ordered.push_back(hit);
				break;
			}
		}
	}
	hits = ordered;
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
// 2 units before the level's first --; a cylinder by the original's check
// on the cut line (CylinderLineCheck), a thousandth of it short, its time
// made the whole line's by the cut. Each hit keeps the place its check
// found, as the trace natives give it: the level's and a mover's
// UModel::LineCheck's, a mover's on the cut line, or the pulled one's
// (DXMP_Silo's BreakableGlass8 at 178.500015, the level's 180.500015 less 2,
// where the start moved along the whole line by the time lands on 178.5); a
// cylinder's its check's on the cut line. The hits sorted by those times
// alone: a mover flush with a wall falls before or after it as the
// original's two times round (the wall's plane a float step off the door's,
// or the cut line's end rounded), equal times keeping the actors first, as
// the original's sort of two equal times (the CRT's qsort) swaps them.
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
			float time = LineCheckTime(from, to, place, levelLocation);
			level = { time, vec3(crossed->PlaneX, crossed->PlaneY, crossed->PlaneZ), nullptr, crossed, crossed, levelLocation };
			if (dot(to_dvec3(level.Normal), direction) > 0.0)
				level.Normal = -level.Normal;
			levelHit = true;
			CutLine(from, to, levelLocation, level.Fraction, cut, cutEnd, nearStart);
		}
	}

	// The cells along the whole line, however long (a rifle's 48000 units);
	// each actor asked along the cut line
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
					time = PullTime(from, cutEnd, levelLocation, time, location);
			}
			time = (float)((double)cut * time);
			if (time < 1.0f)
				hits.push_back({ time, normal, actor, crossed, crossed, location });
		}
		else if (traceActors && !actor->Brush())
		{
			// A cylinder, the original's check along the cut line
			float time, entry;
			vec3 location, normal;
			if (CylinderLineCheck(actor, from, cutEnd, vec3(0.0f), time, entry, location, normal))
				hits.push_back({ cut * time, normal, actor, nullptr, nullptr, location });
		}
		return false;
	});
	// In the order the original's hash finds the actors in the cut line's
	// cells: the slab walk takes fewer cells, in another order
	ivec3 cells, cellsEnd;
	CollisionSystem::QueryCells(CollisionSystem::Least(from, cutEnd), CollisionSystem::Most(from, cutEnd), vec3(0.0f), cells, cellsEnd);
	OrderAsFound(hits, cells);
	return SortedFirstHits(levelHit ? &level : nullptr, hits);
}

// Deus Ex's box, as the original's MultiLineCheck checks one with an extent
// (Engine.dll 0x1039b220; dx-reverse-info engine-dll.md, traces), in its
// float order. The level first: one hit, the nearest hull of its own BSP the
// box enters (UModel::LineCheck's box check, which no filed mover stops),
// backed off a tenth of the line, its time and place UModel::LineCheck's; the
// line cut to 5 units past it as a line's is (CutLine). Then the actors in
// the cells along the cut line, each checked on the cut line itself, as the
// original's hash checks them (FCollisionHash::ActorLineCheck, 0x10359160):
// a mover's brush by the box check from the start to the cut's end, its hit
// backed off a tenth of the cut line, its time made the whole line's by the
// cut (cut x time, a float) -- a mover's hit within 2 units of the level's,
// its normal the level hit's, set 2 units before the level's first --; a
// cylinder by the original's check on the cut line (CylinderLineCheck), the
// box's extent added to its size, a thousandth of the cut line short; so a
// pawn's move meets another pawn or a decoration so. The level met at
// the start cuts the line to nothing, and a box swept no way enters no hull:
// no actor is met, the level's hit the first, where a line from inside a
// mover's solid still meets it (TraceLine). Each hit keeps the place its
// check found, a mover's on the cut line or the pulled one's (TraceLine;
// GrateConsole's DeusExMover35 at y -1943.540039, where the start moved along
// the whole line by the time lands a float step off). The hits sorted by
// their times alone, equal times keeping the actors first.
CollisionHitList TraceTester::TraceBox(const vec3& from, const vec3& to, const vec3& extents, bool traceActors, bool traceWorld, bool visibilityOnly)
{
	CollisionHit level;
	bool levelHit = false;
	vec3 levelLocation(0.0f);
	float cut = 1.0f;
	vec3 cutEnd = to;
	bool nearStart = false;
	if (traceWorld)
	{
		TraceAABBModel tracemodel;
		CollisionHitList worldHits = tracemodel.Trace(GetLevel()->Model, from, to, extents, false);
		if (!worldHits.empty())
		{
			const CollisionHit& hit = worldHits.front();
			float time = BoxCheckTime(from, to, hit.Fraction, levelLocation);
			if (time < 1.0f)
			{
				level = { time, hit.Normal, nullptr, hit.Node, hit.NodeHead, levelLocation };
				levelHit = true;
				CutLine(from, to, levelLocation, time, cut, cutEnd, nearStart);
			}
		}
	}

	// The cells the box takes along the cut line; each actor asked along it
	CollisionHitList hits;
	VisitActorsInBox(GetSweepStartExtents(from, cutEnd, extents), GetSweepEndExtents(from, cutEnd, extents), [&](UActor* actor) {
		UMover* mover = UObject::TryCast<UMover>(actor);
		if (nearStart && !mover)
			return false;
		if (mover && mover->Brush())
		{
			CollisionHit hit;
			if (!traceWorld || !MoverBoxCheck(mover, from, cutEnd, extents, hit))
				return false;
			vec3 location;
			float time = BoxCheckTime(from, cutEnd, hit.Fraction, location);
			if (time >= 1.0f)
				return false;
			if (levelHit && hit.Normal == level.Normal)
			{
				double mx = (double)location.x - levelLocation.x;
				double my = (double)location.y - levelLocation.y;
				double mz = (double)location.z - levelLocation.z;
				if ((mx * mx + my * my) + mz * mz < 4.0)
					time = PullTime(from, cutEnd, levelLocation, time, location);
			}
			hit.Fraction = (float)((double)cut * time);
			hit.Location = location;
			hit.Actor = actor;
			hits.push_back(hit);
		}
		else if (traceActors && !actor->Brush())
		{
			// A cylinder, the original's check along the cut line
			float time, entry;
			vec3 location, normal;
			if (CylinderLineCheck(actor, from, cutEnd, extents, time, entry, location, normal))
				hits.push_back({ cut * time, normal, actor, nullptr, nullptr, location });
		}
		return false;
	});
	return SortedFirstHits(levelHit ? &level : nullptr, hits);
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
	// A cell's bounds 128 either side of its middle, a multiple of 256
	// (CollisionSystem::CellCoord); the first and last cells, which hold
	// everything beyond them, without bound
	auto cellLow = [&](int cell) {
		if (cell <= start[axis])
			return -std::numeric_limits<float>::infinity();
		if (cell >= end[axis])
			return std::numeric_limits<float>::infinity();
		return 256.0f * (cell - 128) - 128.0f;
	};
	for (int slab = start[axis]; slab < end[axis]; slab++)
	{
		float tA = (cellLow(slab) - from[axis]) / delta[axis];
		float tB = (cellLow(slab + 1) - from[axis]) / delta[axis];
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
			cellStart[a] = std::max(CollisionSystem::CellCoord(std::min(c0, c1) - 1.0f), start[a]);
			cellEnd[a] = std::min(CollisionSystem::CellCoord(std::max(c0, c1) + 1.0f) + 1, end[a]);
		}

		for (int x = cellStart.x; x < cellEnd.x; x++)
		{
			for (int y = cellStart.y; y < cellEnd.y; y++)
			{
				for (int z = cellStart.z; z < cellEnd.z; z++)
				{
					const Array<UActor*>& cell = GetActors(x, y, z);
					for (size_t i = cell.size(); i-- > 0;)
					{
						UActor* actor = cell[i];
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
	// Deus Ex's movers block where the level's BSP filed them, whatever they
	// collide with (below), not where they are; the other actors only when
	// asked for, along a line of a hundredth of a unit or more
	if (traceActors && from != to)
	{
		dvec3 origin = to_dvec3(from);
		dvec3 direction = to_dvec3(to) - origin;
		double tmin = 0.01f;
		double tmax = length(direction);
		if (tmax >= tmin)
		{
			direction *= 1.0f / tmax;
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
	}

	if (traceWorld)
	{
		// Deus Ex's is the original's FastLineCheck: its walk down the
		// level's BSP, the movers filed into it, blocked by any solid leaf
		// the segment touches, the start's included -- behind a mover's
		// invisible and portal polygons too, as the walk is stopped by a
		// node that does not block visibility (engine-dll.md, FastTrace),
		// so a mesh light's shadow line passes neither. Every line is
		// walked, one under a hundredth of a unit or of no length too, as
		// the original's is, so from inside solid it is blocked
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

void TraceTester::TraceActor(UActor* actor, const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, double height, double radius, bool traceActors, bool traceWorld, bool visibilityOnly, CollisionHitList& hits)
{
	// A mover with no brush (09_NYC_ShipBelow has one) collides as its
	// cylinder: the original's primitive is the brush, else the mesh, else
	// the engine's cylinder.
	UMover* mover = UObject::TryCast<UMover>(actor);
	if (mover && mover->Brush())
	{
		// A line (the AI's sight line): MoverLineWalk's hit, its distance. A
		// box meets a mover's brush in TraceBox alone, along the cut line.
		if (!traceWorld || radius != 0.0 || height != 0.0)
			return;
		double fraction;
		BspNode* crossed;
		vec3 normal;
		if (MoverLineWalk(mover, vec3(origin), vec3(origin + dirNormalized * tmax), visibilityOnly, fraction, crossed, normal))
			hits.push_back({ (float)(fraction * tmax), normal, actor, crossed, crossed });
	}
	else
	{
		// An actor that is no brush meets the line as the original's cylinder
		// (CylinderLineCheck) along the whole of it, the distance to where
		// the line comes in and the check's normal; a start inside heading in
		// stops at its start (TraceConsole). A brush that is no mover is
		// passed over.
		if (!traceActors || actor->Brush())
			return;

		float time, entry;
		vec3 location, normal;
		if (CylinderLineCheck(actor, vec3(origin), vec3(origin + dirNormalized * tmax), vec3((float)radius, (float)radius, (float)height), time, entry, location, normal))
			hits.push_back({ (float)(entry * tmax), normal, actor, nullptr, nullptr, location });
	}
}
