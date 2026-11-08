#pragma once

#include "Math/vec.h"
#include "LightActorTree.h"
#include "LightmapBuilder.h"
#include "FogmapBuilder.h"
#include "LightmapAtlas.h"
#include "Packages/Engine/Resources/Textures/UBitmap.h"
#include <unordered_map>
#include <list>

#ifdef USE_SSE2
#include <immintrin.h>
#endif

class ULevel;
class UActor;
class CollisionHitList;
class CollisionHit;
struct TextureInfo;
class UMover;

struct LightmapTexture
{
	TextureFormat Format;
	UnrealMipmap Mip;
	int LastStaticUpdate = -1;
	int LastDynamicUpdate = -1;
	// The moving lights it was built with, as a set (LightSystem::GetLightmap)
	uint64_t MovingLightSet = 0;
	Array<vec3> StaticLightColors;

	// Where the lights added over the static map reached when it was last
	// built: outside it, the map is the static map
	LightmapRect AddedRect;

	// Its place in a shared page (LightmapAtlas)
	LightmapAtlas::Place AtlasPlace;
};

class VertexLight
{
public:
	// The original's per-vertex formula (dx-reverse-info/render-dll.md, lighting):
	// a diffuse term, (cos + 1)^2 - 1.5 of the angle to the light, and a
	// highlight, 6 cos^2 of the angle between the eye and the light's
	// reflection when it heads toward the eye, both times 1 - d/r and the
	// light's colour; the sum scaled by 1.4 ScaleGlow, the ambient added,
	// each channel at most 1. An unlit draw is AmbientGlow / 256 + ScaleGlow
	// / 2, mid-grey for most. A vertex is lit by its normal alone, whatever
	// its faces' flags, as the original's Light (Render.dll 0x10b027f0) lights
	// each once: a two-sided face shows on both sides the light its normal
	// gets.
	vec3 GetVertexLight(const vec3& location, const vec3& normal, bool unlit)
	{
		if (unlit)
			return vec3(UnlitLevel);

		vec3 sum(0.0f);
		vec3 eye = CameraLocation - location;
		float eyeLen2 = dot(eye, eye);
		vec3 eyeDir = eyeLen2 > 0.0f ? eye / std::sqrt(eyeLen2) : vec3(0.0f);
		for (int i = 0, count = NumLights; i < count; i++)
		{
			vec3 L = Lights[i].Location - location;
			float lensqr = dot(L, L);
			if (lensqr <= 0.0f)
				continue;
			float dist = std::sqrt(lensqr);
			float attenuation = 1.0f - dist * Lights[i].InvRadius;
			if (attenuation <= 0.0f)
				continue;
			vec3 Ldir = L / dist;
			float c = dot(Ldir, normal);
			float diffuse = (c + 1.0f) * (c + 1.0f) - 1.5f;
			if (diffuse < 0.0f)
				diffuse = 0.0f;
			float highlight = 0.0f;
			float ce = dot(reflect(-Ldir, normal), eyeDir);
			if (ce > 0.0f)
				highlight = 6.0f * ce * ce;
			if (diffuse + highlight > 0.0f)
				sum += Lights[i].Color * ((diffuse + highlight) * attenuation);
		}
		vec3 color = sum * (1.4f * ScaleGlow) + AmbientColor;
		color.r = std::min(color.r, 1.0f);
		color.g = std::min(color.g, 1.0f);
		color.b = std::min(color.b, 1.0f);
		return color;
	}

	// Light:

	vec3 AmbientColor;
	float ScaleGlow;
	// What an unlit draw is lit with in the original's formula
	float UnlitLevel = 0.5f;
	enum { MaxLights = 16 };
	struct Light
	{
		vec3 Location;
		float InvRadiusSquared;
		float InvRadius;
		vec3 Color;
	} Lights[MaxLights];
	int NumLights;

	// The eye, for the highlight
	vec3 CameraLocation;
};

class LightSystem
{
public:
	LightSystem();
	~LightSystem();

	// By the viewport's clock: the real time since the last frame drawn.
	// The meshes' glow pulse, and the lights' steps.
	void Tick(float frameElapsed);

	// The blink, strobe and flicker lights' pace, and the torch and fire
	// wavers': the original's change at each frame it draws, DXEngine's at
	// its steps, as the original's would at 60 frames a second (Tick). Steps
	// counts them, a strobe out at an odd one; StepLevelTime is the level's
	// time at the last, which a blink's turn count is taken at.
	int Steps = 0;
	float StepLevelTime = 0.0f;

	void OnMapLoaded();

	void BeginFrame();
	void UpdateLightList(UActor* actor);

	void SetLevel(ULevel* level);

	TextureInfo GetMoverLightmap(UMover* mover, const Poly& poly, UZoneInfo* zoneActor, UModel* model);
	TextureInfo GetLevelLightmap(BspSurface& surface, UZoneInfo* zoneActor, UModel* model);

	TextureInfo GetMoverFogmap(UMover* mover, const Poly& poly, UZoneInfo* zoneActor, UModel* model);
	TextureInfo GetLevelFogmap(BspSurface& surface, UZoneInfo* zoneActor, UModel* model);

	// The light of a mesh drawn for drawn: its unlit faces at drawn's glow,
	// the rest by the lights, glow and ambient of actor, its light sink -- a
	// held item's the pawn holding it. leafLights: whether the moving lights
	// near the actor are candidates, as the scene's draws give them; an
	// actor drawn on its own has none (SetupForActorDX)
	void InitVertexLight(VertexLight& vertexlight, UActor* drawn, UActor* actor, UZoneInfo* zoneActor, bool leafLights = true);
	void SetupForActorDX(VertexLight& out, UActor* actor, bool leafLights);

	// The eye of the frame drawn now (VisibleFrame::Draw), its meshes'
	// highlights and fog and its fog maps' lines of sight taken from it, as
	// the original's frame draws from its own eye (render-dll.md, a mirror's
	// frame): the main view's, a mirror's, a sky's, a viewport window's. view
	// is the frame's place among the scene's frames, 0 the main view's: each
	// keeps fog maps of its own. worldToView gives a fog light's depth along
	// the frame's view.
	void SetEye(const vec3& eye, int view, const mat4& worldToView) { Eye = eye; EyeView = view; EyeWorldToView = worldToView; }

	// Deus Ex's random tables (Render.dll's TickRandoms, 0x10b13080), from
	// the level's time each frame: 256 fresh draws -- the torch and fire
	// wavers' --, at each of the lights' steps (Tick) where the original's
	// are each frame's, and the watery shimmer's own, each entry easing
	// toward a draw from those, 16 entries taking a new one at each of 35
	// ticks a second, a sixteenth of the way a tick.
	void TickRandoms(float levelTime);
	float Randoms[256] = {};
	float ShimmerRandoms[256] = {};
	float ShimmerSteps[256] = {};
	int RandomTicks = 0;
	int RandomsStep = -1;

private:
	TextureInfo GetLightmap(UModel* model, int lightmapIndex, const Coords& coords, UZoneInfo* zoneActor, const vec3& worldLocation, float radius, UMover* mover, bool specialLit);
	TextureInfo GetFogmap(UModel* model, int lightmapIndex, const Coords& coords, UZoneInfo* zoneActor);
	void CheckLight(UActor* light);

public:
	// The collector's: lights about to be freed leave every list; the tree
	// and its answers are built again.
	void PurgeDying();

private:
	struct SurfaceLights;
	SurfaceLights& GetSurfaceLights(UModel* model, int lightmapIndex);
	const Array<UActor*>& CollectSurfaceLights(SurfaceLights& entry, const vec3& center, float radius);
	bool UpdateFogmapTexture(uint32_t* texels, UModel* model, const Coords& mapCoords, int lightMap, UZoneInfo* zoneActor, bool& clear);

	ULevel* Level = nullptr;

	LightmapBuilder Builder;
	std::map<uint64_t, std::unique_ptr<LightmapTexture>> lmtextures;
	LightmapAtlas Atlas;
	// A fog map: the frame it was last built in, and whether its texels are
	// all clear, as a new one's are
	struct FogmapEntry
	{
		int Frame = -1;
		bool Clear = true;
		std::unique_ptr<LightmapTexture> Texture;
	};
	std::map<uint64_t, FogmapEntry> fogtextures;
	FogmapBuilder FogBuilder;
	// The level's volumetric lights, from its lights at its load
	Array<UActor*> FogBalls;
	// The fog lights as they are this frame (BeginFrame), and those that
	// reach a map with their depths along the view (UpdateFogmapTexture)
	Array<FogLight> FogLights;
	struct ReachedFog
	{
		const FogLight* Light;
		float Depth;
	};
	Array<ReachedFog> FogReached;
	vec3 Eye = vec3(0.0f);
	int EyeView = 0;
	mat4 EyeWorldToView = mat4::identity();
	int FrameCounter = 0;
	int LightmapCheckCounter = 0;
	int NextMoverID = 1;

	float AmbientGlowTime = 0.0f;
	float AmbientGlowAmount = 0.0f;
	float LastFrameElapsed = 0.0f;
	// The real time toward the lights' next step (Tick)
	float StepTime = 0.0f;

	Array<UActor*> TempDynLightList;
	Array<int> TempAnimatedIndexList;
	LightActorTree LightTree;

	// What the light tree was last built from: each light, where it is and
	// how far it reaches, in order. While that stays the same, so do the tree
	// and its every answer; LightTreeVersion counts the builds.
	struct TreeLight
	{
		UActor* Actor;
		vec3 Location;
		float Radius;
	};
	Array<TreeLight> TreeLights;
	uint64_t LightTreeVersion = 0;

	// Each lightmap's lights from the tree that can move, as of the version
	// they were found in, and its map as last drawn: the cache ID it was
	// asked for under and its place in lmtextures, which keeps it until the
	// next level. For each model: the level's and every mover's brush.
	struct SurfaceLights
	{
		uint64_t Version = 0;
		vec3 Center = vec3(0.0f);
		float Radius = 0.0f;
		Array<UActor*> Lights;

		uint64_t TextureID = 0;
		std::unique_ptr<LightmapTexture>* Texture = nullptr;
	};
	std::unordered_map<UModel*, Array<SurfaceLights>> SurfaceLightCaches;
};
