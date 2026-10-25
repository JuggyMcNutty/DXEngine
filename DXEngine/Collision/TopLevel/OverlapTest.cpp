
#include "Precomp.h"
#include "OverlapTest.h"
#include "Collision/BottomLevel/OverlapAABBModel.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
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

	if (testWorld)
	{
		if (extents != vec3(0.0f))
		{
			OverlapAABBModel overlapmodel;
			CollisionHitList worldHits = overlapmodel.TestOverlap(GetLevel()->Model, location, extents, visibilityOnly);
			hits.push_back(worldHits);
		}
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

// What the actor's brush overlaps were it at location, turned to rotation
// (ActorEncroachmentCheck): each actor but a brush, its cylinder's box taken
// into the brush's space, in the cells of the brush's box there.
Array<UActor*> OverlapTester::EncroachingActors(UActor* actor, const vec3& location, const Rotator& rotation)
{
	UModel* brush = actor->Brush();
	if (!brush)
		return CollidingActors(location, actor->CollisionHeight(), actor->CollisionRadius());

	vec3 origin = location;

	mat4 rotateObjToWorld = Coords::Rotation(rotation).ToMatrix();
	mat4 rotateWorldToObj = mat4::transpose(rotateObjToWorld);
	vec3 prePivot = actor->PrePivot();

	vec3 mainScale(1.0f);
	if (UMover* mover = UActor::TryCast<UMover>(actor))
	{
		mainScale = mover->MainScale().Scale;
	}

	mat4 objectToWorld = mat4::translate(origin) * rotateObjToWorld * mat4::scale(mainScale) * mat4::translate(-prePivot);
	BBox bbox = brush->BoundingBox.transform(objectToWorld);
	vec3 center = bbox.center();
	vec3 extents = bbox.extents() + 0.1f;

	Array<UActor*> hits;

	VisitActorsInBox(GetStartExtents(center, extents), GetEndExtents(center, extents), [&](UActor* testActor) {
		if (testActor == actor || testActor->Brush())
			return false;

		vec3 localOrigin = (rotateWorldToObj * vec4(testActor->Location() - origin, 1.0f)).xyz() / mainScale + prePivot;

		float testHeight = testActor->CollisionHeight();
		float testRadius = testActor->CollisionRadius();
		vec3 testExtents = { testRadius, testRadius, testHeight };
		testExtents /= mainScale;

		OverlapAABBModel test;
		auto modelHits = test.TestOverlap(brush, localOrigin, testExtents, false);
		if (!modelHits.empty())
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
// check finds it: other actors by their cylinders, movers by their brushes,
// the cylinder's box taken into the mover's own space as the traces take a
// line.
Array<UActor*> OverlapTester::EncroachedActors(const vec3& location, float height, float radius)
{
	dvec3 dlocation = to_dvec3(location);
	vec3 extents = { radius, radius, height };
	Array<UActor*> hits;

	VisitActorsInBox(GetStartExtents(location, extents), GetEndExtents(location, extents), [&](UActor* actor) {
		UMover* mover = UObject::TryCast<UMover>(actor);
		if (mover && mover->Brush())
		{
			mat4 rotateWorldToObj = mat4::transpose(Coords::Rotation(mover->Rotation()).ToMatrix());
			vec3 scale = mover->MainScale().Scale;
			vec3 localOrigin = (rotateWorldToObj * vec4(location - mover->Location(), 1.0f)).xyz() / scale + mover->PrePivot();
			vec3 localExtents = extents / vec3(std::abs(scale.x), std::abs(scale.y), std::abs(scale.z));
			OverlapAABBModel test;
			if (!test.TestOverlap(mover->Brush(), localOrigin, localExtents, false).empty())
				hits.push_back(actor);
		}
		else if (CylinderActorOverlap(dlocation, height, radius, actor))
		{
			hits.push_back(actor);
		}
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
