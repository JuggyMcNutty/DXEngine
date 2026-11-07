
#include "Precomp.h"
#include "UPawn.h"
#include "UPlayerPawn.h"
#include "Packages/Core/UClass.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UGameInfo.h"
#include "Packages/Engine/Actors/Info/UPlayerReplicationInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Actors/NavigationPoint/UNavigationPoint.h"
#include <queue>
#include <climits>
#include "Utils/Logger.h"
#include "Utils/Random.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Subsystems/UDXEngineAudioDevice.h"
#include "Engine.h"

// The original's APawn::LineOfSightTo (dx-reverse-info/engine-dll.md, the
// senses), UT's with a flag that lifts the distance limits. Each line is a
// FastTrace from the pawn's eyes.
bool UPawn::DeusExLineOfSightTo(UActor* other, bool useLOSFlag, bool ignoreDistance)
{
	if (!other)
		return false;

	// The LOS flag, alternating at each look, is never used on the enemy.
	if (other == Enemy())
		useLOSFlag = false;
	else if (useLOSFlag)
		bLOSflag() = !bLOSflag();

	UPawn* otherPawn = UObject::TryCast<UPawn>(other);
	vec3 delta = other->Location() - Location();
	float distSq = dot(delta, delta);

	// How far it can be: with the LOS flag, the pawn's sight radius (for a
	// pawn, times its visibility), nothing outside its peripheral vision,
	// and the distance scaled up toward that edge and by the height between;
	// without it, a reach its visibility sets for a pawn.
	float maxDistSq;
	if (useLOSFlag)
	{
		if (!otherPawn)
		{
			maxDistSq = SightRadius() * SightRadius();
		}
		else
		{
			float sight = std::min(otherPawn->Visibility() * 0.0078125f, 1.0f) * SightRadius();
			maxDistSq = std::min(sight * sight, bIsPlayer() ? 16000000.0f : 12000000.0f);
		}
		if (distSq > maxDistSq)
			return false;

		vec3 dir = distSq > 1.0e-8f ? delta * (1.0f / std::sqrt(distSq)) : vec3(0.0f);
		float facing = dot(Coords::Rotation(Rotation()).XAxis, dir) - PeripheralVision();
		Stimulus() = (facing > 0.0f ? facing * 0.8f : facing * 0.17f) + 0.2f;
		if (Stimulus() <= 0.0f)
			return false;
		float height = std::abs(other->Location().z - Location().z) / std::max(Skill() + 1.0f, 1.0f);
		distSq = (height * height + distSq) / (Stimulus() * Stimulus());
		if (distSq > maxDistSq && !ignoreDistance)
			return false;
		Stimulus() = 1.0f;
	}
	else
	{
		if (otherPawn)
		{
			float reach = std::min((otherPawn->Visibility() + 16) * 0.015f, 1.0f) * (bIsPlayer() ? 5000.0f : 4000.0f);
			maxDistSq = std::min(reach * reach, bIsPlayer() ? 25000000.0f : 16000000.0f);
		}
		else
		{
			maxDistSq = bIsPlayer() ? 16000000.0f : 9000000.0f;
		}
		if (distSq > maxDistSq && !ignoreDistance)
			return false;
	}

	vec3 viewPoint = Location();
	viewPoint.z += BaseEyeHeight();

	// The enemy: a line to its middle, from the eyes or from the pawn's own
	// middle, and where each stood noted.
	if (other == Enemy())
	{
		if (FastTrace(other->Location(), viewPoint) || FastTrace(other->Location(), Location()))
		{
			LastSeeingPos() = Location();
			LastSeenPos() = other->Location();
			return true;
		}
		if (distSq > 1000000.0f && !ignoreDistance)
			return false;
	}
	else if (distSq > 1000000.0f && !ignoreDistance)
	{
		// Beyond 1,000 units only the middle, and for a pawn beyond half the
		// reach not unless the LOS flag stands, nor half the time for a pawn
		// that is no player.
		if (otherPawn)
		{
			if (!bLOSflag() && maxDistSq * 0.5f < distSq)
				return false;
			if (!bIsPlayer() && FRand() < 0.5f)
				return false;
		}
		return FastTrace(other->Location(), viewPoint);
	}

	// Its head, 0.8 of its height over its middle -- passed over when the
	// LOS flag, used, stands.
	if (!(useLOSFlag && bLOSflag()) && FastTrace(other->Location() + vec3(0.0f, 0.0f, other->CollisionHeight() * 0.8f), viewPoint))
		return true;

	if ((distSq > 250000.0f && !ignoreDistance) || !otherPawn)
		return false;

	// Its cylinder's four corners at its middle's height, less the nearest
	// and the farthest -- measured, as the original measures them, from the
	// world's origin --, each other one tried when the LOS flag is used.
	float r = other->CollisionRadius();
	vec3 loc = other->Location();
	vec3 corners[4] = { loc + vec3(-r, r, 0.0f), loc + vec3(r, r, 0.0f), loc + vec3(-r, -r, 0.0f), loc + vec3(r, -r, 0.0f) };
	int nearest = 0, farthest = 0;
	float nearestSq = dot(corners[0], corners[0]);
	float farthestSq = nearestSq;
	for (int i = 1; i < 4; i++)
	{
		float sizeSq = dot(corners[i], corners[i]);
		if (sizeSq > farthestSq)
		{
			farthestSq = sizeSq;
			farthest = i;
		}
		else if (sizeSq < nearestSq)
		{
			nearestSq = sizeSq;
			nearest = i;
		}
	}
	bool skip = bLOSflag();
	for (int i = 0; i < 4; i++)
	{
		if (i == nearest || i == farthest)
			continue;
		if (skip && useLOSFlag)
		{
			skip = false;
			continue;
		}
		skip = true;
		if (FastTrace(corners[i], viewPoint))
			return true;
	}
	return false;
}

bool UPawn::LineOfSightTo(UActor* other, bool ignoreDistance)
{
	return DeusExLineOfSightTo(other, false, ignoreDistance);
}

bool UPawn::CanSee(UActor* other)
{
	return DeusExLineOfSightTo(other, true, false);
}

bool UPawn::CanHearNoise(UActor* source, float loudness)
{
	UPawn* noisePawn = UObject::Cast<UPawn>(source->Instigator());
	if (!noisePawn->bIsPlayer() && (!noisePawn->Enemy() || !noisePawn->Enemy()->bIsPlayer()))
	{
		if (!IsA(source->Class) && !source->IsA(Class))
			return false;
	}
	else if (UObject::TryCast<UPlayerPawn>(this))
	{
		return false;
	}

	vec3 delta = Location() - source->Location();
	float dist2 = dot(delta, delta);

	if (!bIsPlayer() || !Level()->Game()->bTeamGame() || !noisePawn->bIsPlayer() ||
		!PlayerReplicationInfo() || !noisePawn->PlayerReplicationInfo() || (PlayerReplicationInfo()->Team() != noisePawn->PlayerReplicationInfo()->Team()))
	{
		if (dist2 > (4000.0f * 4000.0f) * (loudness * loudness))
			return false;

		float perceived = std::min(1200000.f / dist2, 2.0f);
		Stimulus() = loudness * perceived + Alertness() * std::min(0.5f, perceived);
		if (Stimulus() < HearingThreshold())
			return false;
	}
	else if (dist2 > (4000.0f * 4000.0f) * (loudness * loudness))
	{
		return false;
	}

	// The line walked from the hearer to the noise, as the original's
	// CanHear takes it (Engine.dll 0x103bf9d5)
	return !XLevel()->Collision.TraceAnyHit(Location(), source->Location(), source, false, true, false);
}

// The original's native (Engine.dll 0x103dfc40): only a player pawn whose
// player is a viewport, with audio, plays it -- the volume and pitch over
// 100, a radius of 0 as 1,600, an actor being destroyed as None.
void UPawn::DeusExClientHearSound(UActor* actor, int id, USound* sound, const vec3& soundLocation, const vec3& parameters)
{
	UPlayerPawn* player = UObject::TryCast<UPlayerPawn>(this);
	if (!player || !UObject::TryCast<UViewport>(player->Player()) || !engine->audiodev)
		return;
	if (actor && actor->bDeleteMe())
		actor = nullptr;
	float radius = parameters.y != 0.0f ? parameters.y : 1600.0f;
	engine->audiodev->PlaySound(actor, id, sound, soundLocation, parameters.x * 0.01f, radius, parameters.z * 0.01f, (id & 14) == SLOT_Talk * 2);
}

UActor* UPawn::PickAnyTarget(float& bestAim, float& bestDist, const vec3& FireDir, const vec3& projStart)
{
	UActor* bestActor = nullptr;
	for (UActor* actor : XLevel()->Actors)
	{
		// We are only looking for targets that isn't a pawn (pawn uses PickTarget if it wants a pawn)
		if (!actor || actor == this || UObject::TryCast<UPawn>(actor) || !actor->bProjTarget())
			continue;

		if (CheckIfBestTarget(actor, bestAim, bestDist, FireDir, projStart))
			bestActor = actor;
	}
	return bestActor;
}

// Auto-aim's targets (APawn::execPickTarget, Engine.dll 0x103baca0;
// execPickAnyTarget, 0x103bb130; dx-reverse-info/engine-dll.md, small): the
// level's pawn list -- a pawn but itself, alive, a projectile target, not a
// team mate in a team game, and none at all when the level has no Game (a
// client's) -- or, for any target, the level's actors that are not pawns
// and are projectile targets.
UActor* UPawn::PickTarget(float& bestAim, float& bestDist, const vec3& FireDir, const vec3& projStart)
{
	UActor* bestActor = nullptr;
	UGameInfo* game = Level()->Game();
	if (!game)
		return nullptr;
	UPlayerReplicationInfo* ourPlayerInfo = PlayerReplicationInfo();
	bool teamGame = game->bTeamGame();
	for (UPawn* pawn = Level()->PawnList(); pawn != nullptr; pawn = pawn->nextPawn())
	{
		if (pawn == this || pawn->Health() <= 0 || !pawn->bProjTarget())
			continue;

		// Skip team mates
		auto pawnPlayerInfo = pawn->PlayerReplicationInfo();
		if (teamGame && ourPlayerInfo && pawnPlayerInfo && ourPlayerInfo->Team() == pawnPlayerInfo->Team())
			continue;

		if (CheckIfBestTarget(pawn, bestAim, bestDist, FireDir, projStart))
			bestActor = pawn;
	}
	return bestActor;
}

// One ahead (the direction's dot with the way to it above 0), nearer than
// 2000, whose aim (that dot over the distance) beats the best so far,
// strictly, and that the pawn sees.
bool UPawn::CheckIfBestTarget(UActor* actor, float& bestAim, float& bestDist, const vec3& FireDir, const vec3& projStart)
{
	vec3 delta = actor->Location() - projStart;
	float angle = dot(FireDir, delta);
	if (angle <= 0.0f)
		return false;

	float distance = length(delta);
	if (distance >= 2000.0f)
		return false;

	angle /= distance;
	if (angle <= bestAim)
		return false;

	// Skip if we can't see the target
	if (!LineOfSightTo(actor, false))
		return false;

	// OK, this is better than what we have
	bestAim = angle;
	bestDist = distance;
	return true;
}

// The original's AICanHear (0x103c7680), which only the event manager
// calls: 0 unless other is bDetectable and the volume above 0; a radius of
// 800 when none above 0 is given; vertical distance counts double; at or
// beyond the radius 0, else (1 - distance / radius) times the volume, less
// the pawn's HearingThreshold, held between 0 and 1.
float UPawn::AICanHear(UActor* other, std::optional<float> volumeArg, std::optional<float> radiusArg)
{
	float volume = volumeArg.value_or(1.0f);
	float radius = radiusArg.value_or(0.0f);
	if (radius <= 0.0f)
		radius = 800.0f;
	if (!other || !other->bDetectable() || volume <= 0.0f)
		return 0.0f;

	vec3 delta = other->Location() - Location();
	delta.z *= 2.0f;
	float distance = length(delta);
	if (distance >= radius)
		return 0.0f;
	return std::clamp((1.0f - distance / radius) * volume - HearingThreshold(), 0.0f, 1.0f);
}

// How much of something size degrees across, angle degrees off the middle of
// a field of view fov degrees wide, is inside it: 1 all of it, 0 none, falling
// off linearly across the edge. From Engine.dll, where it serves
// APawn::AICanSee.
static float AIFieldOfViewShare(float angle, float fov, float size)
{
	while (angle <= -180.0f)
		angle += 360.0f;
	while (angle > 180.0f)
		angle -= 360.0f;
	if (angle < 0.0f)
		angle = -angle;

	float inner = (float)((fov - size) * 0.5);
	float outer = inner + size;
	if (angle > outer)
		return 0.0f;
	if (angle <= inner)
		return 1.0f;
	return 1.0f - (angle - inner) / size;
}

// How well this pawn sees another actor, 0 to 1, as Deus Ex's AI judges it
// (Engine.dll APawn::AICanSee; the defaults are its exec function's). From
// the eyes: how large the other looks -- nothing under MinAngularSize -- and,
// with bCheckDir, how much of it is inside the view (AIHorizontalFov wide,
// AspectRatio times narrower up and down, turned by AIAddViewRotation); with
// bCheckVisibility, how lit it is (AIVisibility); less VisibilityThreshold.
// With bCheckLOS, zero unless a line reaches it past the world and anything
// that blocks sight: its middle, or with bCheckCylinder a player's eyes, its
// top and its bottom, and anything else's sides.
float UPawn::AICanSee(UActor* other, std::optional<float> visibilityArg, std::optional<bool> bCheckVisibilityArg, std::optional<bool> bCheckDirArg, std::optional<bool> bCheckCylinderArg, std::optional<bool> bCheckLOSArg)
{
	float visibility = visibilityArg.value_or(1.0f);
	bool bCheckVisibility = bCheckVisibilityArg.value_or(true);
	bool bCheckDir = bCheckDirArg.value_or(true);
	bool bCheckCylinder = bCheckCylinderArg.value_or(false);
	bool bCheckLOS = bCheckLOSArg.value_or(true);

	if (!other || visibility <= 0.0f || !other->bDetectable())
		return 0.0f;

	vec3 eye = Location();
	eye.z += BaseEyeHeight();
	vec3 delta = other->Location() - eye;
	double distSq = (double)delta.x * delta.x + (double)delta.y * delta.y + (double)delta.z * delta.z;
	if (distSq < 1.0)
		distSq = 1.0;

	// Its apparent size squared, as the tangent of the angle it spans
	double radiusSq = (double)other->CollisionRadius() * other->CollisionRadius();
	double heightSq = (double)other->CollisionHeight() * other->CollisionHeight();
	double sizeSq = (radiusSq + heightSq) / distSq;
	if (sizeSq <= MinAngularSize())
		return 0.0f;
	if (sizeSq < 0.0003046792916483) // under a degree: its middle stands for it
		bCheckCylinder = false;
	visibility = (float)(visibility * sizeSq * 64.0);

	if (bCheckDir && visibility > 0.0f)
	{
		float verticalFov = AspectRatio() > 0.0f ? AIHorizontalFov() / AspectRatio() : 0.0f;
		float width = (float)(std::atan(std::sqrt(radiusSq / distSq)) * 114.59155902616465); // degrees across
		float height = (float)(std::atan(std::sqrt(heightSq / distSq)) * 114.59155902616465);

		Rotator view = (UObject::TryCast<UPlayerPawn>(this) ? ViewRotation() : Rotation()) + AIAddViewRotation();
		Coords axes = Coords::Rotation(view);
		vec3 local = { dot(delta, axes.XAxis), dot(delta, axes.YAxis), dot(delta, axes.ZAxis) };

		// FVector::Rotation's yaw and pitch, in its whole units
		const float unitsPerRadian = 65535.0f / (2.0f * 3.14159265358979f);
		int yaw = (int)(std::atan2(local.y, local.x) * unitsPerRadian);
		int pitch = (int)(std::atan2(local.z, std::sqrt(local.x * local.x + local.y * local.y)) * unitsPerRadian);
		visibility *= AIFieldOfViewShare((float)(yaw * 360.0 / 65536.0), AIHorizontalFov(), width);
		float vertical = AIFieldOfViewShare((float)(pitch * 360.0 / 65536.0), verticalFov, height);
		if (distSq < 22500.0 && vertical < 0.75f) // within 150 units, above or below still shows
			vertical = 0.75f;
		visibility *= vertical;
	}

	if (bCheckVisibility && visibility > 0.0f)
		visibility *= other->AIVisibility(true);

	visibility -= VisibilityThreshold();
	if (visibility < 0.0f)
		visibility = 0.0f;
	else if (visibility >= 1.0f)
		visibility = 1.0f;

	if (bCheckLOS && visibility > 0.0f)
	{
		// What blocks: any actor but this pawn, what owns it and the other,
		// if it blocks sight and is not hidden
		CollisionSystem& collision = XLevel()->Collision;
		auto blocksSight = [&](UActor* actor) {
			for (UActor* viewer = this; viewer; viewer = viewer->Owner())
			{
				if (actor == viewer)
					return false;
			}
			return actor != other && actor->bBlockSight() && !actor->bHidden();
		};
		auto reaches = [&](const vec3& point) { return !collision.SightBlocked(eye, point, blocksSight); };

		const vec3& center = other->Location();
		bool seen;
		if (!bCheckCylinder)
		{
			seen = reaches(center);
		}
		else
		{
			UPlayerPawn* player = UObject::TryCast<UPlayerPawn>(other);
			seen = (player && reaches(center + vec3(0.0f, 0.0f, player->BaseEyeHeight()))) ||
				reaches(center + vec3(0.0f, 0.0f, other->CollisionHeight())) ||
				reaches(center - vec3(0.0f, 0.0f, other->CollisionHeight()));
			if (!player && !seen)
			{
				// Its sides, square to the line to it
				vec3 across = { delta.x, delta.y, 0.0f };
				float lengthSq = dot(across, across);
				across = lengthSq < 1e-8f ? vec3(0.0f) : across * (1.0f / std::sqrt(lengthSq));
				float radius = other->CollisionRadius();
				seen = reaches(center + vec3(across.y * radius, -across.x * radius, 0.0f)) ||
					reaches(center + vec3(-across.y * radius, across.x * radius, 0.0f));
			}
		}
		if (!seen)
			visibility = 0.0f;
	}
	return visibility;
}

bool UPawn::PickWallAdjust()
{
	auto kneeHeight = CollisionHeight() * 0.45f;

	auto forwards = normalize(Acceleration().xy());

	auto afterJumpCollisionHit = TryMove(vec3(forwards, kneeHeight), true);

	if (afterJumpCollisionHit.Fraction == 1)
	{
		// Obstacle can be jumped over. Attempt jumping.
		bFromWall() = false;
		Velocity().z = JumpZ();
		SetPhysics(PHYS_Falling);
		Destination() = Location() + vec3(forwards, kneeHeight);

		return true;
	}

	// Obstacle cannot be jumped over. Try another direction
	auto direction = Focus() - Location();
	auto rightSideVec = normalize(cross(direction, vec3(0, 0, 1)));
	auto rightSideTest = TryMove(rightSideVec, true);
	if (rightSideTest.Fraction == 1)
	{
		// We can move to right instead
		bFromWall() = true;
		Destination() = Location() + rightSideVec;
		// Focus() = Location() + rightSideVec;

		return true;
	}

	auto leftSideVec = -rightSideVec;
	auto leftSideTest = TryMove(leftSideVec, true);
	if (leftSideTest.Fraction >= 1)
	{
		// We can move to left instead
		bFromWall() = true;
		Destination() = Location() + leftSideVec;
		// Focus() = Location() + leftSideVec;

		return true;
	}

	// Cannot go anywhere from here
	return false;
}

// Deus Ex's is the original's (APawn::execEAdjustJump 0x103bc110): the
// pawn's velocity made a jump to its Destination by SuggestJumpVelocity.
vec3 UPawn::EAdjustJump()
{
	vec3 vel = Velocity();
	SuggestJumpVelocity(Destination(), vel);
	return vel;
}

// The original's calcMoveFlags: which reach specs the pawn may use.
int UPawn::CalcMoveFlags()
{
	int flags = 0;
	if (bCanWalk()) flags |= 1;      // R_WALK
	if (bCanFly()) flags |= 2;       // R_FLY
	if (bCanSwim()) flags |= 4;      // R_SWIM
	if (bCanJump()) flags |= 8;      // R_JUMP
	if (bCanOpenDoors()) flags |= 16;   // R_DOOR
	if (bCanDoSpecial()) flags |= 32;   // R_SPECIAL
	if (bIsPlayer()) flags |= 64;       // R_PLAYERONLY (calcMoveFlags, Engine.dll 0x10326d10)
	return flags;
}

// The original's AIDirectionReachable (0x103c78a0): whether the pawn can
// get, along a direction, to a spot whose distance from focus is between
// the two bounds. It moves the pawn itself in steps of its collision
// radius (held between 5 and 25 units, at most 100 of them), each the
// engine's own walk, fly or swim move, and puts it back.
bool UPawn::AIDirectionReachable(const vec3& focus, int yaw, int pitch, float minDist, float maxDist, vec3& bestDest)
{
	// Deus Ex's steps with the original's own walk, fly and swim moves
	// (UPawn_ReachDeusEx.cpp).
	return DeusExAIDirectionReachable(focus, yaw, pitch, minDist, maxDist, bestDest);
}

// The original's AIPickRandomDestination (0x103c7fc0): up to tries
// directions from RandomBiasedRotation, each tested with
// AIDirectionReachable; a multiplier below 1 stops the pawn short of what
// it can reach, at the first step past that share of it. The direction's
// pitch is dropped for a pawn walking, or swimming out of water (0x103c8041),
// whatever its zone.
bool UPawn::AIPickRandomDestination(float minDist, float maxDist, int centralYaw, float yawDistribution, int centralPitch, float pitchDistribution, int tries, float multiplier, vec3& dest)
{
	dest = Location();
	tries = std::max(tries, 1);
	multiplier = std::clamp(multiplier, 0.0001f, 1.0f);
	bool pitched = !(Physics() == PHYS_Walking || (Physics() == PHYS_Swimming && !(Region().Zone && Region().Zone->bWaterZone())));

	for (int i = 0; i < tries; i++)
	{
		Rotator direction = UActor::RandomBiasedRotation(centralYaw, yawDistribution, centralPitch, pitchDistribution);
		int pitch = pitched ? direction.Pitch : 0;
		vec3 found;
		if (!AIDirectionReachable(Location(), direction.Yaw, pitch, minDist / multiplier, maxDist / multiplier, found))
			continue;
		if (multiplier < 1.0f)
		{
			// Again along the same direction, with multiplier of the distance
			// reached as both the least and the most, so the walk stops at its
			// first step past it (0x103c8141-0x103c8148; the root in doubles,
			// as the original's appSqrt takes it).
			vec3 d = found - Location();
			float reached = (float)(std::sqrt((double)d.x * d.x + (double)d.y * d.y + (double)d.z * d.z) * multiplier);
			if (!AIDirectionReachable(Location(), direction.Yaw, pitch, reached, reached, found))
				continue;
		}
		dest = found;
		return true;
	}
	return false;
}

namespace
{
	using PathnodeEntry = std::pair<UNavigationPoint*, float>;

	// GetPathnodeList's comparison (Engine.dll 0x103c6a70): the nearer first.
	int ComparePathnodes(const PathnodeEntry& a, const PathnodeEntry& b)
	{
		if (a.second < b.second)
			return -1;
		return a.second > b.second ? 1 : 0;
	}

	// The C runtime's qsort, which the original's appQsort calls (Core.dll
	// imports MSVCRT's), for the order it leaves equal distances in: a run
	// of up to 8 entries sorted by moving its largest -- the first of equals
	// -- to its end, again and again; a longer one split round the middle of
	// its first, middle and last entries, the two parts kept on a stack, the
	// larger pushed first.
	void SortPathnodes(PathnodeEntry* x, size_t count)
	{
		if (count < 2)
			return;
		size_t stackLo[64], stackHi[64];
		int pos = 0;
		stackLo[0] = 0;
		stackHi[0] = count - 1;
		while (pos >= 0)
		{
			size_t beg = stackLo[pos];
			size_t end = stackHi[pos--];
			if (end - beg < 8)
			{
				for (size_t last = end; last > beg; last--)
				{
					size_t max = beg;
					for (size_t i = beg + 1; i <= last; i++)
					{
						if (ComparePathnodes(x[i], x[max]) > 0)
							max = i;
					}
					if (max != last)
						std::swap(x[max], x[last]);
				}
				continue;
			}

			size_t lo = beg, hi = end, med = lo + (hi - lo + 1) / 2;
			if (ComparePathnodes(x[lo], x[med]) > 0)
				std::swap(x[lo], x[med]);
			if (ComparePathnodes(x[lo], x[hi]) > 0)
				std::swap(x[lo], x[hi]);
			if (ComparePathnodes(x[med], x[hi]) > 0)
				std::swap(x[med], x[hi]);
			lo++;
			hi--;
			for (;;)
			{
				while (lo <= hi && (lo == med || ComparePathnodes(x[lo], x[med]) <= 0))
					lo++;
				while (med != hi && ComparePathnodes(x[hi], x[med]) > 0)
					hi--;
				if (hi < lo)
					break;
				std::swap(x[lo], x[hi]);
				if (hi == med)
					med = lo;
				lo++;
				hi--;
			}
			while (hi > beg && (hi == med || ComparePathnodes(x[hi], x[med]) == 0))
				hi--;

			if (hi - beg >= end - lo)
			{
				stackLo[++pos] = beg;
				stackHi[pos] = hi;
				stackLo[++pos] = lo;
				stackHi[pos] = end;
			}
			else
			{
				stackLo[++pos] = lo;
				stackHi[pos] = end;
				stackLo[++pos] = beg;
				stackHi[pos] = hi;
			}
		}
	}
}

// The original's GetPathnodeList (0x103c6490): up to 32 navigation points and
// their distances, which ReachablePathnodes iterates, sorted, and
// ComputePathnodeDistances seeds from, unsorted (0x103c8cd9, 0x103c8998).
Array<std::pair<UNavigationPoint*, float>> UPawn::GetPathnodeList(UActor* fromPoint, bool usePrunedPaths, bool sorted)
{
	const size_t maxNodes = 32;
	Array<std::pair<UNavigationPoint*, float>> nodes;

	// The start node: FromPoint when it is a navigation point; else the
	// pawn's MoveTarget when that is one the pawn overlaps; else the first
	// in the level's list the pawn overlaps.
	UNavigationPoint* start = UObject::TryCast<UNavigationPoint>(fromPoint);
	if (!start)
	{
		if (UNavigationPoint* target = UObject::TryCast<UNavigationPoint>(MoveTarget()))
		{
			if (IsOverlapping(target))
				start = target;
		}
	}
	if (!start)
	{
		for (UNavigationPoint* nav = Level()->NavigationPointList(); nav; nav = nav->nextNavigationPoint())
		{
			if (IsOverlapping(nav))
			{
				start = nav;
				break;
			}
		}
	}

	if (start)
	{
		// The far end of each of the start's paths, up to the first empty
		// one, whose reach spec the pawn fits and may use, at the spec's
		// distance.
		int moveFlags = CalcMoveFlags();
		auto& specs = XLevel()->ReachSpecs;
		auto addPaths = [&](FixedArrayView<int, 16> paths)
		{
			for (int i = 0; i < 16 && nodes.size() < maxNodes; i++)
			{
				int index = paths[i];
				if (index == -1)
					break;
				if (index < 0 || (size_t)index >= specs.size())
					continue;
				const LevelReachSpec& spec = specs[index];
				if (!spec.endActor)
					continue;
				if (spec.collisionRadius < (int)CollisionRadius() || spec.collisionHeight < (int)CollisionHeight())
					continue;
				if ((spec.reachFlags & moveFlags) != spec.reachFlags)
					continue;
				nodes.push_back({ spec.endActor, (float)spec.distance });
			}
		};
		addPaths(start->Paths());
		if (usePrunedPaths)
			addPaths(start->PrunedPaths());
	}
	else
	{
		// With no start node, the nodes within 1,000 units of FromPoint, or
		// of the pawn (the square under 1e6), at the straight distance, in
		// the level's order: the first 32, then each in place of the nearest
		// held one farther than it, if any -- not the nearest 32. Then those
		// the pawn can reach, its eye's line to each tested (actorReachable),
		// in their order. The x87's sums are doubles here, its stores floats.
		vec3 from = fromPoint ? UObject::Cast<UActor>(fromPoint)->Location() : Location();
		for (UNavigationPoint* nav = Level()->NavigationPointList(); nav; nav = nav->nextNavigationPoint())
		{
			double dx = (double)nav->Location().x - from.x;
			double dy = (double)nav->Location().y - from.y;
			double dz = (double)nav->Location().z - from.z;
			double distSq = (dx * dx + dy * dy) + dz * dz;
			if (distSq >= 1000000.0)
				continue;
			double dist = std::sqrt(distSq);
			if (nodes.size() < maxNodes)
			{
				nodes.push_back({ nav, (float)dist });
				continue;
			}
			size_t slot = nodes.size();
			for (size_t i = 0; i < nodes.size(); i++)
			{
				if (dist < nodes[i].second && (slot == nodes.size() || nodes[slot].second > nodes[i].second))
					slot = i;
			}
			if (slot < nodes.size())
				nodes[slot] = { nav, (float)dist };
		}

		size_t reached = 0;
		for (size_t i = 0; i < nodes.size(); i++)
		{
			if (DeusExActorReachable(nodes[i].first, false))
				nodes[reached++] = nodes[i];
		}
		nodes.resize(reached);
	}

	if (sorted)
		SortPathnodes(nodes.data(), nodes.size());
	return nodes;
}

// The original's ComputePathnodeDistances (0x103c8910): clears the paths
// (clearPaths: every node's visitedWeight 10,000,000, bEndPoint off, the
// open list empty, the cost from SpecialCost or ExtraCost), then sets each
// node's visitedWeight to its shortest distance over the path network from
// GetPathnodeList's nodes, relaxing each node's paths (the original's
// RelaxPathnodeDistances, depth first; a queue by distance here reaches the
// same). No script calls it.
void UPawn::ComputePathnodeDistances(UActor* startActor)
{
	ClearPaths();

	using Entry = std::pair<float, UNavigationPoint*>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
	for (auto& [node, dist] : GetPathnodeList(startActor, false, false))
	{
		if ((int)dist < node->visitedWeight())
		{
			node->visitedWeight() = (int)dist;
			queue.push({ dist, node });
		}
	}

	int moveFlags = CalcMoveFlags();
	auto& specs = XLevel()->ReachSpecs;
	while (!queue.empty())
	{
		auto [dist, node] = queue.top();
		queue.pop();
		if ((int)dist > node->visitedWeight())
			continue;
		for (int i = 0; i < 16; i++)
		{
			int index = node->Paths()[i];
			if (index < 0 || (size_t)index >= specs.size())
				continue;
			const LevelReachSpec& spec = specs[index];
			if (!spec.endActor)
				continue;
			if (spec.collisionRadius < (int)CollisionRadius() || spec.collisionHeight < (int)CollisionHeight())
				continue;
			if ((spec.reachFlags & moveFlags) != spec.reachFlags)
				continue;
			int next = node->visitedWeight() + spec.distance;
			if (next < spec.endActor->visitedWeight())
			{
				spec.endActor->visitedWeight() = next;
				queue.push({ (float)next, spec.endActor });
			}
		}
	}
}
