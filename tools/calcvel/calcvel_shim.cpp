// DXEngine's UPawn::DeusExCalcVelocity, its source text as
// Packages/Engine/Actors/UActor_PhysWalking.cpp has it (calcvel_body.inc,
// extract.py), on DXEngine's own Math/vec.h and Math/floating.h, a pawn of
// the members it reads; and the direction its callers hand it.
#include "Math/vec.h"
#include "Math/floating.h"

enum { NM_Standalone = 0 };
struct UZoneInfo { vec3 gravity; vec3& ZoneGravity() { return gravity; } };
struct PointRegion { UZoneInfo* Zone = nullptr; };
struct ULevelInfo { int netMode = 0; int NetMode() { return netMode; } };
struct UPlayerPawn;
class UPawn;
struct UObject
{
	template<class T> static T* TryCast(UPawn* p);
};
class UPawn
{
public:
	vec3 velocity, acceleration;
	float accelRate = 0, desiredSpeed = 0, mass = 0, buoyancy = 0;
	bool walking = false, isPlayer = false;
	UZoneInfo zone;
	PointRegion region;
	ULevelInfo level;
	vec3& Velocity() { return velocity; }
	vec3& Acceleration() { return acceleration; }
	float& AccelRate() { return accelRate; }
	float& DesiredSpeed() { return desiredSpeed; }
	float& Mass() { return mass; }
	float& Buoyancy() { return buoyancy; }
	bool bIsWalking() { return walking; }
	PointRegion& Region() { return region; }
	ULevelInfo* Level() { return &level; }
	void DeusExCalcVelocity(const vec3& accelDir, float deltaTime, float maxSpeed, float friction, bool fluid, bool brake, bool buoyant);
};
template<> UPlayerPawn* UObject::TryCast<UPlayerPawn>(UPawn* p) { return p->isPlayer ? (UPlayerPawn*)p : nullptr; }

#include "calcvel_body.inc"

extern "C"
{
	struct PawnState
	{
		float vel[3], acc[3];
		float mass, buoyancy, desiredSpeed, accelRate;
		float gravity[3];
		int player, walking, netMode;
	};

	void dxe_draft_single(int on) { X87::Single = on != 0; }

	// The acceleration itself where each axis compares equal to 0, else
	// Core's SafeNormal of it, as walking, flying and swimming hand it over
	void dxe_accel_dir(const float a[3], float out[3])
	{
		vec3 acc(a[0], a[1], a[2]);
		vec3 d = X87::Equal3(acc, vec3(0.0f)) ? acc : X87::SafeNormal(acc);
		out[0] = d.x; out[1] = d.y; out[2] = d.z;
	}

	void dxe_draft(PawnState* s, const float d[3], float dt, float maxSpeed, float friction, int fluid, int brake, int buoyant)
	{
		UPawn p;
		p.velocity = vec3(s->vel[0], s->vel[1], s->vel[2]);
		p.acceleration = vec3(s->acc[0], s->acc[1], s->acc[2]);
		p.accelRate = s->accelRate;
		p.desiredSpeed = s->desiredSpeed;
		p.mass = s->mass;
		p.buoyancy = s->buoyancy;
		p.walking = s->walking != 0;
		p.isPlayer = s->player != 0;
		p.zone.gravity = vec3(s->gravity[0], s->gravity[1], s->gravity[2]);
		p.region.Zone = &p.zone;
		p.level.netMode = s->netMode;
		p.DeusExCalcVelocity(vec3(d[0], d[1], d[2]), dt, maxSpeed, friction, fluid != 0, brake != 0, buoyant != 0);
		s->vel[0] = p.velocity.x; s->vel[1] = p.velocity.y; s->vel[2] = p.velocity.z;
		s->acc[0] = p.acceleration.x; s->acc[1] = p.acceleration.y; s->acc[2] = p.acceleration.z;
	}
}
