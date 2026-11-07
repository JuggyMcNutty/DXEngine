
#include "Precomp.h"
#include "VisibleNode.h"
#include "VisibleDecal.h"
#include "RenderSubsystem.h"
#include "Engine.h"
#include "Packages/Engine/Subsystems/UDXEngineRenderDevice.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"

void VisibleNode::Draw(VisibleFrame* frame)
{
	UModel* model = engine->Level->Model;
	BspSurface& surface = model->Surfaces[Node->Surf];

	const vec3& UVec = model->Vectors[surface.vTextureU];
	const vec3& VVec = model->Vectors[surface.vTextureV];
	const vec3& Base = model->Points[surface.pBase];

	engine->render->UpdateTexture(surface.Material);

	// The zone the surface is seen from, whose ambient lights its light map
	// and whose pan speeds pan it, as the original's draw list has it
	// (dx-reverse-info render-dll.md, light maps); Surreal Engine took the far
	// side's, zone 0 (the LevelInfo) behind a wall, which lit every surface
	// of a zoned level with the LevelInfo's ambient (LightConsole on
	// 06_HongKong_WanChai_Underworld's floor).
	auto zoneActor = engine->GetZoneActor(ViewerBehind ? Node->Zone0 : Node->Zone1);

	// If no ZoneInfo is found, use the values from LevelInfo instead.
	float ZoneUPanSpeed = zoneActor->TexUPanSpeed();
	float ZoneVPanSpeed = zoneActor->TexVPanSpeed();

	UTexture* tex;
	if (surface.Material)
		tex = surface.Material->GetAnimTexture();
	else
		tex = engine->LevelInfo->DefaultTexture();
	// The viewer's Sprite, the Matrix cheat's, in its place, its own detail
	// and macro textures with it
	UTexture* sprite = engine->render->ViewerSprite();
	if (sprite)
	{
		engine->render->UpdateTexture(sprite);
		tex = sprite->GetAnimTexture();
	}

	TextureInfo texture;
	engine->render->UpdateTexture(tex);
	engine->render->UpdateTextureInfo(texture, surface, tex, ZoneUPanSpeed, ZoneVPanSpeed);

	// Deus Ex's detail and macro textures are those of the frame drawn, its
	// texture's current frame or the Sprite's, each locked as it is, not at
	// a current frame of its own, and stepped by its lock, as the original's
	// DrawFrame takes them (Render.dll 0x10b1a8df-0x10b1a965; Engine.dll
	// UTexture::Lock updates the texture it locks, 0x103ececd). They lie as
	// the lock leaves them, at their own scale and not panned with the
	// surface (dx-reverse-info render-dll.md, a frame). The detail texture
	// only for a surface that is no portal, while the frame draws them
	// (DetailTexturesDrawn) (render-dll.md, a frame; d3ddrv-dll.md, detail
	// textures).
	TextureInfo detailtex;
	UTexture* detailTexture = tex && engine->render->DetailTexturesDrawn && (PolyFlags & PF_Portal) == 0 ? tex->DetailTexture() : nullptr;
	if (detailTexture)
	{
		engine->render->UpdateTexture(detailTexture);
		engine->render->UpdateTextureInfo(detailtex, detailTexture);
	}

	TextureInfo macrotex;
	UTexture* macroTexture = tex ? tex->MacroTexture() : nullptr;
	if (macroTexture)
	{
		engine->render->UpdateTexture(macroTexture);
		engine->render->UpdateTextureInfo(macrotex, macroTexture);
	}

	int numverts = Node->NumVertices;
	vec3* points = engine->render->GetTempVertexBuffer(numverts);

	BspVert* v = &model->Vertices[Node->VertPool];
	if (frame->MirrorFlag)
	{
		for (int j = 0; j < numverts; j++)
		{
			points[numverts - 1 - j] = model->Points[v[j].Vertex];
		}
	}
	else
	{
		for (int j = 0; j < numverts; j++)
		{
			points[j] = model->Points[v[j].Vertex];
		}
	}

	// Neither light nor fog map in a frame not lit (RenderSubsystem::
	// SceneLit), as the original's DrawFrame asks none of them then
	TextureInfo lightmap;
	TextureInfo fogmap;
	if ((PolyFlags & PF_Unlit) == 0 && engine->render->SceneLit)
	{
		lightmap = engine->Level->Light.GetLevelLightmap(surface, zoneActor, model);
		// Deus Ex's fog map, as the original's renderer makes it
		// (dx-reverse-info render-dll.md, light maps): only with the render
		// device's VolumetricLighting on and the viewport's actor in a fog
		// zone, for a surface not translucent, from the zone it is seen
		// from, which a fog zone too; Surreal Engine took the camera's zone for
		// every surface, fogging one seen from a zone without fog.
		if (frame->VolumetricFog && (PolyFlags & PF_Translucent) == 0)
			fogmap = engine->Level->Light.GetLevelFogmap(surface, zoneActor, model);
	}

	SurfaceInfo surfaceinfo;
	surfaceinfo.PolyFlags = PolyFlags;
	surfaceinfo.Texture = &texture;
	surfaceinfo.MacroTexture = macroTexture ? &macrotex : nullptr;
	surfaceinfo.DetailTexture = detailTexture ? &detailtex : nullptr;
	surfaceinfo.LightMap = lightmap.NumMips != 0 ? &lightmap : nullptr;
	surfaceinfo.FogMap = fogmap.NumMips != 0 ? &fogmap : nullptr;

	SurfaceFacet facet;
	facet.MapCoords.Origin = Base;
	facet.MapCoords.XAxis = UVec;
	facet.MapCoords.YAxis = VVec;
	facet.Vertices = points;
	facet.VertexCount = numverts;

	frame->Device->DrawComplexSurface(&frame->Frame, surfaceinfo, facet);
	engine->render->Stats.Surfaces++;

	VisibleDecal decal;
	decal.DrawDecals(frame, Node);
}
