#pragma once

#include "Math/vec.h"

class Coords;
class BspSurface;
class LightMapIndex;
class UModel;
class UZoneInfo;
class UActor;

class FogmapBuilder
{
public:
	void Setup(UModel* model, const Coords& mapCoords, int lightMap, UZoneInfo* zoneActor);
	void AddLight(UActor* light, vec3 view);

	// Whether a line of sight from the eye to some texel's centre can cross
	// the light's sphere, so that AddLight would fog any texel
	bool Reaches(UActor* light, const vec3& view) const;

	int Width() const { return width; }
	int Height() const { return height; }
	const vec4* Pixels() const { return fogcolors.data(); }

	static float SphereDensity(const vec3& rayOrigin, const vec3& rayDirection, const vec3& sphereCenter, float sphereRadius, float dbuffer);

private:
	const vec3* WorldLocations() const { return points.data(); }
	static void PrepareFogInfo(UActor* light);
	void Prepare();

	void SetupRows(const Coords& MapCoords, const LightMapIndex& lmindex);
	void RowLine(int y, float& x0, float& x1, vec3& p0, vec3& p1) const;
	vec3 Location(int i, int y) const;
	void CalcWorldLocations();

	int width = 0;
	int height = 0;
	bool prepared = false;
	vec3 corner[3];
	vec2 uv[3];
	float leftDY = 0.0f, leftStep = 0.0f, rightDY = 0.0f, rightStep = 0.0f;
	Array<vec4> fogcolors;
	Array<vec3> points;
};
