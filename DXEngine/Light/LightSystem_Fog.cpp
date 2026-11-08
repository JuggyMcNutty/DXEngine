
#include "Precomp.h"
#include "LightSystem.h"
#include "RenderDevice/RenderDevice.h"
#include "FogmapBuilder.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Info//UZoneInfo.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"
#include "Engine.h"

#ifndef NOSSE
#include <immintrin.h>
#endif

// Don't render the fog in debug builds as we'd rather have a higher frame rate
#if defined(DEBUG) || defined(_DEBUG)
#define NOFOG
#endif

TextureInfo LightSystem::GetMoverFogmap(UMover* mover, const Poly& poly, UZoneInfo* zoneActor, UModel* model)
{
	Coords localCoords;
	localCoords.Origin = -poly.Base;
	localCoords.XAxis = poly.TextureU;
	localCoords.YAxis = poly.TextureV;
	localCoords.ZAxis = poly.Normal;

	mat4 objectToWorld = mover->BrushToWorld() * localCoords.ToMatrix();
	Coords worldCoords = Coords::FromMatrix(objectToWorld);
	return GetFogmap(model, poly.BrushPolyIndex, worldCoords, zoneActor);
}

TextureInfo LightSystem::GetLevelFogmap(BspSurface& surface, UZoneInfo* zoneActor, UModel* model)
{
	Coords mapCoords;
	mapCoords.Origin = model->Points[surface.pBase];
	mapCoords.XAxis = model->Vectors[surface.vTextureU];
	mapCoords.YAxis = model->Vectors[surface.vTextureV];
	mapCoords.ZAxis = model->Vectors[surface.vNormal];
	return GetFogmap(model, surface.LightMap, mapCoords, zoneActor);
}

TextureInfo LightSystem::GetFogmap(UModel* model, int lightmapIndex, const Coords& coords, UZoneInfo* zoneActor)
{
#ifdef NOFOG
	return {};
#else
	if (!zoneActor || !zoneActor->bFogZone() || lightmapIndex < 0)
		return {};

	uint32_t ambientID = (((uint32_t)zoneActor->AmbientHue()) << 16) | (((uint32_t)zoneActor->AmbientSaturation()) << 8) | (uint32_t)zoneActor->AmbientBrightness();
	// One a frame of the scene, its lines of sight its eye's: the low byte's
	// bits above its tag (2) the frame's place
	uint64_t cacheID = (((uint64_t)model->LightMap[lightmapIndex].LMCacheID) << 32) | (((uint64_t)ambientID) << 8) | ((uint64_t)(EyeView & 63) << 2) | 2;

	const LightMapIndex& lmindex = model->LightMap[lightmapIndex];
	FogmapEntry& fogtex = fogtextures[cacheID];
	std::unique_ptr<LightmapTexture>& fogtexture = fogtex.Texture;
	if (!fogtexture)
	{
#if 1 // Float high quality lightmaps
		UnrealMipmap fogmip;
		fogmip.Width = lmindex.UClamp;
		fogmip.Height = lmindex.VClamp;
		fogmip.Data.resize((size_t)fogmip.Width * fogmip.Height * sizeof(vec4));

		fogtexture = std::make_unique<LightmapTexture>();
		fogtexture->Format = TextureFormat::RGBA32_F;
		fogtexture->Mip = std::move(fogmip);
#else // Low quality lightmaps like UE1 got them
		UnrealMipmap fogmip;
		fogmip.Width = lmindex.UClamp;
		fogmip.Height = lmindex.VClamp;
		fogmip.Data.resize((size_t)fogmip.Width * fogmip.Height * 4);

		fogtexture = std::make_unique<LightmapTexture>();
		fogtexture->Format = TextureFormat::BGRA8_LM;
		fogtexture->Mip = std::move(fogmip);
#endif
	}

	bool firstDrawThisScene = false;
	if (fogtex.Frame != FrameCounter)
	{
		firstDrawThisScene = true;
		fogtex.Frame = FrameCounter;
	}

	// Built each frame it is drawn, but sent again only when it changed: a map
	// no fog light reaches stays clear
	bool changed = false;
	if (firstDrawThisScene)
	{
		changed = UpdateFogmapTexture((uint32_t*)fogtexture->Mip.Data.data(), model, coords, lightmapIndex, zoneActor, fogtex.Clear);
	}

	TextureInfo texinfo;
	texinfo.CacheID = cacheID;
	texinfo.bRealtimeChanged = changed;
	texinfo.Format = fogtexture->Format;
	texinfo.Mips = &fogtexture->Mip;
	texinfo.NumMips = 1;
	texinfo.USize = texinfo.Mips[0].Width;
	texinfo.VSize = texinfo.Mips[0].Height;
	texinfo.Pan = { lmindex.PanX, lmindex.PanY };
	texinfo.UScale = lmindex.UScale;
	texinfo.VScale = lmindex.VScale;
	return texinfo;
#endif
}

// Only the fog lights a line of sight to the map can cross go into it, the
// rest, and a light of no density, adding nothing to any texel; they go in
// farthest first along the view, as the original sorts them by their depth
// (Render.dll 0x10b073ad, its order 0x10b06c60). With none, the map is all
// clear: cleared if it held fog, else left as it is. The surface keeps its
// map either way, as the original's, whose passes go by whether it has one.
// Whether the texels changed; clear, whether they are all clear now.
bool LightSystem::UpdateFogmapTexture(uint32_t* dest, UModel* model, const Coords& mapCoords, int lightMap, UZoneInfo* zoneActor, bool& clear)
{
	FogmapBuilder& builder = FogBuilder;
	builder.Setup(model, mapCoords, lightMap, zoneActor);

	FogReached.clear();
	for (const FogLight& light : FogLights)
	{
		if (light.Density > 0.0f && builder.Reaches(light, Eye))
			FogReached.push_back({ &light, (EyeWorldToView * vec4(light.Location, 1.0f)).z });
	}
	std::stable_sort(FogReached.begin(), FogReached.end(), [](const ReachedFog& a, const ReachedFog& b) { return a.Depth > b.Depth; });
	for (const ReachedFog& reached : FogReached)
		builder.AddLight(*reached.Light, Eye);
	if (FogReached.empty())
	{
		if (clear)
			return false;
		clear = true;
		memset(dest, 0, (size_t)builder.Width() * builder.Height() * sizeof(vec4));
		return true;
	}
	clear = false;

	// Each channel drawn at 2c/255 of the original's 0 to 127, as D3DDrv
	// draws a fog map's seven bits
#if 1 // Float high quality lightmaps
	size_t size = (size_t)builder.Width() * builder.Height();
	const vec4* src = builder.Pixels();
	vec4* out = (vec4*)dest;
	for (size_t i = 0; i < size; i++)
		out[i] = src[i] * (2.0f / 255.0f);
#else // Low quality lightmaps like UE1 got them
	size_t size = (size_t)builder.Width() * builder.Height();
	const vec4* src = builder.Pixels();
	for (size_t i = 0; i < size; i++)
	{
		uint32_t red = (uint32_t)clamp(src[i].r, 0.0f, 127.0f);
		uint32_t green = (uint32_t)clamp(src[i].g, 0.0f, 127.0f);
		uint32_t blue = (uint32_t)clamp(src[i].b, 0.0f, 127.0f);
		uint32_t alpha = (uint32_t)clamp(src[i].a, 0.0f, 127.0f);

		dest[i] = (alpha << 24) | (red << 16) | (green << 8) | blue;
	}
#endif
	return true;
}
