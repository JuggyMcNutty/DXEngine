
#include "Precomp.h"
#include "LightSystem.h"
#include "GC/GC.h"
#include <algorithm>
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Math/floating.h"
#include "Math/coords.h"
#include "Utils/Random.h"
#include "Engine.h"
#include "Packages/Engine/UDXEngineClient.h"

LightSystem::LightSystem()
{
}

LightSystem::~LightSystem()
{
}

void LightSystem::Tick(float frameElapsed)
{
	// The original's pulse: 0.25 + 0.2 sin(8 t), t its viewport's clock, the
	// real time, whatever the level's TimeDilation (Render.dll
	// FLightManager::SetupForActor; dx-reverse-info/render-dll.md, meshes;
	// GlowConsole: twelve shots at a hundredth of the level's time run
	// through the pulse's range)
	AmbientGlowTime = std::fmod(AmbientGlowTime + frameElapsed, 2.0f * 3.14159265359f / 8.0f);
	AmbientGlowAmount = 0.25f + 0.2f * std::sin(8.0f * AmbientGlowTime);
	LastFrameElapsed = frameElapsed;

	// The original's blink, strobe and flicker change with each frame it
	// draws (Render.dll GlobalLighting, 0x10b05e90), paused too, so faster at
	// a higher frame rate, and its torch and fire wavers take each frame's
	// draws (TickRandoms). DXEngine's step instead as the original's would at
	// 60 frames a second, by choice (docs/NATIVES.md, lighting): a step each
	// 60th of a second of this clock, whatever the level's TimeDilation or
	// pause, as the textures step (UTexture::Update); at most one a frame,
	// the frame nearest each 60th taking it, so that at 120 frames each state
	// holds two and under 60 every frame steps, as the original's does. A
	// blink's count is taken at the level's time of the step.
	const float step = 1.0f / 60.0f;
	StepTime += frameElapsed;
	if (StepTime + frameElapsed * 0.5f >= step)
	{
		StepTime = std::min(StepTime - step, step);
		Steps++;
		StepLevelTime = engine->LevelInfo ? engine->LevelInfo->TimeSeconds() : 0.0f;
	}
}

void LightSystem::OnMapLoaded()
{
	engine->Level->Light.FogBalls.clear();
	engine->Level->Light.FogLights.clear();
	engine->Level->Light.lmtextures.clear();
	engine->Level->Light.Atlas.Clear();
	engine->Level->Light.SurfaceLightCaches.clear();
	engine->Level->Light.fogtextures.clear();

	std::set<UActor*> lightset;
	for (UActor* light : engine->Level->Model->Lights)
	{
		if (light)
			lightset.insert(light);
	}

	for (UActor* light : lightset)
	{
		if (light->VolumeRadius() != 0)
			engine->Level->Light.FogBalls.push_back(light);
	}
}

void LightSystem::TickRandoms(float levelTime)
{
	int ticks = (int)(int64_t)(levelTime * 35.0f);

	// The torch and fire wavers' draws afresh at each of the lights' steps,
	// as the blink, strobe and flicker change, where the original's draw
	// each frame, faster at a higher frame rate (docs/NATIVES.md, lighting)
	if (RandomsStep != Steps)
	{
		RandomsStep = Steps;
		for (int i = 0; i < 256; i++)
			Randoms[i] = FRand();
	}

	// Under 16 ticks on: the entries the ticks went past take new draws to
	// ease toward, and the others move on a step a tick; else all start over
	if ((uint32_t)(16 * (ticks - RandomTicks)) < 0x100 && RandomTicks != 0)
	{
		int from = (uint8_t)(16 * RandomTicks);
		int to = (uint8_t)(16 * ticks);
		for (int i = from; i != to; i = (i + 1) & 255)
			ShimmerSteps[i] = (Randoms[i] - ShimmerRandoms[i]) * 0.0625f;
		float steps = (float)(uint8_t)(ticks - RandomTicks);
		int i = to;
		do
		{
			ShimmerRandoms[i] += steps * ShimmerSteps[i];
			i = (i + 1) & 255;
		} while (i != from);
	}
	else
	{
		for (int i = 0; i < 256; i++)
		{
			ShimmerRandoms[i] = FRand();
			ShimmerSteps[i] = (Randoms[i] - ShimmerRandoms[i]) * 0.0625f;
		}
	}
	for (int i = 0; i < 256; i++)
		ShimmerRandoms[i] = std::clamp(ShimmerRandoms[i], 0.0f, 1.0f);
	RandomTicks = ticks;
}

void LightSystem::BeginFrame()
{
	FrameCounter++;
	TickRandoms(engine->LevelInfo->TimeSeconds());

	LightTree.Lights.clear();
	for (UActor* actor : engine->Level->Actors)
	{
		if (!actor)
			continue;

		uint8_t lightType = actor->LightType();
		if (lightType == LT_None || actor->LightBrightness() == 0)
			continue;

		// A flickering light draws afresh at each of the lights' steps (Tick),
		// a draw of its own, where the original's takes each frame's, one of
		// the 256 its PreRender draws (TickRandoms, 0x10b13080) by the low
		// byte of the light's address: appFrand's, rand() over 32,767
		if (lightType == LT_Flicker && actor->Light.FlickerStep != Steps)
		{
			actor->Light.FlickerStep = Steps;
			actor->Light.FlickerValue = RandInt(32767) * (1.0f / 32767.0f);
		}

		// An animating light invalidates what was built from it -- unless
		// the client's NoDynamicLights stills it
		if (LightmapBuilder::LightAnimates(actor) && !engine->client->NoDynamicLights)
			actor->Light.LastUpdate = FrameCounter;

		LightTree.Lights.push_back(actor);
	}

	// The fog lights as they are now, as the original sets each up for the
	// fog maps it draws, every frame (FLightManager::SetupLight, Render.dll
	// 0x10b05fa0): the level's volumetric lights, which its leaves list
	// (LeafVolumetricLighting, 0x10b170e0), and each moving light in a fog
	// zone with a LightRadius, a VolumeRadius and a VolumeBrightness, which
	// SetupDynamics makes volumetric (0x10b232d8)
	FogLights.clear();
	for (UActor* light : FogBalls)
		FogLights.push_back(FogmapBuilder::SetupLight(light));
	for (UActor* light : LightTree.Lights)
	{
		bool moving = light->bDynamicLight() || (!light->bStatic() && !light->bNoDelete());
		UZoneInfo* zone = light->Region().Zone;
		if (moving && light->LightRadius() != 0 && light->VolumeRadius() != 0 && light->VolumeBrightness() != 0 &&
			zone && zone->bFogZone() && std::find(FogBalls.begin(), FogBalls.end(), light) == FogBalls.end())
		{
			FogLights.push_back(FogmapBuilder::SetupLight(light));
		}
	}

	// The tree is built from the lights, their order, locations and radii
	// alone: when none of those changed, last frame's tree is this frame's.
	bool sameLights = TreeLights.size() == LightTree.Lights.size();
	for (size_t i = 0; sameLights && i < TreeLights.size(); i++)
	{
		UActor* light = LightTree.Lights[i];
		const TreeLight& last = TreeLights[i];
		sameLights = last.Actor == light && last.Location == light->Location() && last.Radius == light->WorldLightRadius();
	}
	if (!sameLights)
	{
		TreeLights.clear();
		for (UActor* light : LightTree.Lights)
			TreeLights.push_back({ light, light->Location(), light->WorldLightRadius() });
		LightTree.CreateTLAS();
		LightTreeVersion++;
	}
}

void LightSystem::UpdateLightList(UActor* actor)
{
	// A mover's where it is drawn, its filing in the level's BSP
	UMover* mover = UObject::TryCast<UMover>(actor);
	vec3 location = mover ? mover->DrawnLocation() : actor->Location();

	if (!actor->TouchingLights.NeedsUpdate && actor->TouchingLights.Location == location)
		return;

	actor->TouchingLights.NeedsUpdate = false;
	actor->TouchingLights.Location = location;
	actor->TouchingLights.List.clear();

	if (actor->bUnlit())
		return;

	vec3 extents = actor->BspInfo.BoundingBox.extents();

	LightTree.CollectLights(location, std::max(extents.x, std::max(extents.y, extents.z)));
	for (UActor* light : LightTree.CollectedLights)
	{
		if (!light->bCorona() && light->bSpecialLit() == actor->bSpecialLit())
		{
			float radius = light->WorldLightRadius();
			vec3 L = light->Location() - location;
			if (dot(L, L) < radius * radius && !engine->Level->Collision.TraceAnyHit(light->Location(), location, actor, false, true, true))
			{
				actor->TouchingLights.List.push_back(light);
			}
		}
	}
}

void LightSystem::SetLevel(ULevel* level)
{
	Level = level;
}

void LightSystem::PurgeDying()
{
	auto dying = [](UActor* actor) { return GC::IsDying(actor); };
	FogBalls.erase(std::remove_if(FogBalls.begin(), FogBalls.end(), dying), FogBalls.end());
	FogLights.erase(std::remove_if(FogLights.begin(), FogLights.end(), [](const FogLight& light) { return GC::IsDying(light.Actor); }), FogLights.end());
	TempDynLightList.clear();
	bool stale = false;
	for (const TreeLight& light : TreeLights)
		stale = stale || GC::IsDying(light.Actor);
	for (auto it = SurfaceLightCaches.begin(); it != SurfaceLightCaches.end();)
		it = GC::IsDying(it->first) ? SurfaceLightCaches.erase(it) : std::next(it);
	if (stale)
	{
		TreeLights.clear();
		SurfaceLightCaches.clear();
		// The tree's nodes go with its lights, so that no query before the
		// next BeginFrame walks them into the emptied vector, a read of the
		// purged light.
		LightTree.Clear();
		LightTreeVersion++;
	}
}
