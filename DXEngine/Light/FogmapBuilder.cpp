
#include "Precomp.h"
#include "FogmapBuilder.h"
#include "Engine.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "RenderDevice/RenderDevice.h"
#include "LightmapBuilder.h"
#include <cmath>

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

FogLight FogmapBuilder::SetupLight(UActor* light)
{
	FogLight fog;
	fog.Actor = light;
	fog.Location = light->Location();
	fog.Radius = light->WorldVolumetricRadius();
	fog.RadiusSquared = fog.Radius * fog.Radius;
	fog.InvRadius = 1.0f / fog.Radius;
	fog.InvRadiusSquared = fog.InvRadius * fog.InvRadius;
	vec3 color;
	float brightness = LightmapBuilder::GlobalLighting(light, light->LightBrightness() * (1.0f / 255.0f), &color);
	fog.Color = color * (brightness * light->Level()->Brightness());
	fog.Density = light->VolumeBrightness() * brightness * 0.015625f;
	fog.Fog = light->VolumeFog() * (1.0f / 255.0f);
	return fog;
}

// The sphere against the pyramid from the eye to the map's corner texels,
// which holds every texel's centre, the map being a flat grid: wholly
// outside one of its four sides, or behind the surface's plane, it is
// crossed by no line of sight to a texel and fogs none (AddLight gives
// each nothing). A side seen edge on is passed over: taken as reaching.
bool FogmapBuilder::Reaches(const FogLight& light, const vec3& view) const
{
	vec3 center = light.Location;
	float radius = light.Radius;

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

// The light laid over the map as the original's SetupForSurf lays each, the
// lights farther along the view first (Render.dll 0x10b0716a-0x10b07c3f:
// the first one's texels 0x10b075a2, a later one's 0x10b078b6). A texel's
// density along its line of sight from the eye, through the light's sphere
// of radius R, is K x 3/R x the integral of 1 - r^2/R^2 over the part of
// the line within the sphere, r its distance from the light, held to 0..1
// (0x10b075dc-0x10b07824): none where the light lies behind the line from
// an eye outside the sphere, or the line misses it; else the part runs
// from the eye or the sphere's near side to the texel or its far side, its
// lengths in the light manager's square roots. The light's texel is 255 x
// the density x its colour and the density x VolumeFog, each held to 127,
// as the original's table of 256 entries makes it (SetupLight,
// 0x10b06a35-0x10b06b60); over the map, each channel its own plus (127 -
// its alpha)/127 of what lies under it, the alphas summed, each held to 127
// (0x10b07b47, FLightManager::Init's table 0x10b020e3-0x10b02105). In
// floats, by choice, where the original's texels and its table's steps are
// whole numbers.
void FogmapBuilder::AddLight(const FogLight& light, const vec3& view)
{
	Prepare();

	const float R2 = light.RadiusSquared;
	const float invR = light.InvRadius;
	const float invR2 = light.InvRadiusSquared;
	const float K = light.Density;
	const vec3 color = light.Color * 255.0f;
	const float fog = light.Fog * 255.0f;
	const vec3 L = light.Location - view;
	const float L2 = L.x * L.x + L.y * L.y + L.z * L.z;
	const bool inside = L2 < R2;

	size_t size = (size_t)width * height;
	const vec3* locations = WorldLocations();
	vec4* dest = fogcolors.data();
	for (size_t i = 0; i < size; i++)
	{
		vec3 p = locations[i] - view;
		float along = p.x * L.x + p.y * L.y + p.z * L.z;
		if (!inside && std::signbit(along))
			continue;
		float p2 = p.x * p.x + p.y * p.y + p.z * p.z;
		float d2 = L2 - along * along / p2;
		if (!(R2 >= d2))
			continue;
		float depth = LightmapBuilder::LightSqrt(p2);
		float nearest = along / depth;
		float half = LightmapBuilder::LightSqrt(R2 - d2);
		float lower = nearest - depth;
		float upper;
		bool whole = false;
		if (nearest <= half)
		{
			upper = nearest;
			if (lower < -half)
				lower = -half;
		}
		else
		{
			upper = half;
			if (lower < -half)
			{
				lower = -half;
				whole = true;
			}
		}
		if (!(lower < upper))
			continue;
		float a = d2 * invR2;
		float u = upper * invR;
		float density;
		if (whole)
		{
			density = u * (3.0f - a * 3.0f - u * u) * K;
			density = density + density;
		}
		else
		{
			float l = lower * invR;
			density = (u * (3.0f - a * 3.0f - u * u) - (3.0f - a * 3.0f - l * l) * l) * K;
		}
		if (!(density > 0.0f))
			continue;
		density = std::min(density, 1.0f);

		vec4 texel(std::min(density * color.r, 127.0f), std::min(density * color.g, 127.0f), std::min(density * color.b, 127.0f), std::min(density * fog, 127.0f));
		float under = (127.0f - texel.a) * (1.0f / 127.0f);
		dest[i].r = std::min(texel.r + dest[i].r * under, 127.0f);
		dest[i].g = std::min(texel.g + dest[i].g * under, 127.0f);
		dest[i].b = std::min(texel.b + dest[i].b * under, 127.0f);
		dest[i].a = std::min(texel.a + dest[i].a, 127.0f);
	}
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
