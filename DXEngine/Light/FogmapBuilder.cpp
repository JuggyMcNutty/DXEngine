
#include "Precomp.h"
#include "FogmapBuilder.h"
#include "Engine.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Actors/UActor.h"
#include "RenderDevice/RenderDevice.h"
#include "Math/hsb.h"

#ifndef NOSSE
#include <immintrin.h>
#endif

void FogmapBuilder::Setup(UModel* model, const Coords& mapCoords, int lightMap, UZoneInfo* zoneActor)
{
	const LightMapIndex& lmindex = model->LightMap[lightMap];

	width = lmindex.UClamp;
	height = lmindex.VClamp;
	SetupRows(mapCoords, lmindex);

	// The texels' places and the clear map are made only for a light that
	// reaches it (Prepare)
	prepared = false;
}

// The texels' world places and the map all clear, once a map
void FogmapBuilder::Prepare()
{
	if (prepared)
		return;
	prepared = true;

	// Stop allocations over time by building up a reserve
	size_t size = (size_t)width * height;
	if (points.size() < size)
		points.resize(size);
	if (fogcolors.size() < size)
		fogcolors.resize(size);

	CalcWorldLocations();

	vec4 zero(0.0f);
	for (size_t i = 0; i < size; i++)
		fogcolors[i] = zero;
}

// A fog light's colour, density and sphere, worked out on its first use
void FogmapBuilder::PrepareFogInfo(UActor* light)
{
	if (light->FogInfo.brightness < 0.0f)
	{
		light->FogInfo.fogcolor = hsbtorgb(light->LightHue(), light->LightSaturation(), light->LightBrightness());
		light->FogInfo.brightness = light->LightBrightness() * (1.0f / 255.0f) * light->VolumeBrightness() * (1.0f / 64.0f);
		light->FogInfo.fog = light->VolumeFog() * (1.0f / 255.0f);
		light->FogInfo.radius = light->WorldVolumetricRadius();
		light->FogInfo.location = light->Location();
	}
}

// The sphere against the pyramid from the eye to the map's corner texels,
// which holds every texel's centre, the map being a flat grid: wholly
// outside one of its four sides, or behind the surface's plane, it is
// crossed by no line of sight to a texel and fogs none (SphereDensity gives
// each nothing). A side seen edge on is passed over: taken as reaching.
bool FogmapBuilder::Reaches(UActor* light, const vec3& view) const
{
	PrepareFogInfo(light);
	vec3 center = light->FogInfo.location;
	float radius = light->FogInfo.radius;

	vec3 corners[4] = { Location(0, 0), Location(width - 1, 0), Location(width - 1, height - 1), Location(0, height - 1) };
	vec3 middle = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f;
	for (int i = 0; i < 4; i++)
	{
		vec3 n = cross(corners[i] - view, corners[(i + 1) % 4] - view);
		float len = length(n);
		if (len < 0.0001f)
			continue;
		n *= 1.0f / len;
		float side = dot(n, middle - view);
		if (std::abs(side) < 0.001f)
			continue;
		if (side < 0.0f)
			n = -n;
		if (dot(n, center - view) < -radius)
			return false;
	}

	vec3 normal = cross(corners[1] - corners[0], corners[3] - corners[0]);
	float len = length(normal);
	if (len >= 0.0001f)
	{
		normal *= 1.0f / len;
		if (dot(normal, view - corners[0]) < 0.0f)
			normal = -normal;
		if (dot(normal, center - corners[0]) < -radius)
			return false;
	}
	return true;
}

void FogmapBuilder::AddLight(UActor* light, vec3 view)
{
	PrepareFogInfo(light);
	Prepare();

	vec3 fogcolor = light->FogInfo.fogcolor;
	float brightness = light->FogInfo.brightness * 5.0f;
	float fog = light->FogInfo.fog;
	float radius = light->FogInfo.radius;
	vec3 lightpos = light->FogInfo.location;

	size_t size = (size_t)width * height;
	const vec3* locations = WorldLocations();
	vec4* dest = fogcolors.data();
	for (size_t i = 0; i < size; i++)
	{
		vec3 rayDirection = locations[i] - view;
#ifndef NOSSE
		float depth = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(dot(rayDirection, rayDirection))));
#else
		float depth = std::sqrt(dot(rayDirection, rayDirection));
#endif
		rayDirection *= (1.0f / depth);
		float fogamount = SphereDensity(view, rayDirection, lightpos, radius, depth) * brightness;

		float alpha = std::min(fogamount * fog, 1.0f);
		float invalpha = 1.0f - alpha;
		dest[i].r = fogcolor.r * fogamount + dest[i].r * invalpha;
		dest[i].g = fogcolor.g * fogamount + dest[i].g * invalpha;
		dest[i].b = fogcolor.b * fogamount + dest[i].b * invalpha;
		dest[i].a = std::min(dest[i].a + alpha, 1.0f);
	}
}

// The MIT License
// https://www.youtube.com/c/InigoQuilez
// https://iquilezles.org/
// Copyright (c) 2015 Inigo Quilez
//
// Analytically integrating quadratically decaying participating media within a sphere.
// Related info: https://iquilezles.org/articles/spherefunctions
//
float FogmapBuilder::SphereDensity(const vec3& rayOrigin, const vec3& rayDirection, const vec3& sphereCenter, float sphereRadius, float dbuffer)
{
	// normalize the problem to the canonical sphere
	float rcp_radius = 1.0f / sphereRadius;
	float ndbuffer = dbuffer * rcp_radius;
	//vec3 rc = (rayOrigin - sphereCenter) * rcp_radius;
	float rc_x = (rayOrigin.x - sphereCenter.x) * rcp_radius;
	float rc_y = (rayOrigin.y - sphereCenter.y) * rcp_radius;
	float rc_z = (rayOrigin.z - sphereCenter.z) * rcp_radius;

	// find intersection with sphere
	float b = rayDirection.x * rc_x + rayDirection.y * rc_y + rayDirection.z * rc_z; // dot(rayDirection, rc)
	float c = rc_x * rc_x + rc_y * rc_y + rc_z * rc_z - 1.0f; // dot(rc, rc) - 1.0f
	float h = b * b - c;

	// not intersecting
	if (h < 0.0f) return 0.0f;

#ifndef NOSSE
	h = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(h)));
#else
	h = std::sqrt(h);
#endif

	//return h*h*h;

	float t1 = -b - h;
	float t2 = -b + h;

	// not visible (behind camera or behind ndbuffer)
	if (t2 < 0.0f || t1 > ndbuffer) return 0.0f;

	// clip integration segment from camera to ndbuffer
	t1 = std::max(t1, 0.0f);
	t2 = std::min(t2, ndbuffer);

	// analytical integration of an inverse squared density
	float i1 = -(c * t1 + b * t1 * t1 + t1 * t1 * t1 * (1.0f / 3.0f));
	float i2 = -(c * t2 + b * t2 * t2 + t2 * t2 * t2 * (1.0f / 3.0f));
	return (i2 - i1) * (3.0f / 4.0f);
}

// The map's texels on its surface: each row's line from its left edge to its
// right, as the light maps' (LightmapBuilder::CalcRowLines)
void FogmapBuilder::SetupRows(const Coords& MapCoords, const LightMapIndex& lmindex)
{
	float UDot = dot(MapCoords.XAxis, MapCoords.Origin);
	float VDot = dot(MapCoords.YAxis, MapCoords.Origin);
	float LMUPan = UDot + lmindex.PanX - 0.5f * lmindex.UScale;
	float LMVPan = VDot + lmindex.PanY - 0.5f * lmindex.VScale;
	float LMUMult = 1.0f / lmindex.UScale;
	float LMVMult = 1.0f / lmindex.VScale;

	corner[0] = MapCoords.Origin;
	corner[1] = MapCoords.Origin + MapCoords.XAxis;
	corner[2] = MapCoords.Origin + MapCoords.YAxis;
	for (int j = 0; j < 3; j++)
	{
		uv[j] =
		{
			(dot(MapCoords.XAxis, corner[j]) - LMUPan) * LMUMult,
			(dot(MapCoords.YAxis, corner[j]) - LMVPan) * LMVMult
		};
	}

	leftDY = uv[2].y - uv[0].y;
	leftStep = (uv[2].x - uv[0].x) / leftDY;
	rightDY = uv[2].y - uv[1].y;
	rightStep = (uv[2].x - uv[1].x) / rightDY;
}

// Row y's line: where texel 0's and the last's U lie along it, and the
// points at its two ends
void FogmapBuilder::RowLine(int y, float& x0, float& x1, vec3& p0, vec3& p1) const
{
	x0 = uv[0].x + leftStep * (y + 0.5f - uv[0].y);
	x1 = uv[1].x + rightStep * (y + 0.5f - uv[1].y);
	float t0 = (y + 0.5f - uv[0].y) / leftDY;
	float t1 = (y + 0.5f - uv[1].y) / rightDY;
	p0 = mix(corner[0], corner[2], t0);
	p1 = mix(corner[1], corner[2], t1);
	if (x1 < x0)
	{
		std::swap(x0, x1);
		std::swap(p0, p1);
	}
}

// Texel i at its centre along U, as the light maps'
vec3 FogmapBuilder::Location(int i, int y) const
{
	float x0, x1;
	vec3 p0, p1;
	RowLine(y, x0, x1, p0, p1);
	return mix(p0, p1, (i + 0.5f - x0) / (x1 - x0));
}

void FogmapBuilder::CalcWorldLocations()
{
	for (int y = 0; y < height; y++)
	{
		float x0, x1;
		vec3 p0, p1;
		RowLine(y, x0, x1, p0, p1);
		vec3* dest = &points[(size_t)y * width];
		for (int i = 0; i < width; i++)
		{
			float t = (i + 0.5f - x0) / (x1 - x0);
			dest[i] = mix(p0, p1, t);
		}
	}
}
