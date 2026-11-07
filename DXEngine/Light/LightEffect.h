#pragma once

#include <cmath>
#include "Math/vec.h"
#include "Packages/Engine/Actors/UActor.h"

class UActor;

// A run of lightmap texels: [x0, x1) of row y.
struct LightmapSpan
{
	int y, x0, x1;
};

// A rectangle of lightmap texels: [x0, x1) of the rows [y0, y1).
struct LightmapRect
{
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;

	bool Empty() const { return x0 >= x1 || y0 >= y1; }

	// The rectangle that holds both
	void Add(const LightmapRect& r)
	{
		if (r.Empty())
			return;
		if (Empty())
		{
			*this = r;
			return;
		}
		x0 = std::min(x0, r.x0);
		y0 = std::min(y0, r.y0);
		x1 = std::max(x1, r.x1);
		y1 = std::max(y1, r.y1);
	}

	// The spans' rectangle; the spans go down the rows
	static LightmapRect Of(const LightmapSpan* spans, size_t count)
	{
		LightmapRect r;
		if (count == 0)
			return r;
		r.x0 = spans[0].x0;
		r.x1 = spans[0].x1;
		r.y0 = spans[0].y;
		r.y1 = spans[count - 1].y + 1;
		for (size_t i = 1; i < count; i++)
		{
			r.x0 = std::min(r.x0, spans[i].x0);
			r.x1 = std::max(r.x1, spans[i].x1);
		}
		return r;
	}
};

struct LightEffectArgs
{
	UActor* light;
	int size;
	const vec3* locations;
	float* result;
	vec3 LightLocation;
	float radius;
	float invRadius;
	float invRadiusSquared;
	vec3 N;
	const float* shadowmap;
};

class LightEffect
{
public:
	// Writes result[] in the spans only; the texels outside them are left as they were.
	void Run(UActor* light, int width, const LightmapSpan* spans, size_t count, const vec3* locations, vec3 base, vec3 normal, const float* shadowmap, float* result);

	// Where a spotlight's cone points (Render.dll SpatialSpotlight,
	// 0x10b04f89-0x10b04fa9): along a pawn's view rotation, any other
	// light's rotation
	static Rotator SpotRotation(UActor* light);

private:
	// The plain shape and the spotlight, as Render.dll's: the light's
	// falloff, 1 + 2v^3 - 3v^2 (CalcLightDistanceFalloff), times the angle
	void NoneEffect(LightEffectArgs* args);
	void SpotlightEffect(LightEffectArgs* args);

	// Deus Ex's, Render.dll's own shapes
	static float PlainDX(const LightEffectArgs* args, int i, vec3& d);
	void NonIncidenceEffectDX(LightEffectArgs* args);
	void CylinderEffectDX(LightEffectArgs* args);
	void WaveEffectDX(LightEffectArgs* args, bool fast);
	void SlowWaveEffectDX(LightEffectArgs* args);
	void FastWaveEffectDX(LightEffectArgs* args);
	void SearchlightEffectDX(LightEffectArgs* args);
	void RotorEffectDX(LightEffectArgs* args);
	void DiscoEffectDX(LightEffectArgs* args);
	void ShockEffectDX(LightEffectArgs* args);
	void ShellEffectDX(LightEffectArgs* args);
	void InterferenceEffectDX(LightEffectArgs* args);

	typedef void (LightEffect::* EffectFunc)(LightEffectArgs* args);
	static EffectFunc Effects[LE_Unused + 1];

	enum { FalloffTableSize = 1024 };

	static void InitTables();
	static float LightDistanceFalloff(float distsqr);
	static float CalcLightDistanceFalloff(float distsqr);
	static bool TablesInitialized;
	static float FalloffTable[FalloffTableSize];
};
