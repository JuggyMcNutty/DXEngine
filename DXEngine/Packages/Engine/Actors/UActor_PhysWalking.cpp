
#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"
#include "Packages/Engine/Actors/Decoration/UDecoration.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Engine.h"

void UActor::TickWalking(float elapsed, int iterations)
{
	// Deus Ex's pawns walk as the original's do.
	if (UPawn* deusExPawn = UObject::TryCast<UPawn>(this))
		deusExPawn->DeusExPhysWalking(elapsed, iterations);
}

namespace
{
	// The direction calcVelocity is given: the acceleration itself where
	// each axis compares equal to 0 (X87::Equal, a NaN axis too), else
	// Core's SafeNormal of it, none under a squared length of 1e-8, so a
	// pawn whose acceleration is shorter than 0.0001 turns toward none
	// (0x103ca5fc-0x103ca63c)
	vec3 AccelDir(const vec3& acceleration)
	{
		return X87::Equal3(acceleration, vec3(0.0f)) ? acceleration : X87::SafeNormal(acceleration);
	}

	// A wall too steep to walk up: hit, its normal's Z over 0.01 and under
	// 0.7.
	bool SlopedWall(const CollisionHit& hit)
	{
		return hit.Fraction < 1.0f && hit.Normal.z > 0.01f && hit.Normal.z < 0.7f;
	}
}

// APawn::physWalking (Engine.dll 0x103ca540; dx-reverse-info/engine-dll.md,
// moving: walking), Deus Ex's, a whole frame at once (TickPhysics). After
// the ladder check, the speed, level (calcVelocity, braking), and the move,
// with the zone's velocity for a player and for another pawn in a zone moving
// over 300; then steps of it, up to 8 counting a landing fall's (iterations)
// -- a player's, and another pawn's whose move over the time left is over
// 20 units, of at most 0.05 s, half the time left -- each moved held off
// what it meets and stepped up over what blocks it,
// and the floor under it measured. A floor is stood on (floated 2.1 over
// where the trace stops), stepped down onto, or, sloped where the zone's
// ground friction x its normal's Z is under 3.3, slid down by gravity x the
// frame's time squared / (2 x that friction, at least 0.5). With no floor,
// or one steeper than 0.7, a pawn that cannot jump -- or whose MayFall says
// so, asked once if its state probes it -- stops where the frame began, its
// move given up (MoveTimer -1); any other gets Falling and falls the rest of
// the frame, unless its script made it do something else: a bird that flies
// moves on along its move. A step that took it into water swims the rest.
void UPawn::DeusExPhysWalking(float deltaTime, int iterations)
{
	if (DeusExLadder(deltaTime))
		return;
	if (HasLeftWorld())
		return;

	UZoneInfo* zone = Region().Zone;
	if (!zone)
		return;
	bool player = UObject::TryCast<UPlayerPawn>(this) != nullptr;

	Velocity().z = 0.0f;
	Acceleration().z = 0.0f;
	DeusExCalcVelocity(AccelDir(Acceleration()), deltaTime, GroundSpeed(), zone->ZoneGroundFriction(), false, true, false);

	vec3 desiredMove = Velocity();
	if (player || dot(zone->ZoneVelocity(), zone->ZoneVelocity()) > 90000.0f)
		desiredMove += zone->ZoneVelocity() * (25.0f * deltaTime);
	desiredMove.z = 0.0f;

	vec3 gravDir(0.0f, 0.0f, zone->ZoneGravity().z > 0.0f ? 1.0f : -1.0f);
	float reach = MaxStepHeight() + 2.0f;
	vec3 down = gravDir * reach;
	OldLocation() = Location();
	bJustTeleported() = false;

	TraceFlags levelFlags; // the original's flags 6
	levelFlags.movers = true;
	levelFlags.world = true;
	TraceFlags colliding = levelFlags; // the original's flags 23
	colliding.pawns = true;
	colliding.others = true;
	vec3 extent(CollisionRadius(), CollisionRadius(), CollisionHeight());

	float remaining = deltaTime;
	bool mayFallAsked = false;
	while (remaining > 0.0f && iterations < 8)
	{
		iterations++;
		float timeTick = remaining;
		if (remaining > 0.05f && (player || dot(desiredMove, desiredMove) * remaining * remaining > 400.0f))
			timeTick = std::min(remaining * 0.5f, 0.05f);
		remaining -= timeTick;
		vec3 delta = desiredMove * timeTick;
		vec3 stepStart = Location();
		vec3 stepMove = delta;

		CollisionHit floor;
		bool measure = true;
		// A step of nothing, each axis under the double 0.0001
		// (X87::NearlyZero3; 0x103ca93e-0x103ca9d7), is the last
		if (X87::NearlyZero3(delta))
		{
			// Still: a pawn on the same base, a line 20 down from its bottom
			// meeting the floor 4.1 to 4.6 away, is left as it is.
			remaining = 0.0f;
			bHitSlopedWall() = false;
			vec3 bottom = Location() - vec3(0.0f, 0.0f, CollisionHeight());
			CollisionHit under = XLevel()->Collision.TraceFirstHit(bottom, bottom - vec3(0.0f, 0.0f, 20.0f), this, vec3(0.0f), levelFlags);
			float underDist = under.Fraction * 20.0f;
			if (under.Actor && under.Actor == ActorBase() && underDist >= 4.1f && underDist <= 4.6f)
				measure = false;
		}
		else
		{
			// Last step met a wall too steep to walk up: a box along the move
			// (its length + 4, at least 30) meeting one again turns the move
			// along it; a player's three lines from a step's height over its
			// feet, ahead and 45 degrees to each side, 100 long, look for one.
			// Each writes the hit the move is then handed (0x103cb0d8): a move
			// along the wall that comes to nothing keeps the wall's, which is
			// stepped up.
			CollisionHit hit;
			if (bHitSlopedWall())
			{
				float size = length(delta);
				vec3 dir = delta / size;
				hit = XLevel()->Collision.TraceFirstHit(Location(), Location() + dir * std::max(size + 4.0f, 30.0f), this, extent, levelFlags);
				bHitSlopedWall() = SlopedWall(hit);
				if (bHitSlopedWall())
				{
					vec3 normal = X87::SafeNormal(vec3(hit.Normal.x, hit.Normal.y, 0.0f));
					delta -= normal * dot(delta, normal);
				}
				else if (player)
				{
					vec3 eye(Location().x, Location().y, Location().z - CollisionHeight() + MaxStepHeight() + 4.0f);
					vec3 dirs[3] = { dir, X87::SafeNormal(vec3(dir.y + dir.x, dir.y - dir.x, dir.z)), X87::SafeNormal(vec3(dir.x - dir.y, dir.x + dir.y, dir.z)) };
					for (const vec3& d : dirs)
					{
						hit = XLevel()->Collision.TraceFirstHit(eye, eye + d * 100.0f, this, vec3(0.0f), levelFlags);
						bHitSlopedWall() = SlopedWall(hit);
						if (bHitSlopedWall())
							break;
					}
				}
				TryMoveHeldOff(delta, hit);
			}
			else
			{
				hit = TryMoveHeldOff(delta);
				bHitSlopedWall() = SlopedWall(hit);
			}
			if (bDeleteMe())
				return;

			if (hit.Fraction < 1.0f)
			{
				DeusExStepUp(gravDir, X87::SafeNormal(delta), delta * (1.0f - hit.Fraction), hit);
				if (bDeleteMe())
					return;
				if (Physics() == PHYS_Falling)
				{
					// The step up left it falling: what the step did not move
					// across goes back to the time left; Falling, then a fall,
					// or a flight straight up at AirSpeed.
					float moved = length(vec2(Location().x - stepStart.x, Location().y - stepStart.y));
					remaining += (1.0f - std::min(moved / length(stepMove), 1.0f)) * timeTick;
					CallEvent(this, EventName::Falling);
					if (Physics() == PHYS_Falling)
					{
						if (remaining > 0.01f)
							TickFalling(remaining, iterations);
					}
					else if (Physics() == PHYS_Flying)
					{
						Velocity() = vec3(0.0f, 0.0f, AirSpeed());
						Acceleration() = vec3(0.0f, 0.0f, AccelRate());
						if (remaining > 0.01f)
							TickFlying(remaining);
					}
					return;
				}
			}
			if (Physics() == PHYS_Swimming)
			{
				DeusExStartSwimming(Velocity(), timeTick, remaining, iterations);
				return;
			}
		}
		if (!measure)
			continue;

		// The floor: a box of the pawn's size down MaxStepHeight + 2, the first
		// hit of a pawn, a mover, the level or another actor, whether it
		// blocks the pawn or not (SingleLineCheck with flags 23, Engine.dll
		// 0x103cb5f3). None, or the same base within 2.4, is stood on, lifted
		// to 2.1 from nearer than 1.9; any other the pawn steps down onto, its
		// base then what the step meets: over a flat actor that blocks
		// nothing, a credit chip, the floor beneath, which the step's move
		// bumps (BumpWall).
		floor = XLevel()->Collision.TraceFirstHit(Location(), Location() + down, this, extent, colliding);
		UActor* floorActor = floor.Fraction < 1.0f ? (floor.Actor ? floor.Actor : Level()) : nullptr;
		float floorDist = reach * floor.Fraction;
		if (floor.Fraction >= 1.0f || (floorActor == ActorBase() && floorDist <= 2.4f))
		{
			if (floorDist < 1.9f)
			{
				vec3 normal = floor.Normal;
				TryMoveHeldOff(vec3(0.0f, 0.0f, 2.1f - floorDist));
				floor.Fraction = 0.0f;
				floor.Normal = normal;
			}
		}
		else
		{
			floor = TryMoveHeldOff(down);
			if (bDeleteMe())
				return;
			floorActor = floor.Fraction < 1.0f ? (floor.Actor ? floor.Actor : Level()) : nullptr;
			if (floorActor != ActorBase())
				SetSupportBase(floorActor);
			if (Physics() == PHYS_Swimming)
			{
				DeusExStartSwimming(Velocity(), timeTick, remaining, iterations);
				return;
			}
		}

		if (floor.Fraction >= 1.0f || floor.Normal.z < 0.7f)
		{
			// No floor, or one too steep.
			if (bCanJump() && !mayFallAsked && IsEventEnabled(EventName::MayFall))
			{
				mayFallAsked = true;
				CallEvent(this, EventName::MayFall);
			}
			if (!bCanJump())
			{
				Velocity() = vec3(0.0f);
				Acceleration() = vec3(0.0f);
				SetLocation(OldLocation());
				MoveTimer() = -1.0f;
				return;
			}
			if (floor.Fraction < 1.0f)
				bHitSlopedWall() = true;
			float stepSize = length(stepMove);
			if (stepSize == 0.0f)
			{
				remaining = 0.0f;
			}
			else
			{
				float moved = length(vec2(Location().x - stepStart.x, Location().y - stepStart.y));
				remaining += (1.0f - std::min(moved / stepSize, 1.0f)) * timeTick;
			}
			Velocity().z = 0.0f;
			CallEvent(this, EventName::Falling);
			if (Physics() == PHYS_Walking)
				SetPhysics(PHYS_Falling, nullptr);
			if (Physics() == PHYS_Falling)
			{
				float velZ = Velocity().z;
				if (!bJustTeleported() && deltaTime > remaining)
					Velocity() = (Location() - OldLocation()) / (deltaTime - remaining);
				Velocity().z = velZ;
				if (remaining > 0.01f)
					TickFalling(remaining, iterations);
				return;
			}
			TryMoveHeldOff(desiredMove * remaining);
			remaining = 0.0f;
		}
		else if (floor.Normal.z < 1.0f && floor.Normal.z * zone->ZoneGroundFriction() < 3.3f)
		{
			float friction = std::max(zone->ZoneGroundFriction(), 0.5f);
			vec3 slideMove = zone->ZoneGravity() * (deltaTime / (2.0f * friction) * deltaTime);
			vec3 slide = slideMove - floor.Normal * dot(slideMove, floor.Normal);
			if (dot(slide, slideMove) >= 0.0f)
				TryMoveHeldOff(slide);
			if (bDeleteMe())
				return;
			if (Physics() == PHYS_Swimming)
			{
				DeusExStartSwimming(Velocity(), timeTick, remaining, iterations);
				return;
			}
		}
	}

	if (!bJustTeleported())
		Velocity() = (Location() - OldLocation()) / deltaTime;
	Velocity().z = 0.0f;
}

// APawn::calcVelocity (Engine.dll 0x103cd7a0; dx-reverse-info/engine-dll.md,
// moving: the speed), as Deus Ex's walking (GroundSpeed, the zone's ground
// friction, braking), swimming (WaterSpeed, its fluid friction, as a fluid,
// buoyant) and flying (AirSpeed, its fluid friction, as a fluid) ask it, in
// the original's float order: a float wherever the original stores one, the
// x87's own sum, product or quotient (Round) where it does not. It turns and
// brakes with the larger of the friction and the fluid's 1. With no
// acceleration a braking pawn slows in slices of 0.03 s, to the slices'
// velocities that still point the old way weighted by their time, and stops
// under 10 or turned about. Otherwise the acceleration is cut to AccelRate
// (0.3 of it for a walking player -- Deus Ex's player walks whenever it
// swims) and the velocity turns toward it. Then the fluid's friction, the
// acceleration and the buoyancy; then the speed held to maxSpeed (x
// DesiredSpeed but for a player), a walking player's to 0.3 of it (0.6 in a
// net game), brought down to that no faster than the friction allows.
void UPawn::DeusExCalcVelocity(const vec3& accelDir, float deltaTime, float maxSpeed, float friction, bool fluid, bool brake, bool buoyant)
{
	auto R = X87::Round;
	// The squares summed X's and Y's first, then Z's, as Core's SizeSquared
	// and Size sum them (Core.dll 0x10117b70, 0x10117b30); a dot of two
	// velocities Z's product first, then Y's, then X's (0x103cd9c6)
	auto sizeSquared = [&](const vec3& v) { return R(R(R((double)v.x * v.x) + R((double)v.y * v.y)) + R((double)v.z * v.z)); };
	auto dotZYX = [&](const vec3& a, const vec3& b) { return R(R(R((double)a.z * b.z) + R((double)a.y * b.y)) + R((double)a.x * b.x)); };

	// The larger of the friction and the fluid's 1 or 0, a NaN friction
	// taken (0x103cd7bb)
	float fluidFactor = fluid ? 1.0f : 0.0f;
	float turning = X87::Under(fluidFactor, friction) ? friction : fluidFactor;
	bool player = UObject::TryCast<UPlayerPawn>(this) != nullptr;
	bool walkingPlayer = player && bIsWalking();
	vec3& velocity = Velocity();
	vec3& acceleration = Acceleration();

	// Braking with no acceleration, each axis equal to 0 to the x87's
	// compare, a NaN axis too (0x103cd886)
	if (brake && X87::Equal3(acceleration, vec3(0.0f)))
	{
		// Each slice the velocity less (twice itself x the slice) x the
		// friction; one still pointing the old way adds itself x the slice x
		// the x87's reciprocal of the tick (0x103cda14, 0x103cdb44). The time
		// left is the float less the double 0.03, its compare against 0.03
		// unrounded, the float the next slice's (0x103cd8fc, 0x103cda5e).
		vec3 oldVelocity = velocity;
		vec3 sum(0.0f);
		double overTick = R(1.0 / deltaTime);
		auto slice = [&](float time)
		{
			velocity = velocity - ((velocity + velocity) * time) * turning;
			if (dotZYX(oldVelocity, velocity) > 0.0)
			{
				vec3 share = velocity * time;
				sum = vec3((float)R(share.x * overTick) + sum.x, (float)R(share.y * overTick) + sum.y, (float)R(share.z * overTick) + sum.z);
			}
		};
		double left = deltaTime;
		float leftSlice = deltaTime;
		while (left > 0.03)
		{
			slice(0.03f);
			left = R(leftSlice - 0.03);
			leftSlice = (float)left;
		}
		slice(leftSlice);
		velocity = sum;
		if (X87::Under(dotZYX(oldVelocity, velocity), 0.0) || X87::Under(sizeSquared(velocity), 100.0))
			velocity = vec3(0.0f);
	}
	else
	{
		// The speed the root of the squares' sum stored as a double, a float
		// (0x103cdc53). An acceleration whose square is over AccelRate's --
		// a walking player's x the double 0.09 -- is cut to the direction x
		// AccelRate, a walking player's then x 0.3 (0x103cdc7f-0x103cdd93);
		// the velocity turns by (itself less the direction x its speed) x the
		// tick, then x the friction (0x103cdd99-0x103cde67).
		float speed = (float)R(std::sqrt(sizeSquared(velocity)));
		double accelSquared = sizeSquared(acceleration);
		float accelRate = AccelRate();
		if (walkingPlayer)
		{
			if (X87::Under(R(R((double)accelRate * accelRate) * 0.09), accelSquared))
				acceleration = (accelDir * accelRate) * 0.3f;
		}
		else if (X87::Under(R((double)accelRate * accelRate), accelSquared))
		{
			acceleration = accelDir * accelRate;
		}
		velocity = velocity - ((velocity - accelDir * speed) * deltaTime) * turning;
	}

	// x 1 less the fluid's friction x the tick, that factor unrounded
	// (0x103cde9d), plus the acceleration x the tick
	double keep = R(1.0 - R(R((double)fluidFactor * deltaTime) * friction));
	velocity = acceleration * deltaTime + vec3((float)R(keep * velocity.x), (float)R(keep * velocity.y), (float)R(keep * velocity.z));

	if (!player)
		maxSpeed *= DesiredSpeed();

	if (buoyant)
	{
		// (The tick x the gravity) x (1 - Buoyancy / Mass), that a float
		// (0x103cdf3a-0x103cdfb1). The mass as it is; a massless pawn would
		// make this 0/0, and takes 1.
		float mass = Mass() != 0.0f ? Mass() : 1.0f;
		float lift = (float)R(1.0 - R((double)Buoyancy() / mass));
		velocity = (Region().Zone->ZoneGravity() * deltaTime) * lift + velocity;
	}

	if (walkingPlayer)
	{
		// A walking player's held to 0.3 of maxSpeed, 0.6 in a net game: its
		// square x the double 0.09 (0.36) against the velocity's, a float;
		// the velocity over its size by Core's reciprocal (FVector's
		// operator/, Core.dll 0x10117800), then times the more of that speed
		// and (1 - 2 x the friction x the tick) x the size, as the x87
		// compares them (0x103cdfb4-0x103ce176)
		bool net = Level()->NetMode() != NM_Standalone;
		if (X87::Under(R(R((double)maxSpeed * maxSpeed) * (net ? 0.36 : 0.09)), (float)sizeSquared(velocity)))
		{
			float size = (float)R(std::sqrt(sizeSquared(velocity)));
			double over = R(1.0 / size);
			velocity = vec3((float)R(over * velocity.x), (float)R(over * velocity.y), (float)R(over * velocity.z));
			double slowed = R(R(1.0 - R(R((double)turning * deltaTime) * 2.0)) * size);
			double walkSpeed = R((double)maxSpeed * (net ? 0.6f : 0.3f));
			velocity = velocity * (X87::Under(walkSpeed, slowed) ? (float)slowed : (float)walkSpeed);
			return;
		}
	}
	// Held to maxSpeed: its square against the velocity's, a float; Core's
	// SafeNormal of the velocity times it (0x103ce090, 0x103ce1a6)
	if (X87::Under(R((double)maxSpeed * maxSpeed), (float)sizeSquared(velocity)))
		velocity = X87::SafeNormal(velocity) * maxSpeed;
}
