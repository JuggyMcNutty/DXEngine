
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
	UTexture* drawn = tex;

	TextureInfo texture;
	engine->render->UpdateTexture(tex);
	engine->render->UpdateTextureInfo(texture, surface, tex, ZoneUPanSpeed, ZoneVPanSpeed);

	// Deus Ex's detail and macro textures lie as the original's lock leaves
	// them, at their own scale and not panned with the surface
	// (dx-reverse-info render-dll.md, a frame). The detail texture is the
	// texture's current frame's, for a surface that is no portal, while the
	// frame draws them (DetailTexturesDrawn), as Render.dll hands it over
	// (render-dll.md, a frame; d3ddrv-dll.md, detail textures).
	TextureInfo detailtex;
	UTexture* detailTexture = engine->render->DetailTexturesDrawn && (surface.Material || sprite) && (PolyFlags & PF_Portal) == 0 ? tex->DetailTexture() : nullptr;
	bool detail = detailTexture != nullptr;
	if (detail)
	{
		tex = detailTexture->GetAnimTexture();
		engine->render->UpdateTexture(tex);
		engine->render->UpdateTextureInfo(detailtex, tex);
	}

	TextureInfo macrotex;
	UTexture* macroOwner = sprite ? drawn : surface.Material;
	if (macroOwner && macroOwner->MacroTexture())
	{
		tex = macroOwner->MacroTexture()->GetAnimTexture();
		engine->render->UpdateTexture(tex);
		engine->render->UpdateTextureInfo(macrotex, tex);
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

	TextureInfo lightmap;
	TextureInfo fogmap;
	if ((PolyFlags & PF_Unlit) == 0)
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
	surfaceinfo.MacroTexture = surface.Material && surface.Material->MacroTexture() ? &macrotex : nullptr;
	surfaceinfo.DetailTexture = detail ? &detailtex : nullptr;
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
