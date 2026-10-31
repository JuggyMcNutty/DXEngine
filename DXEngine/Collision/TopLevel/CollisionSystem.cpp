
#include "Precomp.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "CollisionSystem.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"
#include "Math/floating.h"
#include "Math/coords.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "Collision/TopLevel/OverlapTest.h"
#include "Collision/TopLevel/TraceTest.h"
#include "GC/GC.h"

bool CollisionSystem::IsOverlapping(UActor* actor1, UActor* actor2)
{
	OverlapTester overlap(this);
	return overlap.IsOverlapping(actor1, actor2);
}

CollisionHitList CollisionSystem::OverlapTest(UActor* actor)
{
	return OverlapTest(actor->Location(), actor->CollisionHeight(), actor->CollisionRadius(), true, false, false);
}

CollisionHitList CollisionSystem::OverlapTest(const vec3& location, float height, float radius, bool testActors, bool testWorld, bool visibilityOnly)
{
	OverlapTester overlap(this);
	return overlap.TestOverlap(location, height, radius, testActors, testWorld, visibilityOnly);
}

bool CollisionSystem::TraceAnyHit(vec3 from, vec3 to, UActor* tracingActor, bool traceActors, bool traceWorld, bool visibilityOnly)
{
	TraceTester trace(this);
	return trace.TraceAnyHit(from, to, tracingActor, traceActors, traceWorld, visibilityOnly);
}

CollisionHitList CollisionSystem::Trace(const vec3& from, const vec3& to, float height, float radius, bool traceActors, bool traceWorld, bool visibilityOnly)
{
	TraceTester trace(this);
	return trace.Trace(from, to, height, radius, traceActors, traceWorld, visibilityOnly);
}

CollisionHitList CollisionSystem::TraceDecal(const dvec3& origin, double tmin, const dvec3& dirNormalized, double tmax, bool visibilityOnly)
{
	TraceRayModel trace;
	return trace.Trace(Level->Model, origin, tmin, dirNormalized, tmax, visibilityOnly);
}

bool CollisionSystem::SightBlocked(const vec3& from, const vec3& to, const std::function<bool(UActor* actor)>& blocksSight)
{
	TraceTester trace(this);
	return trace.SightBlocked(from, to, blocksSight);
}

Array<UActor*> CollisionSystem::ActorRadiusCheck(const vec3& origin, float radius)
{
	OverlapTester overlap(this);
	return overlap.ActorRadiusCheck(origin, radius);
}

CollisionHit CollisionSystem::TraceFirstHit(const vec3& from, const vec3& to, UActor* tracingActor, const vec3& extents, const TraceFlags& flags)
{
	for (const CollisionHit& hit : Trace(from, to, extents.z, extents.x, flags.traceActors(), flags.traceWorld(), false))
	{
		if (hit.Actor && (!tracingActor || !tracingActor->IsOwnedBy(hit.Actor)))
		{
			if (hit.Actor->IsA("Pawn"))
			{
				if (flags.pawns)
					return hit;
			}
			else if (hit.Actor->IsA("Mover"))
			{
				if (flags.movers)
					return hit;
			}
			else if (hit.Actor->IsA("ZoneInfo"))
			{
				if (flags.zoneChanges)
					return hit;
			}
			else if (flags.others)
			{
				if (!flags.onlyProjectiles || hit.Actor->bProjTarget() || (hit.Actor->bBlockActors() && hit.Actor->bBlockPlayers()))
					return hit;
			}
		}
		else if (flags.world && !hit.Actor)
		{
			CollisionHit worldHit = hit;
			if (tracingActor)
				worldHit.Actor = tracingActor->Level();
			return worldHit;
		}
	}
	return {};
}

Array<UActor*> CollisionSystem::CollidingActors(const vec3& origin, float radius)
{
	OverlapTester overlap(this);
	return overlap.CollidingActors(origin, radius);
}

Array<UActor*> CollisionSystem::CollidingActors(const vec3& origin, float height, float radius)
{
	OverlapTester overlap(this);
	return overlap.CollidingActors(origin, height, radius);
}

Array<UActor*> CollisionSystem::EncroachingActors(UActor* actor, const vec3& location, const Rotator& rotation)
{
	OverlapTester overlap(this);
	return overlap.EncroachingActors(actor, location, rotation);
}

Array<UActor*> CollisionSystem::EncroachedActors(const vec3& location, float height, float radius)
{
	OverlapTester overlap(this);
	return overlap.EncroachedActors(location, height, radius);
}

void CollisionSystem::SetLevel(ULevel* level)
{
	Level = level;
}

// A destroyed actor never goes back in: a move after its Destroy (a
// BloodDrop's fall goes on after its HitWall destroys it, as the original's
// does) would leave the hash holding it after it is freed.
void CollisionSystem::AddToCollision(UActor* actor)
{
	if (actor->bCollideActors() && !actor->bDeleteMe())
	{
		vec3 location = actor->Location();
		float height = actor->CollisionHeight();
		float radius = actor->CollisionRadius();
		vec3 extents = { radius, radius, height };

		if (UMover* mover = UObject::TryCast<UMover>(actor))
		{
			UModel* brush = mover->Brush();
			if (brush)
			{
				mat4 objectToWorld = mat4::translate(actor->Location()) * Coords::Rotation(actor->Rotation()).ToMatrix() * mat4::scale(mover->MainScale().Scale) * mat4::translate(-actor->PrePivot());
				BBox bbox = brush->BoundingBox.transform(objectToWorld);
				location = bbox.center();
				extents = bbox.extents() + 0.1f; // 0.1 for numerical stability
			}
		}

		actor->Collision.Inserted = true;
		actor->Collision.Location = location;
		actor->Collision.Extents = extents;

		ivec3 start = GetStartExtents(location, extents);
		ivec3 end = GetEndExtents(location, extents);
		for (int z = start.z; z < end.z; z++)
		{
			for (int y = start.y; y < end.y; y++)
			{
				for (int x = start.x; x < end.x; x++)
				{
					ActorCells.Get(GetBucketId(x, y, z)).push_back(actor);
				}
			}
		}
	}
}

void CollisionSystem::RemoveFromCollision(UActor* actor)
{
	if (actor->Collision.Inserted)
	{
		vec3 location = actor->Collision.Location;
		vec3 extents = actor->Collision.Extents;

		ivec3 start = GetStartExtents(location, extents);
		ivec3 end = GetEndExtents(location, extents);
		for (int z = start.z; z < end.z; z++)
		{
			for (int y = start.y; y < end.y; y++)
			{
				for (int x = start.x; x < end.x; x++)
				{
					ActorCells.Remove(GetBucketId(x, y, z), actor);
				}
			}
		}

		actor->Collision.Inserted = false;
	}
}

// Deus Ex's movers in the BSP: the original's tracker, made at a map's load
// with a client and after a save (dx-reverse-info engine-dll.md, traces: the
// movers in the BSP)
void CollisionSystem::FileMovers()
{
	Tracker.Start(Level);
}

// Each frame drawn, every mover the frame shows -- not hidden, in view or
// not -- that has moved or turned since it was filed is filed again where
// it is, in the actor list's order (Render.dll's SetupDynamics, 0x10b22d90,
// which hands the tracker each moving brush it does not pass over)
void CollisionSystem::RefileMovers(const std::function<bool(UMover*)>& shown)
{
	if (!Level || !Tracker.Active())
		return;
	for (UActor* actor : Level->Actors)
	{
		UMover* mover = UObject::TryCast<UMover>(actor);
		if (mover && !mover->bStatic() && mover->Brush() && shown(mover))
			Tracker.Update(mover);
	}
}

// The filed polygons' textures and movers, which a destroyed mover's filed
// surfaces keep naming (BrushTracker)
void CollisionSystem::Mark(GCMarker& marker)
{
	Tracker.Mark(marker);
}

const Array<BspNode>& CollisionSystem::LineNodes() const
{
	return Tracker.Active() ? Tracker.Nodes() : Level->Model->Nodes;
}

bool CollisionSystem::LineRootOutside() const
{
	return Level->Model->RootOutside != 0;
}

const BspSurface& CollisionSystem::LineSurface(int index) const
{
	return Tracker.Active() ? Tracker.Surface(index) : Level->Model->Surfaces[index];
}

// The polygon's flags without the editor's own (PF_NoAddToBSP: processed,
// cut, selected, memorized), and PF_SpecialLit for a bSpecialLit mover: the
// surface the original's tracker makes for each polygon of a moving brush
// as it starts (Engine.dll FMovingBrushTracker::AddActor, 0x1037b5c0)
uint32_t CollisionSystem::FiledFlags(UMover* mover, const Poly& poly)
{
	return (poly.PolyFlags & ~PF_NoAddToBSP) | (mover->bSpecialLit() ? PF_SpecialLit : 0);
}
