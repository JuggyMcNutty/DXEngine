
#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"
#include "Packages/Engine/Actors/UProjectile.h"
#include "Packages/Engine/Actors/Decoration/UDecoration.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Engine.h"

// AActor::physProjectile (Engine.dll 0x103d4600; dx-reverse-info/engine-dll.md,
// moving: a projectile's move), a whole frame at once: steps of the time
// left, up to 8 counting the caller's, each the velocity less the zone's
// fluid friction (in water) plus the acceleration x the time left, a
// Projectile held to its MaxSpeed, and one held-off move by the velocity x
// the frame's whole time, not the step's. A move that stops, the actor
// living and not teleported, gets HitWall with the hit's actor, the level's
// for the world (a mover's too: nothing else blocks a projectile, which
// blocks nothing itself, EncroachBlockedBy); with bBounce the step's time
// not moved goes back to the time left, twice at most, and an actor the
// event set falling falls it. Without bBounce the velocity is then the
// frame's move over the frame.
void UActor::TickProjectile(float deltaTime, int iterations)
{
	if (Region().ZoneNumber == 0)
	{
		Destroy();
		return;
	}

	UProjectile* projectile = UObject::TryCast<UProjectile>(this);
	OldLocation() = Location();
	bJustTeleported() = false;

	float remaining = deltaTime;
	int bounces = 0;
	while (remaining > 0.0f && iterations < 8)
	{
		iterations++;
		UZoneInfo* zone = Region().Zone;
		if (zone->bWaterZone())
			Velocity() = Velocity() * (1.0f - zone->ZoneFluidFriction() * remaining * 0.2f);
		Velocity() = Velocity() + Acceleration() * remaining;
		float step = remaining;
		remaining = 0.0f;
		if (projectile)
		{
			float maxSpeed = projectile->MaxSpeed();
			// Core's SafeNormal of the velocity times it (0x103d4821)
			if (dot(Velocity(), Velocity()) > maxSpeed * maxSpeed)
				Velocity() = X87::SafeNormal(Velocity()) * maxSpeed;
		}

		CollisionHit hit = TryMoveHeldOff(Velocity() * deltaTime);
		if (hit.Fraction < 1.0f && !bDeleteMe() && !bJustTeleported())
		{
			CallEvent(this, EventName::HitWall, { ExpressionValue::VectorValue(hit.Normal), ExpressionValue::ObjectValue(hit.Actor ? hit.Actor : Level()) });
			if (bDeleteMe())
				return;
			if (bBounce())
			{
				if (bounces < 2)
					remaining = (1.0f - hit.Fraction) * step;
				bounces++;
				if (Physics() == PHYS_Falling)
					TickFalling(remaining, iterations);
			}
		}
		if (bDeleteMe())
			return;
	}

	if (!bBounce() && !bJustTeleported())
		Velocity() = (Location() - OldLocation()) / deltaTime;
}
