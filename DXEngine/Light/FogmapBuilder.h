#pragma once

#include "Math/vec.h"

class Coords;
class BspSurface;
class LightMapIndex;
class UModel;
class UZoneInfo;
class UActor;

// A volumetric light as the original's light manager sets it up for a fog
// map, as it is this frame (FLightManager::SetupLight, Render.dll
// 0x10b05fa0): its sphere, (VolumeRadius + 1) x 25 units about where it is;
// its colour the light map's record of it, GlobalLighting's colour times
// its brightness now and the level's Brightness; its density K,
// VolumeBrightness x that brightness / 64 (0x10b06bf7-0x10b06c03); its
// fog VolumeFog / 255
struct FogLight
{
	UActor* Actor = nullptr;
	vec3 Location = vec3(0.0f);
	float Radius = 0.0f;
	float RadiusSquared = 0.0f;
	float InvRadius = 0.0f;
	float InvRadiusSquared = 0.0f;
	vec3 Color = vec3(0.0f);
	float Density = 0.0f;
	float Fog = 0.0f;
};

class FogmapBuilder
{
public:
	static FogLight SetupLight(UActor* light);

	void Setup(UModel* model, const Coords& mapCoords, int lightMap, UZoneInfo* zoneActor);
	void AddLight(const FogLight& light, const vec3& view);

	// Whether a line of sight from the eye to some texel's centre can cross
	// the light's sphere, so that AddLight would fog any texel
	bool Reaches(const FogLight& light, const vec3& view) const;

	int Width() const { return width; }
	int Height() const { return height; }
	// In the original's units, each channel 0 to 127, which its fog map's
	// seven bits hold and D3DDrv draws at 2c/255
	const vec4* Pixels() const { return fogcolors.data(); }

private:
	const vec3* WorldLocations() const { return points.data(); }
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
