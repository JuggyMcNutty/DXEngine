
#include "Precomp.h"
#include "OverlapTest.h"
#include "Collision/BottomLevel/OverlapAABBModel.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Brush/UBrush.h"
#include "Packages/Engine/Resources/Level/UModel.h"

CollisionHitList OverlapTester::TestOverlap(const vec3& location, float height, float radius, bool testActors, bool testWorld, bool visibilityOnly)
{
	if (!testActors && !testWorld)
		return {};

	vec3 extents = { radius, radius, height };

	CollisionHitList hits;

	if (testActors && !visibilityOnly)
	{
		double dradius = radius;
		double dheight = height;
		dvec3 dlocation = to_dvec3(location);

		VisitActorsInBox(GetStartExtents(location, extents), GetEndExtents(location, extents), [&](UActor* actor) {
			if (CylinderActorOverlap(dlocation, dheight, dradius, actor))
			{
				vec3 normal(0.0f); // To do: do we need the normal for contact tests?
				hits.push_back({ 0.0f, normal, actor, nullptr, nullptr });
			}
			return false;
		});
	}

	// The level's own box check, as ULevel::MultiPointCheck asks it (Engine.dll
	// 0x1039acc0): its model with no owner; for an actor of no size, whether
	// the point is in solid
	if (testWorld)
	{
		OverlapAABBModel overlapmodel;
		if (overlapmodel.TestOverlap(GetLevel()->Model, location, extents))
			hits.push_back({ 0.0f, vec3(0.0f), nullptr, nullptr, nullptr });
	}

	// Only include each actor once, in the order the cells gave them: sorted
	// by address, the list came out in heap order, which a travel, a save's
	// load or another platform's allocator changes (TouchingActors walks it).

	CollisionHitList uniqueHits;
	for (auto& hit : hits)
	{
		if (hit.Actor)
		{
			bool seen = false;
			for (const auto& kept : uniqueHits)
				seen = seen || kept.Actor == hit.Actor;
			if (!seen)
				uniqueHits.push_back(hit);
		}
		else
		{
			uniqueHits.push_back(hit);
		}
	}

	return uniqueHits;
}

// FCollisionHash::ActorRadiusCheck (Engine.dll 0x103589e0): each actor in
// the cells of the radius's box whose place lies within the radius, each
// find put at the list's head, so the last found comes first
Array<UActor*> OverlapTester::ActorRadiusCheck(const vec3& origin, float radius)
{
	vec3 extents = { radius, radius, radius };
	float radiusSquared = radius * radius;

	Array<UActor*> hits;

	VisitActorsInBox(GetStartExtents(origin, extents), GetEndExtents(origin, extents), [&](UActor* actor) {
		vec3 d = actor->Location() - origin;
		if (dot(d, d) < radiusSquared)
			hits.push_back(actor);
		return false;
	});
	std::reverse(hits.begin(), hits.end());
	return hits;
}

Array<UActor*> OverlapTester::CollidingActors(const vec3& origin, float radius)
{
	dvec3 dorigin = to_dvec3(origin);
	double dradius = radius;
	vec3 extents = { radius, radius, radius };

	Array<UActor*> hits;

	VisitActorsInBox(GetStartExtents(origin, extents), GetEndExtents(origin, extents), [&](UActor* actor) {
		if (SphereActorOverlap(dorigin, dradius, actor))
			hits.push_back(actor);
		return false;
	});

	std::set<UActor*> seenActors;
	Array<UActor*> uniqueHits;
	uniqueHits.reserve(hits.size());
	for (auto& hit : hits)
	{
		if (hit)
		{
			if (seenActors.find(hit) == seenActors.end())
			{
				seenActors.insert(hit);
				uniqueHits.push_back(hit);
			}
		}
		else
		{
			uniqueHits.push_back(hit);
		}
	}

	return uniqueHits;
}

Array<UActor*> OverlapTester::CollidingActors(const vec3& origin, float height, float radius)
{
	dvec3 dorigin = to_dvec3(origin);
	double dheight = height;
	double dradius = radius;
	vec3 extents = { radius, radius, height };

	Array<UActor*> hits;

	VisitActorsInBox(GetStartExtents(origin, extents), GetEndExtents(origin, extents), [&](UActor* actor) {
		if (CylinderActorOverlap(dorigin, dheight, dradius, actor))
			hits.push_back(actor);
		return false;
	});

	std::set<UActor*> seenActors;
	Array<UActor*> uniqueHits;
	uniqueHits.reserve(hits.size());
	for (auto& hit : hits)
	{
		if (hit)
		{
			if (seenActors.find(hit) == seenActors.end())
			{
				seenActors.insert(hit);
				uniqueHits.push_back(hit);
			}
		}
		else
		{
			uniqueHits.push_back(hit);
		}
	}

	return uniqueHits;
}

// What the actor's brush overlaps were it at location, turned to rotation,
// as the original's ActorEncroachmentCheck (Engine.dll 0x10358d50) asks the
// brush's box check with the actor its owner: each actor but a brush, in the
// cells of the brush's box there, its cylinder's box as it stands, along the
// world's axes, against the brush's planes taken into the world, PostScale
// in them (ABrush::ToWorld, 0x1030f310).
Array<UActor*> OverlapTester::EncroachingActors(UActor* actor, const vec3& location, const Rotator& rotation)
{
	UModel* brush = actor->Brush();
	UBrush* brushActor = UActor::TryCast<UBrush>(actor);
	if (!brush || !brushActor)
		return CollidingActors(location, actor->CollisionHeight(), actor->CollisionRadius());

	// The cells of the brush's own box there (GetActorExtent), each find
	// put at the list's end
	vec3 lo, hi;
	CollisionSystem::HashBox(actor, location, rotation, lo, hi);
	ivec3 start, end;
	CollisionSystem::BoxCells(lo, hi, start, end);

	BrushPlanes planes(location, rotation, brushActor->MainScale().Scale, brushActor->PostScale().Scale, brushActor->PrePivot());
	Array<UActor*> hits;

	VisitActorsInBox(start, end, [&](UActor* testActor) {
		if (testActor == actor || testActor->Brush())
			return false;

		vec3 testExtents = { testActor->CollisionRadius(), testActor->CollisionRadius(), testActor->CollisionHeight() };
		OverlapAABBModel test;
		if (test.TestOverlap(brush, testActor->Location(), testExtents, &planes))
			hits.push_back(testActor);
		return false;
	});

	std::set<UActor*> seenActors;
	Array<UActor*> uniqueHits;
	uniqueHits.reserve(hits.size());
	for (auto& hit : hits)
	{
		if (hit)
		{
			if (seenActors.find(hit) == seenActors.end())
			{
				seenActors.insert(hit);
				uniqueHits.push_back(hit);
			}
		}
		else
		{
			uniqueHits.push_back(hit);
		}
	}

	return uniqueHits;
}

// What a cylinder standing at a spot overlaps, as the original's encroachment
// check finds it (FCollisionHash::ActorEncroachmentCheck, Engine.dll
// 0x10358d50): each actor in the hash's cells there by its cylinder, against
// the spot's cylinder (UPrimitive::PointCheck, 0x103d87e0); a brush that is
// not static -- a mover -- passed over (0x10358ee6-0x10358f16), so a spawn
// or a SetLocation into a door is not asked of it: a LAM placed on a door's
// face, 2.5 units into its brush, is placed (EncroachMoverConsole).
Array<UActor*> OverlapTester::EncroachedActors(const vec3& location, float height, float radius)
{
	dvec3 dlocation = to_dvec3(location);
	// The cells of the cylinder's own box there (GetActorExtent), each find
	// put at the list's end
	vec3 lo, hi;
	CollisionSystem::CylinderBox(location, radius, height, lo, hi);
	ivec3 start, end;
	CollisionSystem::BoxCells(lo, hi, start, end);
	Array<UActor*> hits;

	VisitActorsInBox(start, end, [&](UActor* actor) {
		if (actor->Brush() && UObject::TryCast<UBrush>(actor) && !actor->bStatic())
			return false;
		if (CylinderCylinderOverlap(dlocation, height, radius, to_dvec3(actor->Location()), actor->CollisionHeight(), actor->CollisionRadius()))
			hits.push_back(actor);
		return false;
	});
	return hits;
}

bool OverlapTester::IsOverlapping(UActor* actor1, UActor* actor2)
{
	return CylinderActorOverlap(to_dvec3(actor1->Location()), actor1->CollisionHeight(), actor1->CollisionRadius(), actor2);
}

bool OverlapTester::CylinderCylinderOverlap(const dvec3& cylinderCenterA, double cylinderHeightA, double cylinderRadiusA, const dvec3& cylinderCenterB, double cylinderHeightB, double cylinderRadiusB)
{
	dvec3 dist = cylinderCenterA - cylinderCenterB;
	double h = cylinderHeightA + cylinderHeightB;
	double r = cylinderRadiusA + cylinderRadiusB;
	return std::abs(dist.z) < h && dot(dist.xy(), dist.xy()) < r * r;
}

// Returns the squared distance between point c and segment ab
static double SqDistPointSegment(const dvec3& a, const dvec3& b, const dvec3& c)
{
	dvec3 ab = b - a;
	dvec3 ac = c - a;
	double e = dot(ac, ab);

	// Handle cases where c projects outside ab
	if (e <= 0.0)
	{
		return dot(ac, ac);
	}
	double f = dot(ab, ab);
	if (e >= f)
	{
		dvec3 bc = c - b;
		return dot(bc, bc);
	}

	// Handle cases where c projects onto ab
	return dot(ac, ac) - e * e / f;
}

bool OverlapTester::SphereCapsuleOverlap(const dvec3& sphereCenter, double sphereRadius, const dvec3& capsuleCenter, double capsuleHeight, double capsuleRadius)
{
	dvec3 base = capsuleCenter - capsuleHeight;
	dvec3 tip = capsuleCenter + capsuleHeight;

	// Compute (squared) distance between sphere center and capsule line segment
	double dist2 = SqDistPointSegment(base, tip, sphereCenter);

	// If (squared) distance smaller than (squared) sum of radii, they collide
	double totalradius = sphereRadius + capsuleRadius;
	return dist2 <= totalradius * totalradius;
}

bool OverlapTester::SphereActorOverlap(const dvec3& sphereCenter, double sphereRadius, UActor* actor)
{
	if (actor->Brush()) // Ignore brushes for now
		return false;

	dvec3 center = to_dvec3(actor->Location());
	double height = actor->CollisionHeight();
	double radius = actor->CollisionRadius();

	// Note: this isn't entirely accurate - what does UE1 do?
	return SphereCapsuleOverlap(sphereCenter, sphereRadius, center, std::max(height - radius, 0.0), radius);
}

bool OverlapTester::CylinderActorOverlap(const dvec3& origin, double cylinderHeight, double cylinderRadius, UActor* actor)
{
	if (actor->Brush()) // Ignore brushes for now
		return false;

	dvec3 center = to_dvec3(actor->Location());
	double height = actor->CollisionHeight();
	double radius = actor->CollisionRadius();
	return CylinderCylinderOverlap(origin, cylinderHeight, cylinderRadius, center, height, radius);
}
