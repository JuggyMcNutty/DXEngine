
#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include "Packages/Engine/Actors/UProjectile.h"
#include "Packages/Engine/Actors/Brush/UBrush.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Decoration/UDecoration.h"
#include "Packages/Engine/Actors/Decoration/UCarcass.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Utils/Random.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"

void UActor::TickPhysics(float elapsed)
{
	bool hadPhysics = Physics() != PHYS_None;
	PhysicsFrameLeft = 0.0f;
	PhysicsFrameEnded = false;

	// The original's performPhysics, once a tick with the whole tick, each
	// physics stepping it as it will (AActor's, Engine.dll 0x103c9770; APawn's,
	// 0x103c9a70): a pawn walks, falls, swims, flies, follows a path or
	// climbs as a spider; any other actor falls, flies as a projectile,
	// rolls, follows a path, moves as a brush or trails its owner; in any
	// other physics it only turns. Then the turn, and the pending touch.
	int mode = Physics();
	if (mode != PHYS_None)
	{
		if (bIsPawn())
		{
			switch (mode)
			{
			case PHYS_Walking: TickWalking(elapsed); break;
			case PHYS_Falling: TickFalling(elapsed); break;
			case PHYS_Swimming: TickSwimming(elapsed); break;
			case PHYS_Flying: TickFlying(elapsed); break;
			case PHYS_Interpolating: TickInterpolating(elapsed); break;
			case PHYS_Spider: TickSpider(elapsed); break;
			}
		}
		else
		{
			switch (mode)
			{
			case PHYS_Falling: TickFalling(elapsed); break;
			case PHYS_Projectile: TickProjectile(elapsed); break;
			case PHYS_Rolling: TickRolling(elapsed); break;
			case PHYS_Interpolating: TickInterpolating(elapsed); break;
			case PHYS_MovingBrush: TickMovingBrush(elapsed); break;
			case PHYS_Trailer: TickTrailer(elapsed); break;
			}
		}
		if (!bDeleteMe())
			TickRotating(elapsed);
	}

	CheckPendingTouch();
	PhysicsFrameLeft = 0.0f;
	PhysicsFrameEnded = false;

	// A pawn's move timer counts down with its physics, and its running
	// average of the tick, which moveToward reads, follows, as the
	// original's APawn::performPhysics ends (0x103c9a70), run only for an
	// actor with physics (AActor::Tick 0x103a1aa0): a pawn with none keeps
	// its timer, its strafe or move never timing out.
	if (hadPhysics && bIsPawn())
	{
		if (UPawn* pawn = UObject::TryCast<UPawn>(this))
		{
			pawn->MoveTimer() -= elapsed;
			pawn->AvgPhysicsTime() = 0.8f * pawn->AvgPhysicsTime() + 0.2f * elapsed;
		}
	}
}

float UActor::EndPhysicsFrame()
{
	float left = PhysicsFrameLeft;
	PhysicsFrameLeft = 0.0f;
	PhysicsFrameEnded = true;
	return left;
}

void UActor::SetPhysics(uint8_t newPhysics)
{
	Physics() = newPhysics;
}

// AActor::setPhysics (Engine.dll 0x103c95f0, dx-reverse-info/engine-dll.md,
// moving): only a change of physics does anything. A change to none,
// walking, rolling, rotating or spider takes the floor for the actor's base
// unless it is already, or with no floor finds its base below; a change to
// any other leaves the base. None and rotating stop the velocity and
// acceleration -- unless the floor's event changed the physics again.
void UActor::SetPhysics(uint8_t newPhysics, UActor* newFloor)
{
	if (Physics() == newPhysics)
		return;

	SetPhysics(newPhysics);
	if (newPhysics == PHYS_None || newPhysics == PHYS_Walking || newPhysics == PHYS_Rolling || newPhysics == PHYS_Rotating || newPhysics == PHYS_Spider)
	{
		if (!newFloor)
			FindBase();
		else if (ActorBase() != newFloor)
			SetSupportBase(newFloor);
	}
	else if (ActorBase())
	{
		SetBase(nullptr, true);
	}

	if (Physics() == PHYS_None || Physics() == PHYS_Rotating)
	{
		Velocity() = vec3(0.0f);
		Acceleration() = vec3(0.0f);
	}
}

// AActor::SetSupportBase (0x103c9390): the floor's SupportActor event, which
// bases the actor by default (Deus Ex's pawns and decorations bounce it off,
// or take stomp damage, instead); no floor leaves the base.
void UActor::SetSupportBase(UActor* floor)
{
	if (floor)
		CallEvent(floor, "SupportActor", { ExpressionValue::ObjectValue(this) });
	else
		SetBase(nullptr, true);
}

// AActor::FindBase (0x103c9470): whatever a box of the actor's size meets
// within 8 units down -- anything colliding, or the level's LevelInfo -- is
// its support base, unless that is based on it.
void UActor::FindBase()
{
	TraceFlags flags;
	flags.pawns = true;
	flags.movers = true;
	flags.world = true;
	flags.others = true;
	CollisionHit hit = XLevel()->Collision.TraceFirstHit(Location(), Location() - vec3(0.0f, 0.0f, 8.0f), this, vec3(CollisionRadius(), CollisionRadius(), CollisionHeight()), flags);
	if (ActorBase() != hit.Actor)
	{
		// The hit standing on this one, through any chain of bases: this
		// IsBasedOn(other) walks other's bases for this, where the original's
		// hit->IsBasedOn(this) walks the hit's (Engine.dll 0x1034c8d0)
		if (hit.Actor && IsBasedOn(hit.Actor))
			return;
		SetSupportBase(hit.Actor);
	}
}

void UActor::SetCollision(bool newColActors, bool newBlockActors, bool newBlockPlayers)
{
	XLevel()->Collision.RemoveFromCollision(this);
	bCollideActors() = newColActors;
	bBlockActors() = newBlockActors;
	bBlockPlayers() = newBlockPlayers;
	XLevel()->Collision.AddToCollision(this);
}

bool UActor::SetLocation(const vec3& newLocation, bool noCheck)
{
	if (bStatic() || !bMovable())
		return false;

	vec3 location = newLocation;
	if (!noCheck)
	{
		bool findRoom = bCollideWorld() || (bCollideWhenPlacing() && Level()->NetMode() != NM_Client);
		if (findRoom)
		{
			auto result = FindSpot(newLocation, CollisionRadius(), CollisionHeight(), false);
			if (!result.first)
				return false;
			location = result.second;
		}

		// Deus Ex: what stands at the spot may stop the move (EncroachingOn),
		// and what blocks hears it come (EncroachedBy), before it moves; and
		// the touches are made there too, as the original's FarMoveActor
		// makes them -- before what stands on it is unbased and before
		// Location changes, so a Touch or UnTouch handler sees it where it
		// was.
		if (CheckEncroachment(location, Level()->bBegunPlay()))
			return false;
	}

	// Whatever stands on it is left behind.
	if (StandingCount() > 0)
	{
		Array<UActor*> standing = BasedActors;
		for (UActor* actor : standing)
		{
			if (actor && actor->ActorBase() == this)
				actor->SetBase(nullptr, true);
		}
	}
	bJustTeleported() = true;

	XLevel()->Collision.RemoveFromCollision(this);
	Location() = location;
	OldLocation() = location;
	XLevel()->Collision.AddToCollision(this);

	UpdateActorZone();
	return true;
}

bool UActor::SetRotation(const Rotator& newRotation, bool checkBrush)
{
	if (Brush() && UObject::TryCast<UBrush>(this))
	{
		if (checkBrush)
			return MoveBrush(vec3(0.0f), newRotation);
		XLevel()->Collision.RemoveFromCollision(this);
		Rotation() = newRotation;
		XLevel()->Collision.AddToCollision(this);
		return true;
	}

	// To do: return false if there isn't room

	Rotator oldRotation = Rotation();
	TurnBasedActors(oldRotation, newRotation);
	Rotation() = newRotation;
	return true;
}

// What stands on an actor that turns, as the original's MoveActor carries it
// (Engine.dll 0x103990e0, dx-reverse-info/engine-dll.md, moving), MoveBrush's
// riders alike: first, each moved round the actor by the change of yaw, each
// yaw to a multiple of 4, what it stands on passed over, then its own yaw (and
// a pawn's view) turned by the change -- its pitch and roll left as they are.
void UActor::TurnBasedActors(const Rotator& oldRotation, const Rotator& newRotation)
{
	if (oldRotation == newRotation || StandingCount() <= 0)
		return;
	int yaw = newRotation.Yaw - oldRotation.Yaw;
	Coords yawRot = Coords::InverseRotation(Rotator(0, (newRotation.Yaw & 0xfffc) - (oldRotation.Yaw & 0xfffc), 0));
	vec3 baseLoc = Location();
	Array<UActor*> riders = BasedActors;
	for (UActor* rider : riders)
	{
		if (!rider || rider->ActorBase() != this || rider->bDeleteMe())
			continue;
		rider->TryMove((baseLoc + yawRot * (rider->Location() - baseLoc)) - rider->Location(), false, false);
		if (yaw != 0)
		{
			rider->SetRotation(rider->Rotation() + Rotator(0, yaw, 0));
			if (UPawn* pawn = UObject::TryCast<UPawn>(rider))
				pawn->ViewRotation().Yaw += yaw;
		}
	}
}

bool UActor::SetCollisionSize(float newRadius, float newHeight)
{
	// To do: return false if there isn't room

	XLevel()->Collision.RemoveFromCollision(this);
	CollisionRadius() = newRadius;
	CollisionHeight() = newHeight;
	XLevel()->Collision.AddToCollision(this);
	return true;
}

void UActor::PhysLanded(UActor* hitActor, const vec3& hitNormal, float remaining)
{
	DeusExPhysLanded(hitActor, hitNormal, remaining);
}

// The original's processLanded (Engine.dll 0x103cef60; dx-reverse-info
// engine-dll.md, moving), each branch in its order.
void UActor::DeusExPhysLanded(UActor* hitActor, const vec3& hitNormal, float remaining)
{
	// A non-pawn in a zone that throws back is thrown again: the zone's
	// velocity and 80 up.
	UZoneInfo* zone = Region().Zone;
	if (!bIsPawn() && zone && zone->bBounceVelocity() && zone->ZoneVelocity() != vec3(0.0f))
	{
		Velocity() = zone->ZoneVelocity() + vec3(0.0f, 0.0f, 80.0f);
		return;
	}

	TraceFlags projTargets; // the original's flags 55
	projTargets.pawns = true;
	projTargets.movers = true;
	projTargets.world = true;
	projTargets.others = true;
	projTargets.onlyProjectiles = true;
	float radius = CollisionRadius();
	float height = CollisionHeight();

	UPawn* pawn = UObject::TryCast<UPawn>(this);
	if (pawn)
	{
		// With nothing under 0.9 of its box within 0.2 of its radius + 8, a
		// pawn is fitted with room to spare and, if that moved it, pushed on
		// at random, still falling: off a ledge it hangs over.
		CollisionHit below = XLevel()->Collision.TraceFirstHit(Location(), Location() - vec3(0.0f, 0.0f, radius * 0.2f + 8.0f), this, vec3(radius, radius, height) * 0.9f, projTargets);
		if (!below.Actor)
		{
			auto spot = FindSpot(Location(), radius * 1.1f, height * 1.1f, false);
			if (spot.first && spot.second != Location())
			{
				SetLocation(spot.second);
				Velocity().x += FRand() * 60.0f - 30.0f;
				Velocity().y += FRand() * 60.0f - 30.0f;
				return;
			}
		}
	}
	else if (UDecoration* decor = UObject::TryCast<UDecoration>(this))
	{
		if (decor->numLandings() >= 5)
		{
			decor->numLandings() = 0;
		}
		else
		{
			// Nothing under its middle: four boxes, a quarter of it each, 8
			// down from its corners. More than one free, one of each
			// diagonal at least, sends it off toward the free side.
			CollisionHit middle = XLevel()->Collision.TraceFirstHit(Location() - vec3(0.0f, 0.0f, height * 0.8f), Location() - vec3(0.0f, 0.0f, height + radius + 8.0f), this, vec3(0.0f), projTargets);
			if (!middle.Actor)
			{
				TraceFlags colliding; // the original's flags 23
				colliding.pawns = true;
				colliding.movers = true;
				colliding.world = true;
				colliding.others = true;
				vec3 extent(radius * 0.5f, radius * 0.5f, height);
				auto cornerFree = [&](float x, float y) {
					vec3 start = Location() + vec3(x * radius * 0.5f, y * radius * 0.5f, 0.0f);
					return XLevel()->Collision.TraceFirstHit(start, start - vec3(0.0f, 0.0f, 8.0f), this, extent, colliding).Actor ? 0 : 1;
				};
				int pp = cornerFree(1.0f, 1.0f), mp = cornerFree(-1.0f, 1.0f), mm = cornerFree(-1.0f, -1.0f), pm = cornerFree(1.0f, -1.0f);
				if (pp + mp + mm + pm > 1 && mm + pp != 0 && pm + mp != 0)
				{
					decor->numLandings()++;
					float speed = std::clamp(-Velocity().z, 30.0f, radius + 30.0f);
					Velocity() = vec3((float)(pm + pp - mp - mm), (float)(mp + pp - mm - pm), 0.5f) * (2.0f * speed);
					return;
				}
			}

			// A sliding carcass bounces off a slope, a landing counted one
			// time in five.
			UCarcass* carcass = UObject::TryCast<UCarcass>(this);
			if (carcass && hitNormal.z < 0.9f && carcass->bSlidingCarcass())
			{
				if (FRand() < 0.2f)
					decor->numLandings()++;
				Velocity() = hitNormal * 120.0f;
				Velocity().z = 70.0f;
				return;
			}
			decor->numLandings() = 0;
		}
	}

	// It lands: the hit actor, the level's LevelInfo for the world, is the
	// floor setPhysics is given. A pawn walking walks out the rest of the
	// tick, its acceleration made unit length first.
	CallEvent(this, EventName::Landed, { ExpressionValue::VectorValue(hitNormal) });
	if (Physics() == PHYS_Falling)
		SetPhysics(pawn ? PHYS_Walking : PHYS_None, hitActor ? hitActor : Level());
	if (pawn && Physics() == PHYS_Walking)
	{
		vec3& acceleration = Acceleration();
		float squareSum = dot(acceleration, acceleration);
		acceleration = squareSum < 1e-8f ? vec3(0.0f) : acceleration * (1.0f / std::sqrt(squareSum));
		if (remaining > 0.01f)
			TickWalking(remaining);
	}
}

bool UActor::HasLeftWorld()
{
	if (Region().ZoneNumber != 0)
		return false;

	CallEvent(this, EventName::FellOutOfWorld);
	return true;
}

void UActor::FireHitWall(const CollisionHit& hit)
{
	CallEvent(this, EventName::HitWall, { ExpressionValue::VectorValue(hit.Normal), ExpressionValue::ObjectValue(hit.Actor ? hit.Actor : Level()) });
}

// AActor::processHitWall (Engine.dll 0x103cecc0, dx-reverse-info/
// engine-dll.md, moving): nothing when a pawn was hit. Any other actor gets
// HitWall if its state probes it. A pawn only with an acceleration, heading
// into the wall -- the direction to its Destination MinHitWall or less along
// the normal, both level when it walks -- and then, not probing HitWall and
// not falling, it gives up its move (MoveTimer -1, bFromWall) instead.
void UActor::ProcessHitWall(vec3 hitNormal, UActor* hitActor)
{
	if (UObject::TryCast<UPawn>(hitActor))
		return;
	if (UPawn* pawn = UObject::TryCast<UPawn>(this))
	{
		if (Acceleration() == vec3(0.0f))
			return;
		vec3 dir = pawn->Destination() - Location();
		float len = length(dir);
		dir = len > 0.0f ? dir / len : vec3(0.0f);
		if (Physics() == PHYS_Walking)
		{
			hitNormal.z = 0.0f;
			dir.z = 0.0f;
		}
		if (dot(dir, hitNormal) > pawn->MinHitWall())
			return;
		if (!IsEventEnabled(EventName::HitWall) && Physics() != PHYS_Falling)
		{
			pawn->MoveTimer() = -1.0f;
			pawn->bFromWall() = true;
			return;
		}
	}
	else if (!IsEventEnabled(EventName::HitWall))
	{
		return;
	}
	CallEvent(this, EventName::HitWall, { ExpressionValue::VectorValue(hitNormal), ExpressionValue::ObjectValue(hitActor ? hitActor : Level()) });
}

// AActor::TwoWallAdjust (Engine.dll 0x10323770): a move stopped by a second
// wall slides along the corner where the two meet, or along the second.
void UActor::TwoWallAdjust(const vec3& desiredDir, vec3& delta, const vec3& hitNormal, const vec3& oldHitNormal, float hitTime)
{
	if (dot(oldHitNormal, hitNormal) <= 0.0f)
	{
		vec3 newDir = cross(hitNormal, oldHitNormal);
		float len = length(newDir);
		newDir = len > 0.0f ? newDir / len : vec3(0.0f);
		delta = newDir * (dot(delta, newDir) * (1.0f - hitTime));
		if (dot(desiredDir, delta) < 0.0f)
			delta = -delta;
	}
	else
	{
		delta = (delta - hitNormal * dot(delta, hitNormal)) * (1.0f - hitTime);
		if (dot(delta, desiredDir) <= 0.0f)
			delta = vec3(0.0f);
	}
}

// The original's ULevel::FindSpot (engine-dll.md, teleporting an actor): the
// spot pushed out of the walls along each axis in turn by the collision
// box's reach, and kept if the box fits there; else pushed from its eight
// corners, and given up if that moved it more than half again the box's
// size. checkFirst takes the spot as it is when the box fits already.
std::pair<bool, vec3> UActor::FindSpot(vec3 location, float radius, float height, bool checkFirst)
{
	vec3 extent(radius, radius, height);
	auto fits = [&](const vec3& spot) { return XLevel()->Collision.OverlapTest(spot, height, radius, false, true, false).empty(); };
	if (extent == vec3(0.0f))
		return { fits(location), location };
	if (checkFirst && fits(location))
		return { true, location };

	vec3 spot = location;
	for (int i = -1; i < 2; i += 2)
	{
		AdjustSpot(spot, spot + vec3(i * extent.x, 0.0f, 0.0f), extent.x);
		AdjustSpot(spot, spot + vec3(0.0f, i * extent.y, 0.0f), extent.y);
		AdjustSpot(spot, spot + vec3(0.0f, 0.0f, i * extent.z), extent.z);
	}
	if (fits(spot))
		return { true, spot };

	float corner = length(extent) + 2.0f;
	for (int i = -1; i < 2; i += 2)
		for (int j = -1; j < 2; j += 2)
			for (int k = -1; k < 2; k += 2)
				AdjustSpot(spot, spot + vec3(i * extent.x, j * extent.y, k * extent.z), corner);
	if (dot(spot - location, spot - location) > dot(extent, extent) * 1.5f || !fits(spot))
		return { false, location };
	return { true, spot };
}

// A line from the spot toward dest, level and movers; on a hit the spot goes
// back along the wall's normal by (1.05 - the hit's time) of the length.
void UActor::AdjustSpot(vec3& spot, const vec3& dest, float traceLength)
{
	TraceFlags flags;
	flags.world = true;
	flags.movers = true;
	CollisionHit hit = XLevel()->Collision.TraceFirstHit(spot, dest, nullptr, vec3(0.0f), flags);
	if (hit.Fraction < 1.0f)
		spot += hit.Normal * ((1.05f - hit.Fraction) * traceLength);
}

// The original's AActor::IsBlockedBy: the world and movers block what
// collides with the world, each by its bBlockPlayers for a player's pawn
// and bBlockActors otherwise; two actors block each other when each blocks
// the other's kind, projectiles counting as players.
bool UActor::EncroachBlockedBy(UActor* other)
{
	auto playerPawn = [](UActor* a) { UPlayerPawn* p = UObject::TryCast<UPlayerPawn>(a); return p && p->Player(); };
	auto isBrush = [](UActor* a) { return a->Brush() && UObject::TryCast<UBrush>(a); };
	if (other == Level())
		return bCollideWorld();
	if (isBrush(other))
		return bCollideWorld() && (playerPawn(this) ? other->bBlockPlayers() : other->bBlockActors());
	if (isBrush(this))
		return other->bCollideWorld() && (playerPawn(other) ? bBlockPlayers() : bBlockActors());
	bool meAsPlayer = playerPawn(this) || UObject::TryCast<UProjectile>(this);
	bool otherAsPlayer = playerPawn(other) || UObject::TryCast<UProjectile>(other);
	return (meAsPlayer ? other->bBlockPlayers() : other->bBlockActors()) && (otherAsPlayer ? bBlockPlayers() : bBlockActors());
}

// The original's ULevel::CheckEncroachment (Engine.dll 0x1039a350): of what
// the actor would overlap at the spot, turned to rotation, each that blocks
// it is asked with the actor's EncroachingOn, which stops it (true) when it
// agrees; then, with touches, as FarMoveActor asks them (SpawnActor and a
// mover's move do not), what the actor no longer overlaps where it stands
// is untouched and what is at the spot and does not block it touched; and
// each that blocks it hears EncroachedBy. Only an actor that collides with
// actors or blocks is checked, or a brush that is not static. A brush that
// is not static first pushes what it meets by its move (moveSmooth); one
// pushed clear is not asked about, and one still in the way is moved back
// against the brush at its new place before it is.
bool UActor::CheckEncroachment(const vec3& location, const Rotator& rotation, bool touches)
{
	bool movingBrush = Brush() && UObject::TryCast<UBrush>(this) && !bStatic();
	if (!movingBrush && (Brush() || !(bCollideActors() || bBlockActors() || bBlockPlayers())))
		return false;

	Array<UActor*> others = movingBrush ?
		XLevel()->Collision.EncroachingActors(this, location, rotation) :
		XLevel()->Collision.EncroachedActors(location, CollisionHeight(), CollisionRadius());
	for (UActor* other : others)
	{
		if (other == this || other == Level() || other->bDeleteMe() || !EncroachBlockedBy(other))
			continue;
		if (movingBrush)
		{
			vec3 push = location - Location();
			other->TryMoveSmooth(push);
			Array<UActor*> still = XLevel()->Collision.EncroachingActors(this, location, rotation);
			if (std::find(still.begin(), still.end(), other) == still.end())
				continue;
			vec3 oldLocation = Location();
			Location() = location;
			other->TryMoveSmooth(-push);
			Location() = oldLocation;
		}
		if (CallEvent(this, EventName::EncroachingOn, { ExpressionValue::ObjectValue(other) }).ToBool())
			return true;
	}
	if (touches)
	{
		// What this no longer overlaps where it stands now (the original's
		// test is of its own place, not the spot's)
		for (UActor* actor : Touching())
		{
			if (actor && !IsOverlapping(actor))
				UnTouch(actor);
		}
	}
	for (UActor* other : others)
	{
		if (other == this || other == Level() || other->bDeleteMe())
			continue;
		if (EncroachBlockedBy(other))
			CallEvent(other, EventName::EncroachedBy, { ExpressionValue::ObjectValue(this) });
		else if (touches && !other->IsBasedOn(this) && !IsBasedOn(other) && bCollideActors() && other->bCollideActors())
			Touch(other);
	}
	return false;
}

bool UActor::IsOverlapping(UActor* other)
{
	return XLevel()->Collision.IsOverlapping(this, other);
}

UObject* UActor::Trace(vec3& hitLocation, vec3& hitNormal, const vec3& traceEnd, const vec3& traceStart, bool bTraceActors, const vec3& extent)
{
	TraceFlags flags;
	flags.movers = true;
	flags.world = true;
	if (bTraceActors)
	{
		flags.pawns = true;
		flags.others = true;
		flags.onlyProjectiles = true;
	}

	CollisionHit hit = XLevel()->Collision.TraceFirstHit(traceStart, traceEnd, this, extent, flags);
	if (!hit.Actor)
	{
		// With nothing hit the original gives a location and normal of zero
		// (TraceConsole), not the line's end
		hitNormal = vec3(0.0f);
		hitLocation = vec3(0.0f);
		return nullptr;
	}
	hitNormal = hit.Normal;
	hitLocation = traceStart + (traceEnd - traceStart) * hit.Fraction;
	return hit.Actor;
}

bool UActor::FastTrace(const vec3& traceEnd, const vec3& traceStart)
{
	return !XLevel()->Collision.TraceAnyHit(traceStart, traceEnd, this, false, true, false);
}

CollisionHit UActor::TryMove(const vec3& delta, bool dryRun, bool isOwnBaseBlocking, CollisionHitList* traced)
{
	// Static and non-movable objects can't move
	if (bStatic() || !bMovable())
	{
		CollisionHit hit;
		hit.Fraction = 0.0f;
		return hit;
	}

	// A brush traces nothing; its move is MoveBrush's
	if (Brush() && UObject::TryCast<UBrush>(this))
	{
		CollisionHit hit;
		if (!dryRun && !MoveBrush(delta, Rotation()))
			hit.Fraction = 0.0f;
		return hit;
	}

	// Avoid moving if movement is too small as the physics code doesn't like very small numbers
	if (dot(delta, delta) < 0.00000001f)
		return {};

	CollisionHitList localHits;
	CollisionHitList& hits = traced ? *traced : localHits;
	CollisionHit blockingHit = TraceMove(delta, isOwnBaseBlocking, hits);

	if (dryRun)
		return blockingHit;

	return FinishMove(delta, hits, blockingHit, Location() + delta * blockingHit.Fraction);
}

// ULevel::MoveActor (Engine.dll 0x103990e0, dx-reverse-info/engine-dll.md,
// moving): the move is traced 2 units past its end; one that something
// blocks moves (its length + 2) x the hit's time − 2 along it, so it stops 2
// short of the hit, or does not move at all when that is 2 or less.
vec3 UActor::HeldOffDelta(const vec3& delta)
{
	float size = length(delta);
	return size > 0.0f ? delta * ((size + 2.0f) / size) : delta;
}

float UActor::HeldOffFraction(const vec3& delta, float tracedFraction)
{
	if (tracedFraction >= 1.0f)
		return 1.0f;
	float size = length(delta);
	float dist = (size + 2.0f) * tracedFraction;
	return (size > 0.0f && dist > 2.0f) ? (dist - 2.0f) / size : 0.0f;
}

CollisionHit UActor::TryMoveHeldOff(const vec3& delta)
{
	// A brush traces nothing, so nothing holds it off: its move is MoveBrush's
	if (bStatic() || !bMovable() || (Brush() && UObject::TryCast<UBrush>(this)) || dot(delta, delta) < 0.00000001f)
		return TryMove(delta);

	CollisionHitList hits;
	vec3 traced = HeldOffDelta(delta);
	CollisionHit blockingHit = TraceMove(traced, true, hits);
	vec3 hitLocation = Location() + traced * blockingHit.Fraction;
	blockingHit.Fraction = HeldOffFraction(delta, blockingHit.Fraction);
	return FinishMove(delta, hits, blockingHit, hitLocation);
}

bool UActor::IsPlayerOrProjectile()
{
	if (Collision.PlayerOrProjectile < 0)
		Collision.PlayerOrProjectile = (UObject::TryCast<UPlayerPawn>(this) || UObject::TryCast<UProjectile>(this)) ? 1 : 0;
	return Collision.PlayerOrProjectile != 0;
}

CollisionHit UActor::TraceMove(const vec3& delta, bool isOwnBaseBlocking, CollisionHitList& hits)
{
	// Analyze what we will hit if we move as requested and stop if it is the level or a blocking actor
	CollisionHit blockingHit;
	if (!Brush())
	{
		hits = XLevel()->Collision.Trace(Location(), Location() + delta, CollisionHeight(), CollisionRadius(), bCollideActors(), bCollideWorld(), false);
		if (bCollideWorld() || bBlockActors() || bBlockPlayers())
		{
			for (auto& hit : hits)
			{
				if (hit.Actor)
				{
					// What blocks the move is the original's IsBlockedBy
					// (EncroachBlockedBy): a mover blocks what collides with
					// the world, by its own flag for the actor's kind, so a
					// projectile or a carcass, which block nothing, stops at
					// a closed door or a lift's floor.
					bool isBlocking = EncroachBlockedBy(hit.Actor);

					// We never hit ourselves or anything moving along with us
					if (isBlocking && (isOwnBaseBlocking || !hit.Actor->IsBasedOn(this)) && !IsBasedOn(hit.Actor))
					{
						blockingHit = hit;
						break;
					}
				}
				else
				{
					blockingHit = hit;
					break;
				}
			}
		}
	}
	return blockingHit;
}

CollisionHit UActor::FinishMove(const vec3& delta, const CollisionHitList& hits, CollisionHit blockingHit, const vec3& hitLocation)
{
	bool useBlockPlayers = IsPlayerOrProjectile();

	vec3 actuallyMoved = delta * blockingHit.Fraction;

	XLevel()->Collision.RemoveFromCollision(this);
	Location() += actuallyMoved;
	XLevel()->Collision.AddToCollision(this);

	for (size_t i = 0; i < BasedActors.size(); )
	{
		UActor* basedActor = BasedActors[i];
		basedActor->TryMove(actuallyMoved, false, false);
		// UnrealScript events triggered in TryMove can call methods such as SetBase or Destroy, so need to guard while iterating.
		if (i < BasedActors.size() && BasedActors[i] == basedActor)
			i++;
	}

	// Send bump notification if we hit an actor
	if (blockingHit.Actor)
	{
		if (!blockingHit.Actor->IsBasedOn(this))
		{
			CallEvent(blockingHit.Actor, EventName::Bump, { ExpressionValue::ObjectValue(this) });
			CallEvent(this, EventName::Bump, { ExpressionValue::ObjectValue(blockingHit.Actor) });
		}
	}
	else if (blockingHit.Fraction < 1.0f)
	{
		// A move the level stopped: Deus Ex's BumpWall to the actor, with
		// where the trace met the level and the normal (ULevel::MoveActor,
		// dx-reverse-info/engine-dll.md, moving)
		CallEvent(this, EventName::BumpWall, { ExpressionValue::VectorValue(hitLocation), ExpressionValue::VectorValue(blockingHit.Normal) });
	}

	// Send touch notifications for anything we crossed while moving
	for (auto& hit : hits)
	{
		if (hit.Fraction >= blockingHit.Fraction)
			break;

		if (hit.Actor && !hit.Actor->IsBasedOn(this) && !IsBasedOn(hit.Actor) && bCollideActors() && hit.Actor->bCollideActors())
		{
			// We can't touch stuff we are blocked by
			bool isBlocking;
			if (useBlockPlayers || hit.Actor->IsPlayerOrProjectile())
				isBlocking = hit.Actor->bBlockPlayers() && bBlockPlayers();
			else
				isBlocking = hit.Actor->bBlockActors() && bBlockActors();
			if (!isBlocking)
				Touch(hit.Actor);
		}
	}

	// Untouch everything we aren't overlapping anymore
	for (const auto actor : Touching())
		if (actor && !IsOverlapping(actor))
			UnTouch(actor);

	UpdateActorZone();

	return blockingHit;
}

// ULevel::MoveActor for a brush (Engine.dll 0x103990e0): it traces nothing.
// What stands on it moves first, by the delta and round the brush by its
// turn's yaw (to a multiple of 4), its own yaw turned with it -- and stays
// moved when the brush is then stopped. Then the encroachment check at the
// new place and rotation; passed, the brush moves and turns at once.
bool UActor::MoveBrush(const vec3& delta, const Rotator& rotation)
{
	if (bStatic() || !bMovable())
		return false;

	Rotator oldRotation = Rotation();
	bool turned = rotation != oldRotation;
	bool moved = std::abs(delta.x) >= 0.0001f || std::abs(delta.y) >= 0.0001f || std::abs(delta.z) >= 0.0001f;
	if (!moved && !turned)
		return true;

	if (StandingCount() > 0)
	{
		int yaw = rotation.Yaw - oldRotation.Yaw;
		Coords yawRot = Coords::InverseRotation(Rotator(0, (rotation.Yaw & 0xfffc) - (oldRotation.Yaw & 0xfffc), 0));
		vec3 baseLoc = Location();
		Array<UActor*> riders = BasedActors;
		for (UActor* rider : riders)
		{
			if (!rider || rider->ActorBase() != this || rider->bDeleteMe())
				continue;
			vec3 riderMove = delta;
			if (turned)
				riderMove += (baseLoc + yawRot * (rider->Location() - baseLoc)) - rider->Location();
			rider->TryMove(riderMove, false, false);
			if (yaw != 0)
			{
				rider->SetRotation(rider->Rotation() + Rotator(0, yaw, 0));
				if (UPawn* pawn = UObject::TryCast<UPawn>(rider))
					pawn->ViewRotation().Yaw += yaw;
			}
		}
	}

	if (CheckEncroachment(Location() + delta, rotation))
		return false;

	XLevel()->Collision.RemoveFromCollision(this);
	Location() += delta;
	Rotation() = rotation;
	XLevel()->Collision.AddToCollision(this);

	for (const auto actor : Touching())
		if (actor && !IsOverlapping(actor))
			UnTouch(actor);

	UpdateActorZone();
	return true;
}

// AActor::moveSmooth (Engine.dll 0x103c9020, dx-reverse-info/engine-dll.md,
// moving), the script's MoveSmooth, a mover's push of what blocks it and a
// client's smoothing: a held-off move; blocked, a slide along what it met by
// the rest of the move, unless that turns back; a second wall gets HitWall,
// TwoWallAdjust and a third move. The one hit serves all three moves, and a
// move under 0.0001 on each axis leaves it as it was, so a slide of nothing
// after a blocked move still sends HitWall with the first move's hit. The
// answer is the first move's: whether the actor moved at all.
CollisionHit UActor::TryMoveSmooth(const vec3& delta)
{
	auto nothing = [](const vec3& v) { return std::abs(v.x) < 0.0001f && std::abs(v.y) < 0.0001f && std::abs(v.z) < 0.0001f; };
	CollisionHit first = TryMoveHeldOff(delta);
	if (first.Fraction < 1.0f && !bDeleteMe())
	{
		vec3 adjusted = (delta - first.Normal * dot(delta, first.Normal)) * (1.0f - first.Fraction);
		if (dot(adjusted, delta) >= 0.0f)
		{
			vec3 oldHitNormal = first.Normal;
			float size = length(delta);
			vec3 desiredDir = size > 0.0f ? delta / size : vec3(0.0f);
			CollisionHit hit = nothing(adjusted) ? first : TryMoveHeldOff(adjusted);
			if (hit.Fraction < 1.0f && !bDeleteMe())
			{
				CallEvent(this, EventName::HitWall, { ExpressionValue::VectorValue(hit.Normal), ExpressionValue::ObjectValue(hit.Actor ? hit.Actor : Level()) });
				if (bDeleteMe())
					return first;
				TwoWallAdjust(desiredDir, adjusted, hit.Normal, oldHitNormal, hit.Fraction);
				if (!nothing(adjusted))
					TryMoveHeldOff(adjusted);
			}
		}
	}
	return first;
}
