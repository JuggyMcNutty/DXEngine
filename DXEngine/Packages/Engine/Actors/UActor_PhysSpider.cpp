#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Resources/Level/ULevel.h"

void UActor::TickSpider(float elapsed)
{
	// A pawn climbs as the original's does; no other actor has the physics
	// (APawn::performPhysics alone calls physSpider).
	if (UPawn* pawn = UObject::TryCast<UPawn>(this))
		pawn->DeusExPhysSpider(elapsed, 0);
}

namespace
{
	// The original's checks for a floor: a box of the pawn's size, against
	// the world and movers alone (flags 6), traced with no actor of its own.
	TraceFlags SpiderFlags()
	{
		TraceFlags flags;
		flags.movers = true;
		flags.world = true;
		return flags;
	}
}

// APawn::physSpider (Engine.dll 0x103d6370; dx-reverse-info/engine-dll.md,
// moving: climbing), the original's, which no stock script sets (bugs.md
// PHYS.26). The speed first: with no acceleration (each axis equal to 0, a
// NaN axis too) the velocity brakes by twice itself x the time x the zone's
// ground friction, and stops where that turned it about; otherwise the
// acceleration is cut to AccelRate along Core's SafeNormal of it and the
// velocity turned toward that direction at its own size. Then the
// acceleration x the time, the speed held to GroundSpeed x DesiredSpeed. The
// move is the velocity, with the zone's velocity where that is over 300.
// The pawn holds to its Floor, the normal of the surface it climbs: one
// MaxStepHeight + 4 beyond it, a box traced from the pawn, or with no Floor,
// is looked for again (findNewFloor). It turns to stand on it (the floor's
// rotation, its pitch a quarter turn down, no roll); a floor steeper than
// 0.6 that the acceleration points out of by more than 0.9 lets it fall.
// The move is made along the floor in steps of the time, as a walk's (at
// most 0.05 s, half the time left, up to 8), each pressed onto the floor
// after it; a hit with a ceiling (its normal down) that the move heads into
// is a wall, one it heads along drops it from a floor it heads out of by
// more than 0.7; any other hit is the new floor, a wall where the move
// heads into both it and the floor, or into their mean by more than 0.9.
// What blocks the step is stepped up from the floor (stepUp, its gravity
// the floor's opposite). The velocity is then the tick's move over the tick.
void UPawn::DeusExPhysSpider(float deltaTime, int iterations)
{
	UZoneInfo* zone = Region().Zone;
	if (!zone)
		return;

	// The speed (0x103d639d-0x103d66dd), its size kept as appSqrt's double
	vec3 accelDir;
	if (X87::Equal3(Acceleration(), vec3(0.0f)))
	{
		accelDir = Acceleration();
		vec3 oldVelocity = Velocity();
		Velocity() = Velocity() - Velocity() * 2.0f * deltaTime * zone->ZoneGroundFriction();
		if (dot(oldVelocity, Velocity()) < 0.0f)
			Velocity() = Acceleration();
	}
	else
	{
		accelDir = X87::SafeNormal(Acceleration());
		double velSize = std::sqrt((double)Velocity().x * Velocity().x + (double)Velocity().y * Velocity().y + (double)Velocity().z * Velocity().z);
		if (dot(Acceleration(), Acceleration()) > AccelRate() * AccelRate())
			Acceleration() = accelDir * AccelRate();
		Velocity() = Velocity() - (Velocity() - accelDir * (float)velSize) * deltaTime * zone->ZoneGroundFriction();
	}
	Velocity() += Acceleration() * deltaTime;
	float maxSpeed = GroundSpeed() * DesiredSpeed();
	iterations++;
	if (dot(Velocity(), Velocity()) > maxSpeed * maxSpeed)
		Velocity() = X87::SafeNormal(Velocity()) * maxSpeed;

	// The move, the zone's velocity added where it is over 300 (0x103d66dd-
	// 0x103d6752); its direction the move over its size, no direction asked
	// of Core: a pawn standing still, in still air, has none (0/0)
	vec3 zoneVelocity = dot(zone->ZoneVelocity(), zone->ZoneVelocity()) > 90000.0f ? zone->ZoneVelocity() : vec3(0.0f);
	vec3 desiredMove = zoneVelocity + Velocity();
	float moveSize = (float)std::sqrt((double)desiredMove.x * desiredMove.x + (double)desiredMove.y * desiredMove.y + (double)desiredMove.z * desiredMove.z);
	vec3 desiredDir = desiredMove * (1.0f / moveSize);

	// The floor: none (each axis under the double 0.0001), or none a box
	// MaxStepHeight + 4 beyond it meets, is looked for again
	// (0x103d680a-0x103d6a54)
	vec3 extent(CollisionRadius(), CollisionRadius(), CollisionHeight());
	vec3 gravDir = Floor() * -1.0f;
	vec3 down = gravDir * (MaxStepHeight() + 4.0f);
	DesiredRotation() = Rotation();
	bool lookForFloor = X87::NearlyZero3(Floor());
	if (!lookForFloor)
		lookForFloor = XLevel()->Collision.TraceFirstHit(Location(), Location() + down, this, extent, SpiderFlags()).Fraction == 1.0f;
	if (lookForFloor)
	{
		if (!DeusExFindNewFloor(Location(), deltaTime, deltaTime, iterations))
			return;
		gravDir = Floor() * -1.0f;
		down = gravDir * (MaxStepHeight() + 4.0f);
	}

	// Turned to stand on it (FVector::Rotation of the floor, its pitch a
	// quarter turn down, no roll); a steep floor the acceleration points out
	// of lets go (0x103d6a5d-0x103d6b38)
	DesiredRotation() = Rotator::FromVector(Floor());
	DesiredRotation().Roll = 0;
	DesiredRotation().Pitch -= 16384;
	if ((double)Floor().z < 0.6 && (double)dot(accelDir, Floor()) > 0.9)
	{
		Floor() = vec3(0.0f);
		CallEvent(this, EventName::Falling);
		SetPhysics(PHYS_Falling, nullptr);
		TickFalling(deltaTime, iterations);
		return;
	}

	// Along the floor: the direction less its part into the floor, Core's
	// SafeNormal of that, at the move's size; the direction as it was kept
	// for the hits' tests
	vec3 headDir = desiredDir;
	desiredDir = X87::SafeNormal(desiredDir - Floor() * dot(desiredDir, Floor()));
	OldLocation() = Location();
	bJustTeleported() = false;
	vec3 stepMove = desiredDir * moveSize;

	float remaining = deltaTime;
	while (remaining > 0.0f && iterations < 8)
	{
		iterations++;
		float timeTick = remaining;
		if ((double)remaining > 0.05)
			timeTick = std::min(remaining * 0.5f, 0.05f);
		remaining -= timeTick;
		vec3 delta = stepMove * timeTick;

		// A step of nothing (each axis under the double 0.0001) is not
		// made; any other is, the pawn turned to DesiredRotation as it goes
		// (MoveActor with that rotation)
		CollisionHit hit;
		if (!X87::NearlyZero3(delta))
		{
			hit = TryMoveHeldOff(delta);
			Rotation() = DesiredRotation();
			if (bDeleteMe())
				return;
			if (hit.Fraction < 1.0f)
			{
				if (!(hit.Normal.z >= 0.0f))
				{
					// A ceiling (0x103d7060): headed into, a wall; headed
					// along, a floor it heads out of by more than 0.7 lets
					// go, the velocity the move so far over its time
					if (dot(headDir, hit.Normal) < 0.0f)
					{
						FireHitWall(hit);
					}
					else if ((double)dot(headDir, Floor()) > 0.7)
					{
						CallEvent(this, EventName::Falling);
						if (Physics() == PHYS_Spider)
							SetPhysics(PHYS_Falling, nullptr);
						if (Physics() == PHYS_Falling)
						{
							float velZ = Velocity().z;
							if (!bJustTeleported() && deltaTime > remaining)
								Velocity() = (Location() - OldLocation()) * (1.0f / (deltaTime - remaining));
							Velocity().z = velZ;
							if ((double)remaining > 0.005)
								TickFalling(remaining, iterations);
							return;
						}
					}
				}
				else
				{
					// Anything else is the new floor (0x103d6d99-0x103d6f95):
					// a wall first where the move heads into both it and the
					// floor, or into their mean by more than 0.9
					if (dot(headDir, hit.Normal) < 0.0f && dot(headDir, Floor()) < 0.0f)
						FireHitWall(hit);
					else if ((double)dot(X87::SafeNormal(hit.Normal + Floor()), headDir) > 0.9)
						FireHitWall(hit);
					Floor() = hit.Normal;
					gravDir = Floor() * -1.0f;
					down = gravDir * (MaxStepHeight() + 4.0f);
				}
				DeusExStepUp(gravDir, X87::SafeNormal(delta), delta * (1.0f - hit.Fraction), hit);
				if (bDeleteMe())
					return;
				if (Physics() == PHYS_Falling)
				{
					if ((double)remaining > 0.005)
						TickFalling(remaining, iterations);
					return;
				}
			}
		}

		// Pressed onto the floor (0x103d7205): none there is looked for
		// again from where the tick began; one met is the floor, and its
		// actor the base but for a pawn
		hit = TryMoveHeldOff(down);
		if (bDeleteMe())
			return;
		if (hit.Fraction == 1.0f)
		{
			if (!DeusExFindNewFloor(OldLocation(), deltaTime, remaining, iterations))
				return;
			gravDir = Floor() * -1.0f;
			down = gravDir * (MaxStepHeight() + 4.0f);
		}
		else
		{
			Floor() = hit.Normal;
			UActor* floorActor = hit.Actor ? hit.Actor : Level();
			if (floorActor != ActorBase() && !UObject::TryCast<UPawn>(floorActor))
				SetSupportBase(floorActor);
		}
	}

	if (!bJustTeleported())
		Velocity() = (Location() - OldLocation()) / deltaTime;
}

// APawn::findNewFloor (Engine.dll 0x103d5db0): a box of the pawn's size
// traced MaxStepHeight from it down, then to -Y, +Y, -X and +X (the last
// checkFloor's, 0x103d5c80), against the world and movers; the first it
// meets is the floor, its normal. None lets the pawn fall: Falling, and from
// Spider the physics Falling, the velocity the move since oldLocation over
// the time gone, the rest of the time fallen.
bool UPawn::DeusExFindNewFloor(const vec3& oldLocation, float deltaTime, float remainingTime, int iterations)
{
	vec3 extent(CollisionRadius(), CollisionRadius(), CollisionHeight());
	static const vec3 dirs[5] = { vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f) };
	for (const vec3& dir : dirs)
	{
		CollisionHit hit = XLevel()->Collision.TraceFirstHit(Location(), Location() - dir * MaxStepHeight(), this, extent, SpiderFlags());
		if (hit.Fraction < 1.0f)
		{
			Floor() = hit.Normal;
			return true;
		}
	}

	CallEvent(this, EventName::Falling);
	if (Physics() == PHYS_Spider)
		SetPhysics(PHYS_Falling, nullptr);
	if (Physics() == PHYS_Falling)
	{
		float velZ = Velocity().z;
		if (!bJustTeleported() && deltaTime > remainingTime)
			Velocity() = (Location() - oldLocation) / (deltaTime - remainingTime);
		Velocity().z = velZ;
		if ((double)remainingTime > 0.005)
			TickFalling(remainingTime, iterations);
	}
	return false;
}
