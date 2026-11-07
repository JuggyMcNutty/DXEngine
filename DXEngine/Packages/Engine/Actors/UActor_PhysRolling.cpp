
#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Engine.h"

// AActor::physRolling (Engine.dll 0x103d4b80; dx-reverse-info/engine-dll.md,
// moving: rolling), as a pushed decoration rolls. The velocity turned toward
// the acceleration by the zone's ground friction (each a direction by
// Core's SafeNormal, none under 1e-8 squared), slowed by its fluid
// friction and given the acceleration; the move, with the zone's velocity, in
// steps of at most 0.1 s (half the time left), up to 8: each moved, HitWall
// for a wall and slid along it (a second wall through TwoWallAdjust), or with
// bBounce the time given back, twice at most; then 16 units down. A slope
// where the zone's ground friction x its normal's Z is under 3.3 slides it
// down by gravity x the frame's time squared / (2 x that friction, at least
// 0.5). No floor, or one under 0.7: back up, what the step did not move
// across back to the time left, Falling, and a fall for the rest unless its
// script made it do something else, which moves on along the move. A floor
// stood on is its base. The velocity after is the frame's move.
void UActor::TickRolling(float deltaTime, int iterations)
{
	UZoneInfo* zone = Region().Zone;
	if (!zone)
		return;

	float speed = length(Velocity());
	Velocity() -= (X87::SafeNormal(Velocity()) - X87::SafeNormal(Acceleration())) * speed * deltaTime * zone->ZoneGroundFriction();
	Velocity() = Velocity() * (1.0f - deltaTime * zone->ZoneFluidFriction()) + Acceleration() * deltaTime;
	vec3 desiredMove = Velocity() + zone->ZoneVelocity();

	OldLocation() = Location();
	bJustTeleported() = false;
	vec3 down(0.0f, 0.0f, zone->ZoneGravity().z > 0.0f ? 16.0f : -16.0f);

	// The original keeps one hit through every move of the frame.
	CollisionHit hit;
	hit.Fraction = 1.0f;
	auto move = [&](const vec3& delta) { hit = TryMoveHeldOff(delta); return !bDeleteMe(); };

	float remaining = deltaTime;
	int bounces = 0;
	while (remaining > 0.0f && iterations < 8)
	{
		iterations++;
		float timeTick = remaining <= 0.1f ? remaining : std::min(remaining * 0.5f, 0.1f);
		remaining -= timeTick;
		vec3 delta = desiredMove * timeTick;
		vec3 stepMove = delta;
		vec3 stepStart = Location();

		// A step of nothing, each axis under the double 0.0001
		// (X87::NearlyZero3; 0x103d4e90-0x103d4f20), is not moved
		if (!X87::NearlyZero3(delta))
		{
			if (!move(delta))
				return;
			if (hit.Fraction < 1.0f)
			{
				FireHitWall(hit);
				if (bBounce())
				{
					if (bounces < 2)
						remaining += (1.0f - hit.Fraction) * timeTick;
					bounces++;
				}
				else
				{
					vec3 oldDelta = delta;
					vec3 oldHitNormal = hit.Normal;
					delta = (delta - hit.Normal * dot(delta, hit.Normal)) * (1.0f - hit.Fraction);
					if (dot(delta, oldDelta) >= 0.0f)
					{
						if (!move(delta))
							return;
						if (hit.Fraction < 1.0f)
						{
							FireHitWall(hit);
							TwoWallAdjust(X87::SafeNormal(desiredMove), delta, hit.Normal, oldHitNormal, hit.Fraction);
							if (!move(delta))
								return;
						}
					}
				}
			}
		}

		if (!move(down))
			return;
		float floorZ = hit.Normal.z;
		float floorFraction = hit.Fraction;
		if (hit.Fraction < 1.0f && hit.Normal.z < 1.0f && hit.Normal.z * zone->ZoneGroundFriction() < 3.3f)
		{
			float friction = std::max(zone->ZoneGroundFriction(), 0.5f);
			vec3 slide = zone->ZoneGravity() * deltaTime * (1.0f / (friction + friction)) * deltaTime;
			vec3 along = slide - hit.Normal * dot(slide, hit.Normal);
			if (dot(along, slide) >= 0.0f)
			{
				if (!move(along))
					return;
				if (floorZ < hit.Normal.z)
					floorZ = hit.Normal.z;
			}
		}

		if (floorFraction == 1.0f || floorZ < 0.7f)
		{
			if (!move(-(down * floorFraction)))
				return;
			// A step that moved nowhere makes this NaN in the original's
			// arithmetic too: it then neither takes a velocity nor falls.
			float moved = length(vec2(Location().x - stepStart.x, Location().y - stepStart.y)) / length(stepMove);
			if (moved >= 1.0f)
				moved = 1.0f;
			remaining += (1.0f - moved) * timeTick;
			CallEvent(this, EventName::Falling);
			if (Physics() == PHYS_Rolling)
				SetPhysics(PHYS_Falling, nullptr);
			if (Physics() == PHYS_Falling)
			{
				if (!bJustTeleported() && deltaTime > remaining)
					Velocity() = (Location() - OldLocation()) / (deltaTime - remaining);
				Velocity().z = 0.0f;
				if (remaining > 0.005f)
					TickFalling(remaining, iterations);
				return;
			}
			if (!move(desiredMove * remaining))
				return;
		}
		else
		{
			UActor* floorActor = hit.Actor ? hit.Actor : Level();
			if (floorActor != ActorBase())
				SetSupportBase(floorActor);
		}
	}

	if (!bJustTeleported())
		Velocity() = (Location() - OldLocation()) / deltaTime;
}
