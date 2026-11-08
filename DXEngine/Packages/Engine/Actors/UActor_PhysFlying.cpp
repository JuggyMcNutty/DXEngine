
#include "Precomp.h"
#include "UActor.h"
#include "VM/ScriptCall.h"
#include "Packages/Engine/Actors/Decoration/UDecoration.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Math/coords.h"
#include "Utils/Logger.h"
#include "Engine.h"

void UActor::TickFlying(float elapsed)
{
	// Deus Ex's pawns fly as the original's do.
	if (UPawn* deusExPawn = UObject::TryCast<UPawn>(this))
		deusExPawn->DeusExPhysFlying(elapsed);
}

namespace
{
	// The direction calcVelocity is given, as physWalking's (0x103d30da-
	// 0x103d3121): the acceleration where each axis compares equal to 0,
	// else Core's SafeNormal of it, none under 1e-8 squared
	vec3 AccelDir(const vec3& acceleration)
	{
		return X87::Equal3(acceleration, vec3(0.0f)) ? acceleration : X87::SafeNormal(acceleration);
	}

	// A texture's group: its outer's name, as spelled (ladderProbe's test is
	// case-sensitive, the ladder check's not).
	bool InGroup(UTexture* texture, const char* group, bool exact)
	{
		UObject* outer = texture ? texture->Outer() : nullptr;
		if (!outer)
			return false;
		return exact ? outer->Name.ToString() == group : outer->Name == group;
	}
}

// APawn::physFlying (Engine.dll 0x103d3040; dx-reverse-info/engine-dll.md,
// moving: flying): out of the world a flyer that collides with it goes (but a
// player). Otherwise the speed is calcVelocity's at AirSpeed with the zone's
// fluid friction, as a fluid; one move a tick -- with the zone's velocity for
// a player, or another pawn in a zone moving over 300 -- a wall met flying
// level stepped up, any other hit slid along; the velocity the move's own.
// The moves share one hit (0x103d3297), a slide of nothing keeping the
// wall's.
void UPawn::DeusExPhysFlying(float deltaTime)
{
	if (bCollideWorld() && Region().ZoneNumber == 0)
	{
		// The original's line first, on a client too (0x103d309d)
		LogMessage(Name.ToString() + " flew out of the world!");
		if (!bIsPlayer())
			Destroy();
		return;
	}
	UZoneInfo* zone = Region().Zone;
	if (!zone)
		return;

	DeusExCalcVelocity(AccelDir(Acceleration()), deltaTime, AirSpeed(), zone->ZoneFluidFriction(), true, false, false);
	OldLocation() = Location();
	bJustTeleported() = false;

	vec3 zoneVel(0.0f);
	if (UObject::TryCast<UPlayerPawn>(this) || dot(zone->ZoneVelocity(), zone->ZoneVelocity()) > 90000.0f)
		zoneVel = zone->ZoneVelocity();
	vec3 adjusted = (Velocity() + zoneVel) * deltaTime;
	CollisionHit hit;
	TryMoveHeldOff(adjusted, hit);
	if (hit.Fraction < 1.0f)
	{
		vec3 gravDir(0.0f, 0.0f, zone->ZoneGravity().z > 0.0f ? 1.0f : -1.0f);
		vec3 desiredDir = X87::SafeNormal(adjusted);
		float upDown = dot(gravDir, X87::SafeNormal(Velocity()));
		if (std::abs(hit.Normal.z) < 0.2f && upDown < 0.5f && upDown > -0.2f)
		{
			float stepZ = Location().z;
			DeusExStepUp(gravDir, desiredDir, adjusted * (1.0f - hit.Fraction), hit);
			OldLocation().z = Location().z + (OldLocation().z - stepZ);
		}
		else
		{
			ProcessHitWall(hit.Normal, hit.Actor);
			vec3 oldHitNormal = hit.Normal;
			vec3 delta = (adjusted - hit.Normal * dot(adjusted, hit.Normal)) * (1.0f - hit.Fraction);
			if (dot(delta, adjusted) >= 0.0f)
			{
				TryMoveHeldOff(delta, hit);
				if (hit.Fraction < 1.0f)
				{
					ProcessHitWall(hit.Normal, hit.Actor);
					TwoWallAdjust(desiredDir, delta, hit.Normal, oldHitNormal, hit.Fraction);
					TryMoveHeldOff(delta, hit);
				}
			}
		}
	}
	if (!bJustTeleported())
		Velocity() = (Location() - OldLocation()) / deltaTime;
}

// The texture of the level's surface a line meets first (the world and
// movers, flags 6), a filed mover's polygon's where the node met is one
// (BrushTracker): none for a mover met as an actor (0x103ccb70).
UTexture* UPawn::DeusExLineTexture(const vec3& start, const vec3& end, CollisionHit& hit, vec3& hitLocation)
{
	TraceFlags world;
	world.movers = true;
	world.world = true;
	hit = XLevel()->Collision.TraceFirstHit(start, end, this, vec3(0.0f), world);
	hitLocation = start + (end - start) * hit.Fraction;
	if (!hit.Actor || !hit.Node || !UObject::TryCast<ULevelInfo>(hit.Actor))
		return nullptr;
	const BspNode* node = hit.NodeHead ? hit.NodeHead : hit.Node;
	return XLevel()->Collision.LineSurface(node->Surf).Material;
}

// The ladder probe (0x103ccc40): lines 2 x the radius long from 0.95 of the
// height under the middle, toward the facing (pitch level), the left, the
// right and the back, each ending 0 to 32 units up in steps of 4, until one
// meets a texture in a group named Ladder; the texture last met.
UTexture* UPawn::DeusExLadderProbe(CollisionHit& hit, vec3& hitLocation)
{
	vec3 start = Location() - vec3(0.0f, 0.0f, CollisionHeight() * 0.95f);
	Rotator facing = Rotation();
	facing.Pitch = 0;
	int baseYaw = facing.Yaw;
	auto found = [](UTexture* texture) { return texture && (!texture->Outer() || InGroup(texture, "Ladder", true)); };

	UTexture* texture = nullptr;
	const int yaws[4] = { 0, -16384, 16384, 32768 };
	for (int d = 0; d < 4; d++)
	{
		if (d > 0 && found(texture))
			break;
		facing.Yaw = baseYaw + yaws[d];
		vec3 end = start + Coords::Rotation(facing).XAxis * (2.0f * CollisionRadius());
		for (int up = 0; up <= 32; up += 4)
		{
			if (found(texture))
				break;
			texture = DeusExLineTexture(start, end + vec3(0.0f, 0.0f, (float)up), hit, hitLocation);
		}
	}
	return texture;
}

// The ladder check (0x103cc450), first in Deus Ex's walking, falling and
// swimming, for a player only. On a ladder (the probe's texture in a Ladder
// group) WalkTexture is given it and the player climbs: with an
// acceleration, walking, its velocity's Z half GroundSpeed x the Z of its
// view's direction, the pitch 6,144 farther from 0; physFlying moves it, and
// with no acceleration it stops. That takes the tick. Off a ladder
// WalkTexture is given the texture a line straight down (2 x the radius + the
// height long) meets, and a walking player on a texture with a Friction under
// 1 slides along the floor by gravity.
bool UPawn::DeusExLadder(float deltaTime)
{
	if (!UObject::TryCast<UPlayerPawn>(this))
		return false;

	CollisionHit hit;
	vec3 hitLocation;
	UTexture* texture = DeusExLadderProbe(hit, hitLocation);
	if (!InGroup(texture, "Ladder", false))
	{
		texture = DeusExLineTexture(Location(), Location() - vec3(0.0f, 0.0f, 2.0f * CollisionRadius() + CollisionHeight()), hit, hitLocation);
		CallEvent(this, "WalkTexture", { ExpressionValue::ObjectValue(texture), ExpressionValue::VectorValue(hitLocation), ExpressionValue::VectorValue(hit.Normal) });
		if (Physics() == PHYS_Walking && texture && texture->Friction() < 1.0f)
		{
			// The zone's gravity x the tick, x one over half the slip (the
			// x87's reciprocal), x the tick, each a float; less its part along
			// the floor's normal, each dot Z's product first, then Y's, then
			// X's (0x103cc846-0x103cc994)
			auto R = X87::Round;
			float slip = std::max(texture->Friction() * 4.0f, 0.05f);
			double over = R(1.0 / ((double)slip * 0.5));
			vec3 fall = Region().Zone->ZoneGravity() * deltaTime;
			vec3 move((float)R(fall.x * over) * deltaTime, (float)R(fall.y * over) * deltaTime, (float)R(fall.z * over) * deltaTime);
			const vec3& n = hit.Normal;
			double along = R(R(R((double)move.z * n.z) + R((double)move.y * n.y)) + R((double)n.x * move.x));
			vec3 slide = move - vec3((float)R(n.x * along), (float)R(n.y * along), (float)R(n.z * along));
			if (!X87::Under(R(R(R((double)move.z * slide.z) + R((double)move.y * slide.y)) + R((double)move.x * slide.x)), 0.0))
				TryMoveHeldOff(slide);
		}
		return false;
	}

	CallEvent(this, "WalkTexture", { ExpressionValue::ObjectValue(texture), ExpressionValue::VectorValue(hitLocation), ExpressionValue::VectorValue(hit.Normal) });
	if (Acceleration() != vec3(0.0f))
	{
		Rotator view = ViewRotation();
		view.Pitch += view.Pitch >= 0 ? 6144 : -6144;
		bIsWalking() = true;
		Velocity().z = GroundSpeed() * Coords::Rotation(view).XAxis.z * 0.5f;
	}
	DeusExPhysFlying(deltaTime);
	if (Acceleration() == vec3(0.0f))
		Velocity() = vec3(0.0f);
	return true;
}
