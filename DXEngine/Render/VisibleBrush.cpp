
#include "Precomp.h"
#include "VisibleBrush.h"
#include "VisibleFrame.h"
#include "RenderSubsystem.h"
#include "RenderDevice/RenderDevice.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include "Packages/Engine/Subsystems/UDXEngineRenderDevice.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Collision/TopLevel/CollisionSystem.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"

mat4 VisibleBrush::MoverToWorld(UMover* mover)
{
	return mat4::translate(mover->Location()) * Coords::Rotation(mover->Rotation()).ToMatrix() * mat4::scale(mover->MainScale().Scale) * mat4::translate(-mover->PrePivot());
}

bool VisibleBrush::Draw(VisibleFrame* frame, UActor* actor, bool translucentPass)
{
	UMover* mover = UActor::TryCast<UMover>(actor);
	if (!mover)
		return false;

	engine->render->Stats.Actors++;

	UModel* brush = mover->Brush();
	SceneNode brushframe = frame->Frame;

	engine->Level->Light.UpdateLightList(mover);

	ToWorld = MoverToWorld(mover);
	brushframe.ObjectToWorld = ToWorld;

	frame->Device->SetSceneNode(&brushframe);
	bool needTranslucentPass = false;

	// Each polygon once: a translucent or modulated one blends again each
	// time it is drawn
	for (const Poly& poly : brush->Polys->Polys)
	{
		if (DrawBrushPoly(frame, brush, poly, mover, translucentPass))
			needTranslucentPass = true;
	}

	frame->Device->SetSceneNode(&frame->Frame);
	return needTranslucentPass;
}

// A mover's mirror sealed after its frame (VisibleFrame::DrawPortals): its
// polygon's depth alone, at the mover's step toward the viewer, so what lies
// behind it stays behind the reflection
void VisibleBrush::DrawMirrorSeal(VisibleFrame* frame, UMover* mover, const Poly& poly)
{
	SceneNode brushframe = frame->Frame;
	ToWorld = MoverToWorld(mover);
	brushframe.ObjectToWorld = ToWorld;
	frame->Device->SetSceneNode(&brushframe);
	DrawBrushPoly(frame, mover->Brush(), poly, mover, false, true);
	frame->Device->SetSceneNode(&frame->Frame);
}

bool VisibleBrush::DrawBrushPoly(VisibleFrame* frame, UModel* model, const Poly& poly, UMover* mover, bool translucentPass, bool seal)
{
	uint32_t PolyFlags = CollisionSystem::FiledFlags(mover, poly);

	if (seal)
	{
		PolyFlags |= PF_Occlude | PF_Invisible;
	}
	else
	{
		if (!engine->getEditorMode() && PolyFlags & PolyFlags::PF_Invisible)
			return false;

		// One-sided and seen from behind, not drawn: the original files a
		// mover's polygons into the level's BSP, whose walk passes over such
		// a surface, as VisibleFrame::ProcessNodeSurface does
		if (!(PolyFlags & PF_TwoSided) && !poly.Vertices.empty())
		{
			vec3 normal = (ToWorld * vec4(poly.Normal, 0.0f)).xyz();
			vec3 point = (ToWorld * vec4(poly.Vertices[0], 1.0f)).xyz();
			if (dot(frame->ViewLocation.xyz() - point, normal) < 0.0f)
				return false;
		}

		// A mirror that reflects this frame is drawn as its seal, and a
		// translucent one over its reflection; a translucent one with the
		// render device's ShinySurfaces off reflects nothing and is drawn
		// solid, as a BSP mirror (VisibleFrame::ProcessNodeSurface)
		if (PolyFlags & PF_Mirrored)
		{
			if ((PolyFlags & PF_Translucent) && !engine->renderdev->ShinySurfaces)
				PolyFlags &= ~PF_Translucent;
			else if (!(PolyFlags & PF_Translucent) && frame->HasMoverMirror(mover, &poly))
				return false;
		}

		bool isTranslucent = (PolyFlags & (PF_Translucent | PF_Modulated | PF_Highlighted)) != 0;
		if (isTranslucent && !translucentPass)
			return true;
		else if (!isTranslucent && translucentPass)
			return false;
	}

	//UpdateTexture(poly.Texture);

	auto zoneActor = mover->Region().Zone;
	if (!zoneActor)
		zoneActor = engine->LevelInfo;
	float ZoneUPanSpeed = zoneActor->TexUPanSpeed();
	float ZoneVPanSpeed = zoneActor->TexVPanSpeed();

	SurfaceInfo surfaceinfo;
	UTexture* polyTex = poly.Texture;
	if (!polyTex)
		polyTex = engine->LevelInfo->DefaultTexture();

	TextureInfo texture;
	TextureInfo detailtex;
	TextureInfo macrotex;

	if (polyTex)
	{
		UTexture* tex = polyTex->GetAnimTexture();
		engine->render->UpdateTexture(tex);
		engine->render->UpdateTextureInfo(texture, poly, tex, ZoneUPanSpeed, ZoneVPanSpeed);
		surfaceinfo.Texture = &texture;

		// Deus Ex's detail and macro textures not panned with the
		// polygon, the detail texture the current frame's, as VisibleNode's
		UTexture* detailTexture = engine->render->DetailTexturesDrawn && (PolyFlags & PF_Portal) == 0 ? tex->DetailTexture() : nullptr;
		if (detailTexture)
		{
			UTexture* tex = detailTexture->GetAnimTexture();
			engine->render->UpdateTexture(tex);
			engine->render->UpdateTextureInfo(detailtex, tex);
			surfaceinfo.DetailTexture = &detailtex;
		}

		if (polyTex->MacroTexture())
		{
			UTexture* tex = poly.Texture->MacroTexture()->GetAnimTexture();
			engine->render->UpdateTexture(tex);
			engine->render->UpdateTextureInfo(macrotex, tex);
			surfaceinfo.MacroTexture = &macrotex;
		}
	}

	int numverts = (int)poly.Vertices.size();
	const vec3* points = poly.Vertices.data();

	SurfaceFacet facet;
	facet.MapCoords.Origin = poly.Base;
	facet.MapCoords.XAxis = poly.TextureU;
	facet.MapCoords.YAxis = poly.TextureV;
	facet.Vertices = const_cast<vec3*>(points);
	facet.VertexCount = numverts;

	TextureInfo lightmap;
	TextureInfo fogmap;
	if ((PolyFlags & PF_Unlit) == 0 && !seal)
	{
		lightmap = engine->Level->Light.GetMoverLightmap(mover, poly, zoneActor, model);
		// Deus Ex's only as VisibleNode's makes one
		if (frame->VolumetricFog && (PolyFlags & PF_Translucent) == 0)
			fogmap = engine->Level->Light.GetMoverFogmap(mover, poly, zoneActor, model);
	}

	surfaceinfo.PolyFlags = PolyFlags;
	surfaceinfo.DepthBias = true;
	surfaceinfo.LightMap = lightmap.NumMips != 0 ? &lightmap : nullptr;
	surfaceinfo.FogMap = fogmap.NumMips != 0 ? &fogmap : nullptr;

	frame->Device->DrawComplexSurface(&frame->Frame, surfaceinfo, facet);
	return false;
}
