
#include "Precomp.h"
#include "LightSystem.h"
#include "RenderDevice/RenderDevice.h"
#include "Engine.h"
#include "Packages/Engine/UDXEngineClient.h"
#include "Render/RenderSubsystem.h"
#include "Math/hsb.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Resources/Level/UPolys.h"
#include "Packages/Engine/Resources/Level/UModel.h"

TextureInfo LightSystem::GetMoverLightmap(UMover* mover, const Poly& poly, UZoneInfo* zoneActor, UModel* model)
{
	Coords localCoords;
	localCoords.Origin = -poly.Base;
	localCoords.XAxis = poly.TextureU;
	localCoords.YAxis = poly.TextureV;
	localCoords.ZAxis = poly.Normal;

	vec3 scale = mover->MainScale().Scale;

	mat4 brushToWorld;
	if (mover->bDynamicLightMover())
	{
		vec3 moverLocation = mover->Location();
		Rotator moverRotation = mover->Rotation();
		brushToWorld = mat4::translate(moverLocation) * Coords::Rotation(moverRotation).ToMatrix() * mat4::scale(scale) * mat4::translate(-mover->PrePivot());
	}
	else
	{
		// The key held to the 8 (a map file sets BrushRaytraceKey; one over 7
		// read past the arrays)
		int key = std::min<int>(mover->BrushRaytraceKey(), 7);
		vec3 moverLocation = mover->BasePos() + mover->KeyPos()[key];
		Rotator moverRotation = mover->BaseRot() + mover->KeyRot()[key];
		brushToWorld = mat4::translate(moverLocation) * Coords::Rotation(moverRotation).ToMatrix() * mat4::scale(scale) * mat4::translate(-mover->PrePivot());
	}
	mat4 objectToWorld = brushToWorld * localCoords.ToMatrix();
	Coords worldCoords = Coords::FromMatrix(objectToWorld);

	// The sphere the moving lights are gathered in: the whole brush's, in its
	// own space, put in the world with the brush (not one polygon's, through
	// that polygon's texture space)
	if (!mover->Light.Calculated)
	{
		vec3 location = { 0.0f };
		float radius = 0.0f;
		if (UModel* brush = mover->Brush())
		{
			location = brush->BoundingBox.center();
			radius = length(brush->BoundingBox.extents());
		}
		mover->Light.Calculated = true;
		mover->Light.Center = location;
		mover->Light.Radius = radius;
	}

	return GetLightmap(
		model, poly.BrushPolyIndex, worldCoords, zoneActor,
		(brushToWorld * vec4(mover->Light.Center, 1.0f)).xyz(),
		mover->Light.Radius * std::max(scale.x, std::max(scale.y, scale.z)),
		mover->bDynamicLightMover() ? mover : nullptr, mover->bSpecialLit());
}

TextureInfo LightSystem::GetLevelLightmap(BspSurface& surface, UZoneInfo* zoneActor, UModel* model)
{
	Coords mapCoords;
	mapCoords.Origin = model->Points[surface.pBase];
	mapCoords.XAxis = model->Vectors[surface.vTextureU];
	mapCoords.YAxis = model->Vectors[surface.vTextureV];
	mapCoords.ZAxis = model->Vectors[surface.vNormal];
	return GetLightmap(model, surface.LightMap, mapCoords, zoneActor, surface.Center, surface.Radius, nullptr, surface.PolyFlags & PF_SpecialLit);
}

// What is kept for a lightmap of a model: the level's and every mover's
// brush, drawn in turn every frame, each keep their own.
LightSystem::SurfaceLights& LightSystem::GetSurfaceLights(UModel* model, int lightmapIndex)
{
	Array<SurfaceLights>& cache = SurfaceLightCaches[model];
	if (cache.size() <= (size_t)lightmapIndex)
		cache.resize(std::max(model->LightMap.size(), (size_t)lightmapIndex + 1));
	return cache[lightmapIndex];
}

// The tree's lights that can move, for a lightmap's surface. Every visible
// surface asked the tree again every frame (~6 ms a frame on the handheld);
// its answer is now kept until the tree is rebuilt, or the surface's sphere
// differs. Only a light that can move is ever taken from it -- a static or
// no-delete one lights a map through the surface's own list -- so the
// others are left out here once, not passed over every frame.
const Array<UActor*>& LightSystem::CollectSurfaceLights(SurfaceLights& entry, const vec3& center, float radius)
{
	if (entry.Version != LightTreeVersion || entry.Center != center || entry.Radius != radius)
	{
		LightTree.CollectLights(center, radius);
		entry.Lights.clear();
		for (UActor* light : LightTree.CollectedLights)
		{
			if (!light->bStatic() && !light->bNoDelete())
				entry.Lights.push_back(light);
		}
		entry.Version = LightTreeVersion;
		entry.Center = center;
		entry.Radius = radius;
	}
	return entry.Lights;
}

TextureInfo LightSystem::GetLightmap(UModel* model, int lightmapIndex, const Coords& coords, UZoneInfo* zoneActor, const vec3& worldLocation, float radius, UMover* dynamicMover, bool specialLit)
{
	if (lightmapIndex < 0)
		return {};

	LightMapIndex& lmindex = model->LightMap[lightmapIndex];

	uint32_t ambientID = (((uint32_t)zoneActor->AmbientHue()) << 16) | (((uint32_t)zoneActor->AmbientSaturation()) << 8) | (uint32_t)zoneActor->AmbientBrightness();
	uint64_t cacheID = (((uint64_t)lmindex.LMCacheID) << 32) | (((uint64_t)ambientID) << 8) | 1;

	if (dynamicMover)
	{
		// Place mover ID in upper 12 bits (4096 movers). This leaves 20 bits (1 million) for the lightmaps.
		if (dynamicMover->Light.MoverID == 0)
			dynamicMover->Light.MoverID = NextMoverID++;
		cacheID |= ((uint64_t)dynamicMover->Light.MoverID) << 52;
	}

	// The counter marks each light checked in this lookup. Before it would
	// overflow (some hours at a few thousand surfaces a frame) every light's
	// mark goes back to never visited and it starts again: a wrapped counter
	// would be undefined, and at -1 would skip a never-visited light.
	if (LightmapCheckCounter == INT32_MAX)
	{
		for (UActor* actor : Level->Actors)
		{
			if (actor)
				actor->Light.LightmapCheckCounter = -1;
		}
		LightmapCheckCounter = 0;
	}
	int checkCounter = LightmapCheckCounter++;
	SurfaceLights& surface = GetSurfaceLights(model, lightmapIndex);

	// Collect lights for the lightmap and check if they changed

	int lastStaticUpdate = -100;
	int lastDynamicUpdate = -100;
	bool noDynamicLights = engine->client->NoDynamicLights;
	TempDynLightList.clear();
	TempAnimatedIndexList.clear();

	if (!dynamicMover)
	{
		// The surface's own lights: the still ones are the kept static
		// map's; an animating one is added over it every frame, through
		// its shadow bits (dx-reverse-info/render-dll.md, light maps). With
		// NoDynamicLights, animated lights count as static and moving
		// ones are left out.

		if (lmindex.LightActors >= 0)
		{
			UActor** lightlist = &model->Lights[lmindex.LightActors];
			for (int lightindex = 0; lightlist[lightindex] != nullptr; lightindex++)
			{
				UActor* light = lightlist[lightindex];
				CheckLight(light);
				if (!noDynamicLights && LightmapBuilder::LightAnimates(light))
				{
					TempAnimatedIndexList.push_back(lightindex);
					lastDynamicUpdate = FrameCounter;
				}
				else
				{
					lastStaticUpdate = std::max(lastStaticUpdate, light->Light.LastUpdate);
				}

				// Mark the light as visited so the second pass doesn't pick it up
				light->Light.LightmapCheckCounter = checkCounter;
			}
		}

		// The moving lights over the surface go into the dynamic light list:

		if (!noDynamicLights)
		{
			for (UActor* light : CollectSurfaceLights(surface, worldLocation, radius))
			{
				if (light->Light.LightmapCheckCounter != checkCounter)
				{
					light->Light.LightmapCheckCounter = checkCounter;
					if (light->bSpecialLit() == specialLit)
					{
						CheckLight(light);
						lastDynamicUpdate = std::max(lastDynamicUpdate, light->Light.LastUpdate);
						TempDynLightList.push_back(light);
					}
				}
			}
		}
	}
	else
	{
		// A mover's maps are rebuilt when the mover moved or turned
		// since, or one of its lights changed
		if (!dynamicMover->Light.HasLastTransform ||
			dynamicMover->Light.LastLocation != dynamicMover->Location() ||
			dynamicMover->Light.LastRotation != dynamicMover->Rotation())
		{
			dynamicMover->Light.HasLastTransform = true;
			dynamicMover->Light.LastLocation = dynamicMover->Location();
			dynamicMover->Light.LastRotation = dynamicMover->Rotation();
			dynamicMover->Light.MovedFrame = FrameCounter;
		}
		lastDynamicUpdate = dynamicMover->Light.MovedFrame;

		for (UActor* light : dynamicMover->TouchingLights.List)
		{
			CheckLight(light);
			lastDynamicUpdate = std::max(lastDynamicUpdate, light->Light.LastUpdate);
			TempDynLightList.push_back(light);
		}
	}

	// The moving lights over the map as a set, whichever order they came
	// in: one that left it -- switched off, destroyed, moved or shrunk out
	// of reach -- changes the set where it need not change the newest
	// stamp, and the map is built again without it, as the original's
	// dynamic map, built every frame from the lights then over it, drops it
	uint64_t movingSet = TempDynLightList.size();
	for (UActor* light : TempDynLightList)
	{
		uint64_t h = (uint64_t)(uintptr_t)light;
		h ^= h >> 30; h *= 0xbf58476d1ce4e5b9ull; h ^= h >> 27; h *= 0x94d049bb133111ebull; h ^= h >> 31;
		movingSet ^= h;
	}

	// If anything changed update the lightmap:

	bool bRealtimeChanged = false;
	LightmapRect updated;
	// The map: the surface's last one, else the cache's search
	if (!surface.Texture || surface.TextureID != cacheID)
	{
		surface.Texture = &lmtextures[cacheID];
		surface.TextureID = cacheID;
	}
	std::unique_ptr<LightmapTexture>& lmtexture = *surface.Texture;
	if (!lmtexture || lmtexture->LastStaticUpdate != lastStaticUpdate || lmtexture->LastDynamicUpdate != lastDynamicUpdate || lmtexture->MovingLightSet != movingSet)
	{
		engine->render->Stats.LightmapsUpdated++;

		Builder.Setup(model, coords, lightmapIndex);
		Builder.FindAddedLights(model, lightmapIndex, TempAnimatedIndexList, TempDynLightList);

		if (!lmtexture || lmtexture->Mip.Width != Builder.Width() || lmtexture->Mip.Height != Builder.Height())
		{
			lmtexture = std::make_unique<LightmapTexture>();
			lmtexture->Format = TextureFormat::RGBA32_F;
			lmtexture->Mip.Width = Builder.Width();
			lmtexture->Mip.Height = Builder.Height();
			lmtexture->Mip.Data.resize((size_t)lmtexture->Mip.Width * lmtexture->Mip.Height * sizeof(vec4));
		}

		// When only the animating and moving lights changed, the map is
		// built again only where they reach now or reached when it was last
		// built -- the kept static map, the lights over it -- and only that
		// part is converted and uploaded. A mover's map is built whole.
		bool partial = !dynamicMover && lmtexture->LastStaticUpdate == lastStaticUpdate && lmtexture->StaticLightColors.size() == (size_t)Builder.Width() * Builder.Height();
		if (partial)
		{
			updated = Builder.AddedLightsRect();
			updated.Add(lmtexture->AddedRect);
			Builder.CalcWorldLocations(updated);
			Builder.LoadStaticLight(lmtexture->StaticLightColors, updated);
		}
		else
		{
			updated = { 0, 0, Builder.Width(), Builder.Height() };
			Builder.CalcWorldLocations(updated);
			Builder.SetAmbientLight(zoneActor);
			if (!dynamicMover)
			{
				Builder.AddStaticLights(model, lightmapIndex);
				Builder.SaveStaticLight(lmtexture->StaticLightColors);
			}
		}

		Builder.AddLights(model, lightmapIndex);

		UnrealMipmap& lmmip = lmtexture->Mip;
		const vec3* pixels = Builder.Pixels();
		// Deus Ex's maps are the original's bytes, up to 127: a 255th each,
		// which the light map's doubling on the screen makes D3DDrv's 2/255
		float scale = 1.0f / 255.0f;
		for (int y = updated.y0; y < updated.y1; y++)
		{
			vec4* dest = (vec4*)lmmip.Data.data() + (size_t)y * lmmip.Width;
			const vec3* src = pixels + (size_t)y * lmmip.Width;
			for (int x = updated.x0; x < updated.x1; x++)
			{
				dest[x].r = std::min(src[x].r * scale, 1.0f);
				dest[x].g = std::min(src[x].g * scale, 1.0f);
				dest[x].b = std::min(src[x].b * scale, 1.0f);
				dest[x].a = 1.0f;
			}
		}

		lmtexture->AddedRect = Builder.AddedLightsRect();
		lmtexture->LastStaticUpdate = lastStaticUpdate;
		lmtexture->LastDynamicUpdate = lastDynamicUpdate;
		lmtexture->MovingLightSet = movingSet;
		bRealtimeChanged = !updated.Empty();
	}

	// The map's place in a shared page, found when it is first built at its
	// size: what changed written there, and the page drawn in its stead, at
	// the map's place (LightmapAtlas). A map too large for a page is drawn
	// as it is.
	if (bRealtimeChanged && lmtexture->AtlasPlace.Page < 0)
	{
		lmtexture->AtlasPlace = Atlas.Allocate(lmtexture->Mip.Width, lmtexture->Mip.Height);
		if (lmtexture->AtlasPlace.Page >= 0)
			updated = { 0, 0, lmtexture->Mip.Width, lmtexture->Mip.Height };
	}
	const LightmapAtlas::Place& place = lmtexture->AtlasPlace;
	if (place.Page >= 0)
	{
		LightmapRect written;
		if (bRealtimeChanged)
			Atlas.Write(place, lmtexture->Mip, updated, written);

		TextureInfo texinfo;
		texinfo.bRealtimeChanged = !written.Empty();
		texinfo.UpdateX = written.x0;
		texinfo.UpdateY = written.y0;
		texinfo.UpdateWidth = written.x1 - written.x0;
		texinfo.UpdateHeight = written.y1 - written.y0;
		texinfo.CacheID = Atlas.PageCacheID(place.Page);
		texinfo.Format = lmtexture->Format;
		texinfo.Mips = Atlas.PageMip(place.Page);
		texinfo.NumMips = 1;
		texinfo.USize = LightmapAtlas::PageSize;
		texinfo.VSize = LightmapAtlas::PageSize;
		texinfo.Pan = { lmindex.PanX - place.X * lmindex.UScale, lmindex.PanY - place.Y * lmindex.VScale };
		texinfo.UScale = lmindex.UScale;
		texinfo.VScale = lmindex.VScale;
		return texinfo;
	}

	TextureInfo texinfo;
	texinfo.bRealtimeChanged = bRealtimeChanged;
	if (bRealtimeChanged && (updated.x1 - updated.x0 < lmtexture->Mip.Width || updated.y1 - updated.y0 < lmtexture->Mip.Height))
	{
		texinfo.UpdateX = updated.x0;
		texinfo.UpdateY = updated.y0;
		texinfo.UpdateWidth = updated.x1 - updated.x0;
		texinfo.UpdateHeight = updated.y1 - updated.y0;
	}
	texinfo.CacheID = cacheID;
	texinfo.Format = lmtexture->Format;
	texinfo.Mips = &lmtexture->Mip;
	texinfo.NumMips = 1;
	texinfo.USize = texinfo.Mips[0].Width;
	texinfo.VSize = texinfo.Mips[0].Height;
	texinfo.Pan = { lmindex.PanX, lmindex.PanY };
	texinfo.UScale = lmindex.UScale;
	texinfo.VScale = lmindex.VScale;
	return texinfo;
}

void LightSystem::CheckLight(UActor* light)
{
	if (light->Light.LastCheck == FrameCounter)
		return;

	light->Light.LastCheck = FrameCounter;

	vec3 location = light->Location();
	float radius = light->WorldLightRadius();
	uint8_t type = light->LightType();
	uint8_t effect = light->LightEffect();
	uint8_t hue = light->LightHue();
	uint8_t saturation = light->LightSaturation();
	uint8_t brightness = light->LightBrightness();
	// Its aim, cone, period and phase too: a spotlight that turns (the
	// alarm lights) or whose cone a script changes lights its surfaces anew,
	// as the original's dynamic map, built every frame, has it
	Rotator rotation = light->Rotation();
	uint8_t cone = light->LightCone();
	uint8_t period = light->LightPeriod();
	uint8_t phase = light->LightPhase();
	if (
		location != light->Light.Location ||
		radius != light->Light.Radius ||
		type != light->Light.Type ||
		effect != light->Light.Effect ||
		hue != light->Light.Hue ||
		saturation != light->Light.Saturation ||
		brightness != light->Light.Brightness ||
		rotation != light->Light.Rotation ||
		cone != light->Light.Cone ||
		period != light->Light.Period ||
		phase != light->Light.Phase)
	{
		light->Light.Location = location;
		light->Light.Radius = radius;
		light->Light.Type = type;
		light->Light.Effect = effect;
		light->Light.Hue = hue;
		light->Light.Saturation = saturation;
		light->Light.Brightness = brightness;
		light->Light.Rotation = rotation;
		light->Light.Cone = cone;
		light->Light.Period = period;
		light->Light.Phase = phase;
		light->Light.LastUpdate = FrameCounter;
	}
}

void LightSystem::InitVertexLight(VertexLight& out, UActor* actor, UZoneInfo* zoneActor)
{
	// AmbientGlow value 255 is a special pulsating effect used for powerups
	float ambientGlow = actor->AmbientGlow() == 255 ? AmbientGlowAmount : actor->AmbientGlow() * (1.0f / 255.0f);
	// Deus Ex's is the original's: the zone's ambient light in FGetHSV's
	// colour (dx-reverse-info/render-dll.md, meshes)
	out.AmbientColor = ambientGlow + FGetHSV(zoneActor->AmbientHue(), zoneActor->AmbientSaturation(), zoneActor->AmbientBrightness());

	out.ScaleGlow = actor->ScaleGlow();
	// DrawLodMesh's unlit level (Render.dll 0x10b0f6e0): the glow raw,
	// not pulsing, held to 0-1
	out.UnlitLevel = std::clamp(actor->AmbientGlow() / 256.0f + actor->ScaleGlow() * 0.5f, 0.0f, 1.0f);
	SetupForActorDX(out, actor);

	out.CameraLocation = engine->CameraLocation;

	int fogIndex = 0;
	for (UActor* light : FogBalls)
	{
		if (light->FogInfo.brightness < 0.0f)
		{
			light->FogInfo.fogcolor = hsbtorgb(light->LightHue(), light->LightSaturation(), light->LightBrightness());
			light->FogInfo.brightness = light->LightBrightness() * (1.0f / 255.0f) * light->VolumeBrightness() * (1.0f / 64.0f);
			light->FogInfo.fog = light->VolumeFog() * (1.0f / 255.0f);
			light->FogInfo.radius = light->WorldVolumetricRadius();
			light->FogInfo.location = light->Location();
		}

		out.FogBalls[fogIndex].fogcolor = light->FogInfo.fogcolor;
		out.FogBalls[fogIndex].brightness = light->FogInfo.brightness * 5.0f;
		out.FogBalls[fogIndex].fog = light->FogInfo.fog;
		out.FogBalls[fogIndex].radius = light->FogInfo.radius;
		out.FogBalls[fogIndex].lightpos = light->FogInfo.location;

		fogIndex++;
		if (fogIndex == VertexLight::MaxFogBalls)
			break;
	}
	out.NumFogBalls = fogIndex;
}

void LightSystem::SetupForActorDX(VertexLight& out, UActor* actor)
{
	// The original's SetupForActor picks an actor's lights once a draw
	// (dx-reverse-info/render-dll.md, lighting -- meshes): the static lights that
	// reach into the actor's leaf of the BSP, the moving lights in it, and
	// the lights it had last frame, each counted at its strength at the
	// actor's centre; the pick strongest first -- statics until 8 are
	// taken, the others while fewer than 8 in all are, none below an
	// eighth of the strongest; each picked light shadow-checked through
	// the BSP every 16 frames by the frame count and the light's index,
	// except a movable non-static one, always taken; and each fading in
	// and out over about a third of a second, lighting as it fades.
	auto& state = actor->MeshLights;
	out.NumLights = 0;

	if (actor->bUnlit())
	{
		state.List.clear();
		return;
	}

	if (state.LastFrame != FrameCounter)
	{
		state.LastFrame = FrameCounter;
		vec3 location = actor->Location();
		UModel* model = Level->Model;

		for (UActor::MeshLightEntry& entry : state.List)
			entry.Picked = false;

		// The candidates
		Array<UActor*> candidates;
		auto addCandidate = [&](UActor* light)
		{
			if (!light || light->bDeleteMe())
				return;
			if (light->LightType() == LT_None || light->LightBrightness() == 0)
				return;
			if (light->bSpecialLit() != actor->bSpecialLit())
				return;
			for (UActor* seen : candidates)
				if (seen == light)
					return;
			candidates.push_back(light);
		};

		int leaf = model->FindLeafAt(location);
		if (leaf >= 0 && (size_t)leaf < model->Leaves.size())
		{
			int index = model->Leaves[leaf].Permeating;
			if (index >= 0)
				for (size_t i = index; i < model->Lights.size() && model->Lights[i]; i++)
					addCandidate(model->Lights[i]);
		}
		vec3 extents = actor->BspInfo.BoundingBox.extents();
		LightTree.CollectLights(location, std::max(extents.x, std::max(extents.y, extents.z)));
		for (UActor* light : LightTree.CollectedLights)
		{
			if (light->bDynamicLight() || (!light->bStatic() && !light->bNoDelete()))
				addCandidate(light);
		}
		for (const UActor::MeshLightEntry& entry : state.List)
			addCandidate(entry.Light);

		// The strength at the actor's centre; a cylinder light counts with
		// three quarters of its brightness and radius
		struct Candidate
		{
			UActor* Light;
			float Strength;
			bool Static;
		};
		Array<Candidate> reaching;
		for (UActor* light : candidates)
		{
			float radius = light->WorldLightRadius();
			float brightnessScale = 1.0f;
			if (light->LightEffect() == LE_Cylinder)
			{
				radius *= 0.75f;
				brightnessScale = 0.75f;
			}
			if (radius <= 0.0f)
				continue;
			vec3 L = light->Location() - location;
			float dist = std::sqrt(dot(L, L));
			if (dist >= radius)
				continue;
			float strength = (1.0f - dist / radius) * light->LightBrightness() * brightnessScale;
			if (strength <= 0.0f)
				continue;
			reaching.push_back({ light, strength, (bool)light->bStatic() });
		}
		std::sort(reaching.begin(), reaching.end(), [](const Candidate& a, const Candidate& b) { return a.Strength > b.Strength; });

		// The pick
		Array<const Candidate*> taken;
		for (const Candidate& candidate : reaching)
		{
			if (!candidate.Static)
				continue;
			taken.push_back(&candidate);
			if (taken.size() == 8)
				break;
		}
		for (const Candidate& candidate : reaching)
		{
			if (candidate.Static || taken.size() >= 8)
				continue;
			taken.push_back(&candidate);
		}
		float strongest = 0.0f;
		for (const Candidate* candidate : taken)
			strongest = std::max(strongest, candidate->Strength);

		for (const Candidate* candidate : taken)
		{
			if (candidate->Strength < strongest * 0.125f)
				continue;

			UActor::MeshLightEntry* entry = nullptr;
			for (UActor::MeshLightEntry& kept : state.List)
			{
				if (kept.Light == candidate->Light)
				{
					entry = &kept;
					break;
				}
			}
			if (!entry)
			{
				if (state.List.size() >= 16)
					continue;
				UActor::MeshLightEntry fresh;
				fresh.Light = candidate->Light;
				state.List.push_back(fresh);
				entry = &state.List.back();
			}
			entry->Picked = true;
			entry->Strength = candidate->Strength;

			// Shadowed or not
			UActor* light = entry->Light;
			if (light->bMovable() && !light->bStatic())
			{
				entry->Shadowed = false;
			}
			else if (entry->LastShadowFrame < 0 ||
				((FrameCounter + light->Name.GetCompareIndex()) & 15) == 0)
			{
				entry->Shadowed = engine->Level->Collision.TraceAnyHit(light->Location(), location, actor, false, true, true);
				entry->LastShadowFrame = FrameCounter;
			}
		}

		// The fades
		float elapsed = clamp(LastTickElapsed, 0.0f, 0.1f);
		for (size_t i = 0; i < state.List.size();)
		{
			UActor::MeshLightEntry& entry = state.List[i];
			bool lit = entry.Picked && !entry.Shadowed;
			entry.Fade = clamp(entry.Fade + (lit ? 3.0f : -3.0f) * elapsed, 0.0f, 1.0f);
			if (!entry.Picked && entry.Fade <= 0.0f)
			{
				state.List.erase(state.List.begin() + i);
				continue;
			}
			i++;
		}
	}

	int lightIndex = 0;
	for (const UActor::MeshLightEntry& entry : state.List)
	{
		if (entry.Fade <= 0.0f || entry.Light->bDeleteMe())
			continue;
		UActor* light = entry.Light;
		out.Lights[lightIndex].Location = light->Location();
		out.Lights[lightIndex].Color = LightmapBuilder::GetLightColorDX(light) * entry.Fade;
		float invRadius = 1.0f / light->WorldLightRadius();
		out.Lights[lightIndex].InvRadius = invRadius;
		out.Lights[lightIndex].InvRadiusSquared = invRadius * invRadius;
		lightIndex++;
		if (lightIndex == VertexLight::MaxLights)
			break;
	}
	out.NumLights = lightIndex;
}
