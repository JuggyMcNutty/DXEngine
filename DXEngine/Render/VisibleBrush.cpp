
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
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Collision/TopLevel/CollisionSystem.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"

mat4 VisibleBrush::MoverToWorld(UMover* mover)
{
	return mover->BrushToWorld();
}

bool VisibleBrush::Draw(VisibleFrame* frame, UActor* actor, bool translucentPass)
{
	// A mover whose brush has no polygons (a hand-made map's) is passed over,
	// as the mirror and static passes pass it over
	UMover* mover = UActor::TryCast<UMover>(actor);
	if (!mover || !mover->Brush() || !mover->Brush()->Polys)
		return false;

	engine->render->Stats.Actors++;

	UModel* brush = mover->Brush();
	SceneNode brushframe = frame->Frame;

	engine->Level->Light.UpdateLightList(mover);

	ToWorld = MoverToWorld(mover);
	brushframe.ObjectToWorld = ToWorld;

	frame->Device->SetSceneNode(&brushframe);
	bool needTranslucentPass = false;

	// Each polygon once, as the brush tracker last filed it: a translucent
	// or modulated one blends again each time it is drawn
	const Array<Fragments>* filed = engine->Level->Collision.FiledFragments(mover);
	const Array<Poly>& polys = brush->Polys->Polys;
	for (size_t i = 0; i < polys.size(); i++)
	{
		if (DrawBrushPoly(frame, brush, polys[i], mover, translucentPass, filed && i < filed->size() ? &(*filed)[i] : nullptr))
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
	const Array<Fragments>* filed = engine->Level->Collision.FiledFragments(mover);
	size_t index = (size_t)(&poly - mover->Brush()->Polys->Polys.data());
	DrawBrushPoly(frame, mover->Brush(), poly, mover, false, filed && index < filed->size() ? &(*filed)[index] : nullptr, true);
	frame->Device->SetSceneNode(&frame->Frame);
}

bool VisibleBrush::DrawBrushPoly(VisibleFrame* frame, UModel* model, const Poly& poly, UMover* mover, bool translucentPass, const Fragments* fragments, bool seal)
{
	// One the brush tracker dropped whole -- in solid, or for want of room --
	// is in no node of the BSP, and the original's walk never meets it
	if (fragments && fragments->empty() && !seal)
		return false;

	// The surface the brush tracker files for the polygon, its texture's own
	// flags with it, as the original's walk takes every surface of the BSP,
	// a mover's among them: a grate whose texture alone is masked is drawn
	// masked (Render.dll OccludeBsp 0x10b173d0 ORs the surface's
	// Texture->PolyFlags in at 0x10b181b2, as VisibleFrame::ProcessNodeSurface
	// does a level surface's). Its back-face test (0x10b17fc9) comes before,
	// on the surface's own flags alone.
	const uint32_t filedFlags = CollisionSystem::FiledFlags(mover, poly);
	uint32_t PolyFlags = filedFlags;
	if (poly.Texture)
		PolyFlags |= poly.Texture->PolyFlags();

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
		if (!(filedFlags & PF_TwoSided) && !poly.Vertices.empty())
		{
			// The normal as the filing turns it: the matrix takes it
			// times PostScale, the filing over it (the stock brushes'
			// MainScale is 1 or -1)
			vec3 postScale = mover->PostScale().Scale;
			vec3 normal = (ToWorld * vec4(poly.Normal, 0.0f)).xyz() / (postScale * postScale);
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

	SurfaceInfo surfaceinfo;

	// The frame drawn, as on the level's surfaces (VisibleNode), the
	// original's DrawFrame taking a mover's polygons as the BSP's own: the
	// polygon's texture stepped and its current frame, the level's default
	// texture as it is without one, and the viewer's Sprite's current frame
	// in its place with the Matrix cheat
	UTexture* tex;
	if (poly.Texture)
	{
		engine->render->UpdateTexture(poly.Texture);
		tex = poly.Texture->GetAnimTexture();
	}
	else
		tex = engine->LevelInfo->DefaultTexture();
	if (UTexture* sprite = engine->render->ViewerSprite())
	{
		engine->render->UpdateTexture(sprite);
		tex = sprite->GetAnimTexture();
	}

	TextureInfo texture;
	TextureInfo detailtex;
	TextureInfo macrotex;
	if (tex)
	{
		engine->render->UpdateTexture(tex);
		surfaceinfo.Texture = &texture;

		// Its detail and macro textures the frame's own, each locked as it
		// is and stepped by its lock, not panned with the polygon, as
		// VisibleNode's
		UTexture* detailTexture = engine->render->DetailTexturesDrawn && (PolyFlags & PF_Portal) == 0 ? tex->DetailTexture() : nullptr;
		if (detailTexture)
		{
			engine->render->UpdateTexture(detailTexture);
			engine->render->UpdateTextureInfo(detailtex, detailTexture);
			surfaceinfo.DetailTexture = &detailtex;
		}

		if (UTexture* macroTexture = tex->MacroTexture())
		{
			engine->render->UpdateTexture(macroTexture);
			engine->render->UpdateTextureInfo(macrotex, macroTexture);
			surfaceinfo.MacroTexture = &macrotex;
		}
	}
	surfaceinfo.PolyFlags = PolyFlags;

	// The zone a part is drawn in, which pans its texture at its speeds and
	// gives its light map its ambient light and its fog map its fog, as the
	// original's draw list takes a surface's zone (dx-reverse-info
	// render-dll.md, light maps)
	TextureInfo lightmap;
	TextureInfo fogmap;
	UZoneInfo* drawnZone = nullptr;
	auto drawIn = [&](UZoneInfo* zone)
	{
		if (zone == drawnZone)
			return;
		drawnZone = zone;
		if (tex)
			engine->render->UpdateTextureInfo(texture, poly, tex, zone->TexUPanSpeed(), zone->TexVPanSpeed());
		lightmap = {};
		fogmap = {};
		// None in a frame not lit, as on the level's surfaces (VisibleNode)
		if ((PolyFlags & PF_Unlit) == 0 && !seal && engine->render->SceneLit)
		{
			lightmap = engine->Level->Light.GetMoverLightmap(mover, poly, zone, model);
			// Deus Ex's only as VisibleNode's makes one
			if (frame->VolumetricFog && (PolyFlags & PF_Translucent) == 0)
				fogmap = engine->Level->Light.GetMoverFogmap(mover, poly, zone, model);
		}
		surfaceinfo.LightMap = lightmap.NumMips != 0 ? &lightmap : nullptr;
		surfaceinfo.FogMap = fogmap.NumMips != 0 ? &fogmap : nullptr;
	};

	// A step toward the viewer, as the original's walk draws a polygon its
	// brush tracker filed in front of the level's surfaces before them; one
	// filed in the plane of a surface of the level's own hangs at the end of
	// that plane's chain, after the level's surface, which hides it where it
	// occludes (the floor of 00_Intro under its trap door: bugs.md REN.26),
	// so a step away
	auto drawPart = [&](const vec3* corners, int count, bool behindLevel)
	{
		SurfaceFacet facet;
		facet.MapCoords.Origin = poly.Base;
		facet.MapCoords.XAxis = poly.TextureU;
		facet.MapCoords.YAxis = poly.TextureV;
		facet.Vertices = const_cast<vec3*>(corners);
		facet.VertexCount = count;
		surfaceinfo.DepthBias = behindLevel ? -1 : 1;
		frame->Device->DrawComplexSurface(&frame->Frame, surfaceinfo, facet);
	};

	// The zone the polygon is drawn in. One the tracker does not file
	// (spawned after the load) takes the mover's own, and so does a mirror's
	// seal, its depth alone. A filed one takes the zones of the fragments
	// its last filing left, as the original's walk draws each fragment's
	// node: in the node's zone on the side of its own plane the eye lies --
	// its parent's on its side, or, hung in a node's plane, that plane's on
	// each side --, the LevelInfo for a zone with no ZoneInfo (Render.dll
	// OccludeBsp 0x10b173d0: the side at 0x10b17d9f, a plane chain's next
	// node's by its own plane at 0x10b19b8c, the zone at 0x10b19561 and its
	// actor at 0x10b19577, whose ambient light SetupForSurf takes,
	// 0x10b06e83 and 0x10b07fa8)
	CollisionSystem& collision = engine->Level->Collision;
	const vec4& eye = frame->ViewLocation;
	auto zoneOf = [&](const BrushTracker::FiledFragment& fragment) -> UZoneInfo*
	{
		const BspNode& node = collision.FiledNode(fragment.Node);
		bool front = node.PlaneZ * eye.z + node.PlaneY * eye.y + eye.x * node.PlaneX - node.PlaneW > 0.0f;
		return engine->GetZoneActor(front ? node.Zone1 : node.Zone0);
	};
	UZoneInfo* zoneActor = nullptr;
	if (!fragments || seal)
	{
		zoneActor = mover->Region().Zone;
		if (!zoneActor)
			zoneActor = engine->LevelInfo;
	}
	else
	{
		zoneActor = zoneOf(fragments->front());
	}
	bool acrossZones = false;
	bool inLevelPlane = false;
	if (fragments)
	{
		for (const BrushTracker::FiledFragment& fragment : *fragments)
		{
			inLevelPlane = inLevelPlane || fragment.InLevelPlane;
			if (!seal && zoneOf(fragment) != zoneActor)
				acrossZones = true;
		}
	}

	// In one zone, as all but a few are, the polygon drawn whole, where the
	// original draws its fragments, by choice: the same picture, without the
	// seams its splits leave between them; a step away where some fragment
	// hangs in a level surface's plane
	if (!acrossZones)
	{
		drawIn(zoneActor);
		drawPart(poly.Vertices.data(), (int)poly.Vertices.size(), inLevelPlane);
		return false;
	}

	// Across zones, each fragment in its own, as the original's walk draws it
	for (const BrushTracker::FiledFragment& fragment : *fragments)
	{
		drawIn(zoneOf(fragment));
		drawPart(collision.FiledCorners(fragment.Node), collision.FiledNode(fragment.Node).NumVertices, fragment.InLevelPlane);
	}
	return false;
}
