
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

	// A mover's polygons are lit where the level's BSP filed them: the
	// original's brush tracker files them into the BSP as surfaces of its
	// own, each filing moving their bases and axes with the mover, and lights
	// them there, a static map through the shadows the editor raytraced at
	// its BrushRaytraceKey (dx-reverse-info's engine-dll.md, the movers in the
	// BSP; render-dll.md, light maps)
	vec3 scale = mover->MainScale().Scale * mover->PostScale().Scale;
	mat4 brushToWorld = mover->BrushToWorld();
	mat4 objectToWorld = brushToWorld * localCoords.ToMatrix();
	Coords worldCoords = Coords::FromMatrix(objectToWorld);
	// The normal as the filing turns it, over PostScale where the matrix
	// takes it times PostScale (the stock brushes' MainScale is 1 or -1)
	vec3 postScale = mover->PostScale().Scale;
	worldCoords.ZAxis = worldCoords.ZAxis / (postScale * postScale);
	scale = vec3(std::abs(scale.x), std::abs(scale.y), std::abs(scale.z));

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
		mover, mover->bSpecialLit());
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
// differs. Only a moving light is ever taken from it -- bDynamicLight, or
// neither static nor no-delete; any other lights a map through the
// surface's own list -- so the others are left out here once, not passed
// over every frame.
const Array<UActor*>& LightSystem::CollectSurfaceLights(SurfaceLights& entry, const vec3& center, float radius)
{
	if (entry.Version != LightTreeVersion || entry.Center != center || entry.Radius != radius)
	{
		LightTree.CollectLights(center, radius);
		entry.Lights.clear();
		for (UActor* light : LightTree.CollectedLights)
		{
			if (light->bDynamicLight() || (!light->bStatic() && !light->bNoDelete()))
				entry.Lights.push_back(light);
		}
		entry.Version = LightTreeVersion;
		entry.Center = center;
		entry.Radius = radius;
	}
	return entry.Lights;
}

TextureInfo LightSystem::GetLightmap(UModel* model, int lightmapIndex, const Coords& coords, UZoneInfo* zoneActor, const vec3& worldLocation, float radius, UMover* mover, bool specialLit)
{
	if (lightmapIndex < 0)
		return {};

	// Asked for, counted and timed for STAT CACHE's Gets, its build for its
	// Crts, as the original's cache counts and clocks each get and create
	// (FMemCache::Status); the times while it is shown
	auto& panel = engine->render->PanelStats;
	panel.CacheGets++;
	StatClock cacheClock(panel.CacheGetTime, engine->render->StatPanels[RenderSubsystem::StatCache]);

	LightMapIndex& lmindex = model->LightMap[lightmapIndex];

	uint32_t ambientID = (((uint32_t)zoneActor->AmbientHue()) << 16) | (((uint32_t)zoneActor->AmbientSaturation()) << 8) | (uint32_t)zoneActor->AmbientBrightness();
	uint64_t cacheID = (((uint64_t)lmindex.LMCacheID) << 32) | (((uint64_t)ambientID) << 8) | 1;

	UMover* dynamicMover = mover && mover->bDynamicLightMover() ? mover : nullptr;
	// Any other mover's surface is lit as a level surface's: the original's
	// SetupNormalSurface drops the surface's actor (Render.dll 0x10b070c1)
	// before its cached static map is tested, and only a bDynamicLightMover's
	// is tested against where the mover is (0x10b07e29-0x10b07ec5). So its
	// static map is built where the mover is when none is kept -- its first
	// draw, after a flush (Flush), when a light of it changed -- and kept as
	// the mover moves and turns; so is the shape of each of its animating
	// lights whose shape holds still, kept as its static map's place gave it
	// and only scaled by its brightness (0x10b0873d-0x10b0888a), where an
	// animating light whose shape changes, and each moving light, is run
	// where the mover is now (render-dll.md, light maps)
	UMover* keptMover = mover && !dynamicMover ? mover : nullptr;
	if (mover)
	{
		// A mover's maps are its own: the mover's ID in the upper 12 bits
		// (4096 movers), which leaves 20 bits (1 million) for the lightmaps.
		// And a map of each zone it is drawn in, by the zone actor's own zone
		// number, as the original's cache holds one under the model, the
		// light map and that number (the zone actor's at 0x10b06e90), where a
		// level surface's by its ambient light alone gives the same picture:
		// a door drawn in a zone other than the one its map was built in, of
		// the same ambient light, has a map built where it then stands.
		if (mover->Light.MoverID == 0)
			mover->Light.MoverID = NextMoverID++;
		cacheID |= ((uint64_t)mover->Light.MoverID) << 52;
		cacheID |= ((uint64_t)(zoneActor->Region().ZoneNumber & 63)) << 2;

		// When the mover last moved or turned: a bDynamicLightMover's maps
		// are built again then, as the original's are (0x10b07e29), and the
		// moving lights over any other's are run again where it is now
		if (!mover->Light.HasLastTransform ||
			mover->Light.LastLocation != mover->DrawnLocation() ||
			mover->Light.LastRotation != mover->DrawnRotation())
		{
			mover->Light.HasLastTransform = true;
			mover->Light.LastLocation = mover->DrawnLocation();
			mover->Light.LastRotation = mover->DrawnRotation();
			mover->Light.MovedFrame = FrameCounter;
		}
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
		// its shadow bits; a bDynamicLight one, before all else, is a
		// moving one, with no shadow (dx-reverse-info/render-dll.md, light
		// maps). With NoDynamicLights, animated lights count as static and
		// moving ones are left out. An LE_OmniBumpMap one, given no owner,
		// is a kind of its own, before all that, which no light map takes
		// (Render.dll AddLight 0x10b08b7a-0x10b08b85; SetupForSurf
		// 0x10b06c90 merges the static, animated and moving ones): it
		// lights no surface. A still one's changes are the static map's,
		// so that a light made one, or one no more, builds it again. On a
		// kept mover map a light that is not bStatic animates too, as the
		// original's AddLight counts a light static only with bStatic
		// (0x10b08b30): a TriggerLight's shape is kept where the map was
		// built and scaled by its brightness as it fades, the static map
		// left as built (a door's map, opened and its room's light switched);
		// on the level's surfaces either gives the same picture.

		if (lmindex.LightActors >= 0)
		{
			UActor** lightlist = &model->Lights[lmindex.LightActors];
			for (int lightindex = 0; lightlist[lightindex] != nullptr; lightindex++)
			{
				UActor* light = lightlist[lightindex];
				CheckLight(light);
				bool animates = LightmapBuilder::LightAnimates(light) || (keptMover && !light->bStatic());
				if (light->LightEffect() == LE_OmniBumpMap)
				{
					if (!light->bDynamicLight() && (noDynamicLights || !animates))
						lastStaticUpdate = std::max(lastStaticUpdate, light->Light.LastUpdate);
				}
				else if (light->bDynamicLight())
				{
					if (!noDynamicLights && light->bSpecialLit() == specialLit)
					{
						lastDynamicUpdate = std::max(lastDynamicUpdate, light->Light.LastUpdate);
						TempDynLightList.push_back(light);
					}
				}
				else if (!noDynamicLights && animates)
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

		// The moving lights over the surface go into the dynamic light list,
		// but an LE_OmniBumpMap one, given no owner as the surface's own:

		if (!noDynamicLights)
		{
			for (UActor* light : CollectSurfaceLights(surface, worldLocation, radius))
			{
				if (light->Light.LightmapCheckCounter != checkCounter)
				{
					light->Light.LightmapCheckCounter = checkCounter;
					if (light->bSpecialLit() == specialLit && light->LightEffect() != LE_OmniBumpMap)
					{
						CheckLight(light);
						lastDynamicUpdate = std::max(lastDynamicUpdate, light->Light.LastUpdate);
						TempDynLightList.push_back(light);
					}
				}
			}
		}
		if (keptMover && !TempDynLightList.empty())
			lastDynamicUpdate = std::max(lastDynamicUpdate, keptMover->Light.MovedFrame);
	}
	else
	{
		// A dynamic mover's maps are rebuilt when the mover moved or turned
		// since, or one of its lights changed. The original gives the moving
		// lights over its surfaces the mover for their owner, so that each is
		// a moving light, an LE_OmniBumpMap one too (AddLight 0x10b08b72),
		// and the lights of its leaf none, as a level surface's own: an
		// LE_OmniBumpMap light lights it only as a moving light.
		lastDynamicUpdate = dynamicMover->Light.MovedFrame;

		for (UActor* light : dynamicMover->TouchingLights.List)
		{
			if (light->LightEffect() == LE_OmniBumpMap && !light->bDynamicLight() && (light->bStatic() || light->bNoDelete()))
				continue;
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
	if (!lmtexture || lmtexture->LastStaticUpdate != lastStaticUpdate || lmtexture->LastDynamicUpdate != lastDynamicUpdate || lmtexture->MovingLightSet != movingSet ||
		(keptMover && lmtexture->StaticFlush != FlushCount))
	{
		engine->render->Stats.LightmapsUpdated++;
		panel.CacheCreates++;
		cacheClock.Next(panel.CacheCreateTime);

		Builder.Setup(model, coords, lightmapIndex);
		if (!keptMover)
			Builder.FindAddedLights(model, lightmapIndex, TempAnimatedIndexList, TempDynLightList);

		if (!lmtexture || lmtexture->Mip.Width != Builder.Width() || lmtexture->Mip.Height != Builder.Height())
		{
			lmtexture = std::make_unique<LightmapTexture>();
			lmtexture->Format = TextureFormat::RGBA32_F;
			lmtexture->Mip.Width = Builder.Width();
			lmtexture->Mip.Height = Builder.Height();
			lmtexture->Mip.Data.resize((size_t)lmtexture->Mip.Width * lmtexture->Mip.Height * sizeof(vec4));
		}

		if (keptMover)
		{
			BuildKeptMoverMap(model, lightmapIndex, coords, zoneActor, *lmtexture, lastStaticUpdate);
			updated = { 0, 0, Builder.Width(), Builder.Height() };
		}
		else
		{
			// When only the animating and moving lights changed, the map is
			// built again only where they reach now or reached when it was
			// last built -- the kept static map, the lights over it -- and only
			// that part is converted and uploaded. A mover's map is built
			// whole.
			bool partial = !mover && lmtexture->LastStaticUpdate == lastStaticUpdate && lmtexture->StaticLightColors.size() == (size_t)Builder.Width() * Builder.Height();
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
		}

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
		cacheClock.Next(panel.CacheGetTime);
	}
	lmtexture->AskedFrame = FrameCounter;

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

// A kept mover map built (GetLightmap's keptMover): the static map where the
// mover stands when there is none -- its first draw, after a flush, or once
// one of its static lights has changed -- else the one kept; each animating
// light whose shape holds still where its shape was first worked out, as the
// original keeps that shape by the light and the surface, made once and only
// scaled by its brightness (Render.dll 0x10b0873d-0x10b0888a, a flush
// dropping it with the maps); each animating light whose shape changes, and
// each moving light, where the mover is now.
void LightSystem::BuildKeptMoverMap(UModel* model, int lightmapIndex, const Coords& coords, UZoneInfo* zoneActor, LightmapTexture& map, int lastStaticUpdate)
{
	const LightMapIndex& lmindex = model->LightMap[lightmapIndex];
	LightmapRect whole = { 0, 0, Builder.Width(), Builder.Height() };
	if (map.StaticFlush != FlushCount)
		map.ShapePlaces.clear();
	bool keptStatic = map.StaticFlush == FlushCount && map.LastStaticUpdate == lastStaticUpdate &&
		map.StaticLightColors.size() == (size_t)Builder.Width() * Builder.Height();

	Builder.CalcWorldLocations(whole);
	if (keptStatic)
	{
		Builder.LoadStaticLight(map.StaticLightColors, whole);
	}
	else
	{
		Builder.SetAmbientLight(zoneActor);
		Builder.AddStaticLights(model, lightmapIndex, true);
		Builder.SaveStaticLight(map.StaticLightColors);
		map.StaticCoords = coords;
		map.StaticFlush = FlushCount;
	}

	// The still shapes: a light lit for the first time is given the place
	// the mover stands at now; one off adds nothing and is given none
	TempChangingIndexList.clear();
	TempPlaces.clear();
	for (int lightindex : TempAnimatedIndexList)
	{
		UActor* light = model->Lights[lmindex.LightActors + lightindex];
		if (LightmapBuilder::ShapeChanges(light))
		{
			TempChangingIndexList.push_back(lightindex);
			continue;
		}
		if (light->LightType() == LT_None || light->LightBrightness() == 0)
			continue;
		auto it = std::find_if(map.ShapePlaces.begin(), map.ShapePlaces.end(), [&](const LightmapTexture::ShapePlace& sp) { return sp.LightIndex == lightindex; });
		if (it == map.ShapePlaces.end())
		{
			map.ShapePlaces.push_back({ lightindex, coords });
			it = map.ShapePlaces.end() - 1;
		}
		auto group = std::find_if(TempPlaces.begin(), TempPlaces.end(), [&](const PlaceGroup& g) { return g.Place == it->Place; });
		if (group == TempPlaces.end())
		{
			TempPlaces.push_back({ it->Place, {} });
			group = TempPlaces.end() - 1;
		}
		group->Lights.push_back(lightindex);
	}
	for (const PlaceGroup& group : TempPlaces)
	{
		Builder.Setup(model, group.Place, lightmapIndex);
		Builder.FindAddedLights(model, lightmapIndex, group.Lights, TempNoLights);
		Builder.CalcWorldLocations(whole);
		Builder.AddLights(model, lightmapIndex);
	}

	Builder.Setup(model, coords, lightmapIndex);
	Builder.FindAddedLights(model, lightmapIndex, TempChangingIndexList, TempDynLightList);
	Builder.CalcWorldLocations(whole);
	Builder.AddLights(model, lightmapIndex);
}

void LightSystem::CountMaps(int& items, size_t& fresh, size_t& stale) const
{
	items = 0;
	fresh = 0;
	stale = 0;
	auto count = [&](const LightmapTexture* texture)
	{
		if (!texture)
			return;
		items++;
		size_t bytes = texture->Mip.Data.size() + texture->StaticLightColors.size() * sizeof(vec3);
		if (texture->AskedFrame == FrameCounter)
			fresh += bytes;
		else
			stale += bytes;
	};
	for (const auto& it : lmtextures)
		count(it.second.get());
	for (const auto& it : fogtextures)
		count(it.second.Texture.get());
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
	// as the original's dynamic map, built every frame, has it; a pawn's
	// aim its view rotation (LightEffect::SpotRotation)
	Rotator rotation = LightEffect::SpotRotation(light);
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

void LightSystem::InitVertexLight(VertexLight& out, UActor* drawn, UActor* actor, UZoneInfo* zoneActor, bool leafLights)
{
	// AmbientGlow value 255 is a special pulsating effect used for powerups
	float ambientGlow = actor->AmbientGlow() == 255 ? AmbientGlowAmount : actor->AmbientGlow() * (1.0f / 255.0f);
	// Deus Ex's is the original's: the zone's ambient light in FGetHSV's
	// colour (dx-reverse-info/render-dll.md, meshes)
	out.AmbientColor = ambientGlow + FGetHSV(zoneActor->AmbientHue(), zoneActor->AmbientSaturation(), zoneActor->AmbientBrightness());

	out.ScaleGlow = actor->ScaleGlow();
	// DrawLodMesh's unlit level (Render.dll 0x10b0f6e0): the glow raw,
	// not pulsing, held to 0-1, of the mesh drawn, a held item's its own
	// though the pawn lights the rest of it (DrawActorSprite 0x10b25080)
	out.UnlitLevel = std::clamp(drawn->AmbientGlow() / 256.0f + drawn->ScaleGlow() * 0.5f, 0.0f, 1.0f);
	SetupForActorDX(out, actor, leafLights);

	out.CameraLocation = Eye;
}

void LightSystem::SetupForActorDX(VertexLight& out, UActor* actor, bool leafLights)
{
	// The original's SetupForActor picks an actor's lights once a draw
	// (Render.dll 0x10b08c70; dx-reverse-info/render-dll.md, lighting --
	// meshes): the static lights that reach into the actor's leaf of the
	// BSP, the moving lights in it, and the lights it had last frame that
	// still reach it, each counted at its strength at the actor's centre;
	// then one pass over them, strongest first, and each light's fade, a
	// byte kept under the actor, lighting as it fades. The moving lights
	// are those the scene's walk filed in the leaf: an actor drawn on its
	// own (Canvas.DrawActor, GC.DrawActor: URender::DrawActor, 0x10b262c0)
	// has none, so a first-person weapon is lit by the static lights alone.
	// The actor's own light is a candidate as any other and counts in the
	// pass, but lights nothing of it (AddLight, 0x10b08b30).
	auto& state = actor->MeshLights;
	out.NumLights = 0;

	if (actor->bUnlit())
	{
		state.List.clear();
		return;
	}

	// A frame not lit -- the viewer's RendMap off REN_DynLight, the client's
	// NoLighting on (RenderSubsystem::SceneLit) -- picks no light and steps
	// no fade, its mesh drawn unlit, the lights kept under it as they were
	// (Render.dll SetupForActor 0x10b08e78-0x10b08e98)
	if (!engine->render->SceneLit)
		return;

	if (state.LastFrame != FrameCounter)
	{
		// The actor's first draw, which finds no lights kept under it
		bool fresh = state.LastFrame == -1;
		state.LastFrame = FrameCounter;
		vec3 location = actor->Location();
		UModel* model = Level->Model;

		// The candidates, each counted once, by the first list it is in, with
		// that list's fade: the lights kept under the actor at their own; the
		// static lights reaching into its leaf at 0; the moving lights in the
		// leaf at 255, new to it (SetupForActor 0x10b08c70). A light off or
		// of no brightness stays a candidate, the moving ones none such.
		struct Candidate
		{
			UActor* Light;
			int Fade;
			float Strength;
			bool Static;
		};
		Array<Candidate> candidates;
		auto addCandidate = [&](UActor* light, int fade)
		{
			if (!light || light->bDeleteMe())
				return;
			if (light->bSpecialLit() != actor->bSpecialLit())
				return;
			for (const Candidate& seen : candidates)
				if (seen.Light == light)
					return;
			candidates.push_back({ light, fade, 0.0f, (bool)light->bStatic() });
		};

		for (const UActor::MeshLightEntry& entry : state.List)
			addCandidate(entry.Light, entry.Fade);
		int leaf = model->FindLeafAt(location);
		if (leaf >= 0 && (size_t)leaf < model->Leaves.size())
		{
			int index = model->Leaves[leaf].Permeating;
			if (index >= 0)
				for (size_t i = index; i < model->Lights.size() && model->Lights[i]; i++)
					addCandidate(model->Lights[i], 0);
		}
		if (leafLights)
		{
			vec3 extents = actor->BspInfo.BoundingBox.extents();
			LightTree.CollectLights(location, std::max(extents.x, std::max(extents.y, extents.z)));
			size_t kept = candidates.size();
			for (UActor* light : LightTree.CollectedLights)
			{
				if (light->bDynamicLight() || (!light->bStatic() && !light->bNoDelete()))
					addCandidate(light, 255);
			}
			// STAT MESH's MeshVtrics: the moving lights new to the actor, as
			// the original's SetupForActor counts each it adds (0x10b097a0)
			engine->render->PanelStats.MeshVtrics += (int)(candidates.size() - kept);
		}

		// Each that reaches the actor's centre, at its strength there. A
		// cylinder light counts with three quarters of its brightness and
		// radius, its reach at the whole radius, so that past three
		// quarters of it its strength is below 0 and it fades out.
		Array<Candidate> reaching;
		for (Candidate& candidate : candidates)
		{
			UActor* light = candidate.Light;
			float reach = light->WorldLightRadius();
			if (reach <= 0.0f)
				continue;
			vec3 L = light->Location() - location;
			float dist = std::sqrt(dot(L, L));
			if (dist >= reach)
				continue;
			float radius = reach;
			float brightnessScale = 1.0f;
			if (light->LightEffect() == LE_Cylinder)
			{
				radius *= 0.75f;
				brightnessScale = 0.75f;
			}
			candidate.Strength = (1.0f - dist / radius) * light->LightBrightness() * brightnessScale;
			reaching.push_back(candidate);
		}
		std::sort(reaching.begin(), reaching.end(), [](const Candidate& a, const Candidate& b) { return a.Strength > b.Strength; });

		// The pass: a static light is lit while fewer than 8 static ones
		// are, any other while fewer than 8 in all are, none under an eighth
		// of the first lit one's strength (its strength in 1024ths). A
		// movable light that is not static is lit; another as its shadow
		// line through the BSP found it, run again every 16 frames by the
		// frame count and the light's index, and for every light on the
		// actor's first draw, when one found open is full at once; between,
		// as its fade's lowest bit has it, so that a moving light new to the
		// actor is lit full until its line is run, a static one dark. Each
		// fade steps by the viewport's time since its last frame x 768, at
		// most 255 (a third of a second): lit, up and odd, to 255; else down
		// and even, to 0. A light off or of no brightness takes its place in
		// the pass, but lights nothing (below).
		// Up to 16 lights with a fade over 1 are kept for the next draw,
		// strongest first; one that no longer reaches the actor is dropped
		// at once.
		int step = std::clamp((int)(LastFrameElapsed * 768.0f), 0, 255);
		int threshold = -1, litStatics = 0, litAll = 0;
		Array<UActor::MeshLightEntry> kept;
		for (const Candidate& candidate : reaching)
		{
			UActor* light = candidate.Light;
			bool moving = light->bMovable() && !light->bStatic();
			UActor::MeshLightEntry entry;
			entry.Light = light;
			entry.Fade = candidate.Fade;

			int strength = (int)(candidate.Strength * 1024.0f);
			bool lit = false;
			if (strength >= threshold && (candidate.Static ? litStatics : litAll) < 8)
			{
				if (moving)
				{
					lit = true;
				}
				else if (fresh || ((FrameCounter + light->Name.GetCompareIndex()) & 15) == 0)
				{
					lit = !engine->Level->Collision.TraceAnyHit(light->Location(), location, actor, false, true, true);
					if (lit && fresh)
						entry.Fade = 255;
				}
				else
				{
					lit = (entry.Fade & 1) != 0;
				}
			}

			if (lit)
			{
				litAll++;
				if (candidate.Static)
					litStatics++;
				if (threshold < 0)
					threshold = strength / 8;
				entry.Fade = std::min(entry.Fade + step, 255) | 1;
			}
			else
			{
				entry.Fade = std::max(entry.Fade - step, 0) & ~1;
			}
			if (entry.Fade > 1 && kept.size() < 16)
				kept.push_back(entry);
		}
		state.List = std::move(kept);
	}

	// Each light as it fades, but the actor's own and one off or of no
	// brightness, which AddLight takes not (0x10b08b30)
	int lightIndex = 0;
	for (const UActor::MeshLightEntry& entry : state.List)
	{
		if (entry.Fade <= 0 || entry.Light->bDeleteMe() || entry.Light == actor)
			continue;
		UActor* light = entry.Light;
		if (light->LightType() == LT_None || light->LightBrightness() == 0)
			continue;
		out.Lights[lightIndex].Location = light->Location();
		out.Lights[lightIndex].Color = LightmapBuilder::GetLightColorDX(light) * (entry.Fade / 255.0f);
		float invRadius = 1.0f / light->WorldLightRadius();
		out.Lights[lightIndex].InvRadius = invRadius;
		out.Lights[lightIndex].InvRadiusSquared = invRadius * invRadius;
		lightIndex++;
		if (lightIndex == VertexLight::MaxLights)
			break;
	}
	out.NumLights = lightIndex;
}
