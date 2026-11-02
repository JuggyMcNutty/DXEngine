#pragma once

#include "CollisionTest.h"

class TraceTester : public CollisionTester
{
public:
	using CollisionTester::CollisionTester;

	CollisionHitList Trace(const vec3& from, const vec3& to, float height, float radius, bool traceActors, bool traceWorld, bool visibilityOnly);
	bool TraceAnyHit(vec3 from, vec3 to, UActor* tracingActor, bool traceActors, bool traceWorld, bool visibilityOnly);
	bool SightBlocked(const vec3& from, const vec3& to, const std::function<bool(UActor* actor)>& blocksSight);

private:
	CollisionHitList TraceLine(const vec3& from, const vec3& to, bool traceActors, bool traceWorld, bool visibilityOnly);
	CollisionHitList TraceBox(const vec3& from, const vec3& to, const vec3& extents, bool traceActors, bool traceWorld, bool visibilityOnly);
	static CollisionHitList SortedFirstHits(CollisionHitList& hits);
	template<typename Visit> bool VisitActorsOnRay(const vec3& from, const vec3& to, Visit&& visit);
	void TraceActor(UActor* actor, const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, double height, double radius, bool traceActors, bool traceWorld, bool visibilityOnly, CollisionHitList& hits);
};
