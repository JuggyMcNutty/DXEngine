
#include "Precomp.h"
#include "VisibleActor.h"
#include "VisibleFrame.h"
#include "VisibleBrush.h"
#include "VisibleMesh.h"
#include "VisibleSprite.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Subsystems/UDXEngineRenderDevice.h"

void VisibleActor::Process(VisibleFrame* frame, UActor* actor)
{
	// Deus Ex coronas come from the viewer's leaf and are kept across
	// frames (RenderSubsystem::DrawCoronasDX), not gathered here

	if (actor->bHidden())
		return;

	UActor* viewportActor = engine->viewport->Actor();
	bool isOwnedByViewport = actor->IsOwnedBy(viewportActor);

	// As the original's SetupDynamics (Render.dll 0x10b22d90): bOnlyOwnerSee
	// and bOwnerNoSee go by the viewer's bBehindView in every frame, a
	// mirror's too; the actor the view is from is left out in the main frame
	// alone, in first person (OccludeFrame 0x10b19fc0 names it), so the
	// player shows in a mirror
	bool behindView = false;
	if (UPawn* pawn = UObject::TryCast<UPawn>(viewportActor))
		behindView = pawn->bBehindView();

	if ((behindView || !isOwnedByViewport) && actor->bOnlyOwnerSee())
		return;

	if (!behindView && isOwnedByViewport && actor->bOwnerNoSee())
		return;

	if (frame->PortalDepth == 0 && !behindView && actor == engine->CameraActor)
		return;

	// Behind a mirror, or before a warp zone's portal, nothing shows in its
	// frame: the original draws an actor only through its zone's spans, which
	// that frame never reaches
	const BBox& box = actor->BspInfo.BoundingBox;
	const vec4& plane = frame->PortalPlane;
	vec3 farthest(plane.x >= 0.0f ? box.max.x : box.min.x, plane.y >= 0.0f ? box.max.y : box.min.y, plane.z >= 0.0f ? box.max.z : box.min.z);
	if (dot(plane, vec4(farthest, 1.0f)) < 0.0f)
		return;

	if (!frame->Clipper.IsAABBVisible(actor->BspInfo.BoundingBox))
		return;

	// Drawn for the depth buffer to hide; counted as drawn only if some of
	// it survives the world in front of it, as the original counts it
	frame->AddOcclusionProxy(actor);

	EDrawType dt = (EDrawType)actor->DrawType();
	if (dt == DT_Mesh && actor->Mesh())
	{
		Type = dt;
		Actor = actor;
		frame->Actors.push_back(*this);
	}
	else if ((dt == DT_Sprite || dt == DT_SpriteAnimOnce) && actor->Texture())
	{
		Type = dt;
		Actor = actor;
		frame->Actors.push_back(*this);
	}
	else if (dt == DT_Brush && actor->Brush() && actor->Brush()->Polys && UActor::TryCast<UMover>(actor))
	{
		Type = dt;
		Actor = actor;
		frame->Actors.push_back(*this);
	}
}

void VisibleActor::DrawOpaque(VisibleFrame* frame)
{
	if (Type == DT_Mesh)
	{
		VisibleMesh vismesh;
		if (vismesh.DrawMesh(frame, Actor, false, false))
		{
			vec3 v = Actor->Location() - frame->ViewLocation.xyz();
			frame->AddTranslucent(VisibleFrame::ActorTranslucentList(Actor), VisibleTranslucent(*this, dot(v, v)));
		}
	}
	else if (Type == DT_Sprite || Type == DT_SpriteAnimOnce)
	{
		// A sprite is drawn with the translucent parts, in the list its style takes
		vec3 v = Actor->Location() - frame->ViewLocation.xyz();
		frame->AddTranslucent(VisibleFrame::ActorTranslucentList(Actor), VisibleTranslucent(*this, dot(v, v)));
	}
	else if (Type == DT_Brush)
	{
		// A mover's translucent polygons go with the level's surfaces, as
		// the original files a mover's polygons into the level's BSP
		VisibleBrush visbrush;
		if (visbrush.Draw(frame, Actor, false))
		{
			vec3 v = Actor->Location() - frame->ViewLocation.xyz();
			frame->AddTranslucent(VisibleFrame::TranslucentSurfaces, VisibleTranslucent(*this, dot(v, v)));
		}
	}
}

void VisibleActor::DrawTranslucent(VisibleFrame* frame)
{
	if (Type == DT_Mesh)
	{
		VisibleMesh vismesh;
		vismesh.DrawMesh(frame, Actor, false, true);
	}
	else if (Type == DT_Sprite || Type == DT_SpriteAnimOnce)
	{
		VisibleSprite vissprite;
		vissprite.Draw(frame, Actor);
	}
	else if (Type == DT_Brush)
	{
		VisibleBrush visbrush;
		visbrush.Draw(frame, Actor, true);
	}
}
