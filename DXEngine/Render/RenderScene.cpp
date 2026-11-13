
#include "Precomp.h"
#include "RenderSubsystem.h"
#include "RenderDevice/RenderDevice.h"
#include "GameWindow.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include "VisibleFrame.h"
#include "VisibleActor.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"
#include "Packages/Engine/URenderIterator.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/UCanvas.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"
#include "Packages/Engine/Actors/Brush/UMover.h"
#include "Packages/Engine/Subsystems/UDXEngineRenderDevice.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Extension/Windows/UViewportWindow.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"
#include <optional>

void RenderSubsystem::DrawScene()
{
	if (!engine->Level)
		return;

	engine->Level->Light.BeginFrame();

	// The movers and actors filed, timed for STAT GLOBAL's Filter, as the
	// original's SetupDynamics (Render.dll 0x10b22df1-0x10b2334d)
	std::optional<StatClock> filterClock(std::in_place, PanelStats.Filter);

	// Deus Ex's movers filed again where they are, as the original's
	// SetupDynamics files each it does not pass over, each frame drawn:
	// first, a mover being placed and drawn where it is filed; none while
	// the actors are hidden (ActorsShown)
	if (ActorsShown)
		engine->Level->Collision.RefileMovers([](UMover* mover) { return !VisibleActor::PassedOver(mover, 0); });

	// Make sure all actors are at the right location in the BSP
	IteratorActors.clear();
	// Only where Actor has the render interface properties
	bool hasRenderIterators = PropOffsets_Actor.RenderInterface.DataOffset != ~(size_t)0 &&
		PropOffsets_Actor.RenderIteratorClass.DataOffset != ~(size_t)0;
	UpdateStaticActors(hasRenderIterators);
	for (size_t i = std::min(engine->Level->FirstDynamicActor, engine->Level->Actors.size()); i < engine->Level->Actors.size(); i++)
	{
		if (UActor* actor = engine->Level->Actors[i])
		{
			actor->UpdateBspInfo();
			if (hasRenderIterators)
			{
				UpdateRenderInterface(actor);
				if (actor->RenderInterface())
					IteratorActors.push_back(actor);
			}
		}
	}
	filterClock.reset();
	mat4 worldToView = Coords::ViewToRenderDev().ToMatrix() * Coords::Rotation(engine->CameraRotation).Inverse().ToMatrix() * Coords::Location(engine->CameraLocation).ToMatrix();
	SceneFirstFrame = FrameCounter;
	MainFrame.Process(engine->CameraLocation, worldToView, Coords::Rotation(engine->CameraRotation));
	MainView = MainFrame.Frame;
	MainFrame.Draw();
	// The render device's Coronas gates the pass, as the original's DrawFrame
	// tests it before its corona pass (Render.dll 0x10b1a2d0): the game's ini
	// turns them off for SoftDrv
	if (engine->renderdev->Coronas)
		MainFrame.DrawCoronas();
}

// The level's static actors, which do not move -- the level puts them first,
// before its FirstDynamicActor, as the original's does -- are filed in the BSP
// when its actors are sorted, and then a sixteenth of them a frame, in turn,
// should a script change one's look; only those with a render iterator are
// visited each frame. The rest, the dynamic actors, are filed every frame.
void RenderSubsystem::UpdateStaticActors(bool hasRenderIterators)
{
	ULevel* level = engine->Level;
	size_t firstDynamic = std::min(level->FirstDynamicActor, level->Actors.size());
	bool all = StaticActorsLevel != level || StaticActorsSorted != level->ActorsSorted;
	if (all)
	{
		StaticActorsLevel = level;
		StaticActorsSorted = level->ActorsSorted;
		StaticIteratorActors.clear();

		// The movers with a mirrored face, static or not: a brush's faces
		// keep their flags
		MirrorMovers.clear();
		for (UActor* actor : level->Actors)
		{
			UMover* mover = UActor::TryCast<UMover>(actor);
			if (!mover || !mover->Brush() || !mover->Brush()->Polys)
				continue;
			for (const Poly& poly : mover->Brush()->Polys->Polys)
			{
				if (poly.PolyFlags & PF_Mirrored)
				{
					MirrorMovers.push_back(mover);
					break;
				}
			}
		}
	}

	size_t turn = (size_t)(TextureFrameCounter & 15);
	for (size_t i = all ? 0 : turn; i < firstDynamic; i += all ? 1 : 16)
	{
		UActor* actor = level->Actors[i];
		if (!actor)
			continue;
		actor->UpdateBspInfo();
		if (hasRenderIterators && actor->RenderIteratorClass() &&
			std::find(StaticIteratorActors.begin(), StaticIteratorActors.end(), actor) == StaticIteratorActors.end())
		{
			StaticIteratorActors.push_back(actor);
		}
	}

	if (hasRenderIterators)
	{
		for (UActor* actor : StaticIteratorActors)
		{
			UpdateRenderInterface(actor);
			if (actor->RenderInterface())
				IteratorActors.push_back(actor);
		}
	}
}

void RenderSubsystem::UpdateRenderInterface(UActor* actor)
{
	// Without a RenderInterface, or with one that is no longer valid, the
	// renderer makes one: an object of the actor's RenderIteratorClass with
	// the actor as its outer. With the class cleared, the one there is goes
	// (dx-reverse-info/render-dll.md, render iterators).
	UClass* cls = actor->RenderIteratorClass();
	URenderIterator*& iterator = actor->RenderInterface();
	if (!cls)
	{
		iterator = nullptr;
		return;
	}
	if (iterator && iterator->Class != cls)
		iterator = nullptr;
	if (!iterator)
	{
		UObject* obj = engine->packages->GetTransientPackage()->NewObject(cls->Name, cls, ObjectFlags::Transient);
		iterator = UObject::TryCast<URenderIterator>(obj);
		if (iterator)
			iterator->Outer() = actor;
	}
}

void RenderSubsystem::DrawCoronasDX(VisibleFrame* frame)
{
	// The original's coronas (dx-reverse-info/render-dll.md, coronas): the
	// static lights shining into the viewer's own leaf of the BSP -- its
	// permeating list -- with bCorona and a Skin, at any distance. No
	// dynamic light: the original's pass reads the leaf's dynamic lights
	// only after its occlusion pass has emptied their list. One is seen when
	// the line from the eye meets no level geometry or mover, and no pawn or
	// other actor but the viewer's own pawn. Each frame every corona loses
	// three times the real seconds elapsed and each one seen gains twice
	// that, so one comes up, or goes, in about a third of a second; at 0 it
	// is dropped, and up to 32 are kept.
	UModel* model = engine->Level->Model;
	UActor* viewerPawn = engine->viewport->Actor();
	vec3 eye = frame->ViewLocation.xyz();

	// The viewer's leaf: its pawn's, as the original reads the viewport
	// actor's region; where there is none, the eye's
	int leaf = viewerPawn ? viewerPawn->Region().BspLeaf : -1;
	if (leaf < 0)
		leaf = model->FindLeafAt(eye);

	// Which lights, this frame
	Array<UActor*> candidates;
	if (leaf >= 0 && (size_t)leaf < model->Leaves.size())
	{
		int index = model->Leaves[leaf].Permeating;
		if (index >= 0)
		{
			for (size_t i = index; i < model->Lights.size() && model->Lights[i]; i++)
			{
				UActor* light = model->Lights[i];
				if (light->bCorona() && light->Skin() && !light->bDeleteMe())
					candidates.push_back(light);
			}
		}
	}

	// Seen, as the original's CoronaTest: the eye reaches the light past
	// the level and the movers -- every surface, an invisible one too,
	// which a sight test passes: a glass mover's collision hull round its
	// pane -- then past pawns and other actors, but the viewer's own pawn
	auto blocksSight = [&](UActor* actor) { return actor != viewerPawn; };
	auto clear = [&](UActor* light)
	{
		return !engine->Level->Collision.TraceAnyHit(eye, light->Location(), nullptr, false, true, false) &&
			!engine->Level->Collision.SightBlocked(eye, light->Location(), blocksSight);
	};
	auto seenNow = [&](UActor* light)
	{
		for (UActor* candidate : candidates)
			if (candidate == light)
				return clear(light);
		return false;
	};

	// Fade on real time
	auto now = std::chrono::steady_clock::now();
	float elapsed = 0.0f;
	if (CoronaLastUpdate.time_since_epoch().count() != 0)
		elapsed = clamp(std::chrono::duration<float>(now - CoronaLastUpdate).count(), 0.0f, 0.1f);
	CoronaLastUpdate = now;

	for (size_t i = 0; i < CoronaStates.size();)
	{
		CoronaState& state = CoronaStates[i];
		bool seen = !state.Light->bDeleteMe() && seenNow(state.Light);
		state.Brightness -= 3.0f * elapsed;
		if (seen)
			state.Brightness += 6.0f * elapsed;
		state.Brightness = std::min(state.Brightness, 1.0f);
		if (state.Brightness <= 0.0f && !seen)
		{
			CoronaStates.erase(CoronaStates.begin() + i);
			continue;
		}
		state.Brightness = std::max(state.Brightness, 0.0f);
		i++;
	}
	for (UActor* light : candidates)
	{
		bool known = false;
		for (const CoronaState& state : CoronaStates)
		{
			if (state.Light == light)
			{
				known = true;
				break;
			}
		}
		if (!known && CoronaStates.size() < 32 && clear(light))
		{
			CoronaState state;
			state.Light = light;
			state.Brightness = 3.0f * elapsed;
			CoronaStates.push_back(state);
		}
	}

	// Drawn at the light's place on screen, if in front of the eye: a
	// square a fifth of the view's width times the light's DrawScale,
	// whatever the distance, translucent, in the colour of its hue and
	// saturation times the brightness
	for (const CoronaState& state : CoronaStates)
	{
		UActor* light = state.Light;
		if (state.Brightness <= 0.0f || light->bDeleteMe() || !light->Skin() || light->Skin()->UsedMipmaps.empty())
			continue;

		vec4 pos = frame->Frame.WorldToView * frame->Frame.ObjectToWorld * vec4(light->Location(), 1.0f);
		if (pos.z < 1.0f)
			continue;
		vec4 clip = frame->Frame.Projection * pos;
		float x = frame->Frame.FX2 + clip.x / clip.w * frame->Frame.FX2;
		float y = frame->Frame.FY2 + clip.y / clip.w * frame->Frame.FY2;

		// The skin's current frame, brought up to date and keyed as every
		// texture is (UpdateTextureInfo): an animated skin's frames were all
		// cached under the first one's ID, and a fractal skin's changes never
		// sent again
		// The viewer's Sprite, the Matrix cheat's, is written into the
		// light's Skin as its corona is drawn, and stays there, as the
		// original's DrawFrame writes it
		if (UTexture* sprite = engine->render->ViewerSprite())
		{
			engine->render->UpdateTexture(sprite);
			light->Skin() = sprite->GetAnimTexture();
		}
		UTexture* skin = light->Skin();
		engine->render->UpdateTexture(skin);
		UTexture* skinFrame = skin->GetAnimTexture();
		engine->render->UpdateTexture(skinFrame);
		if (skinFrame->UsedMipmaps.empty())
			continue;

		TextureInfo texinfo;
		engine->render->UpdateTextureInfo(texinfo, skinFrame);

		float width = (float)skinFrame->UsedMipmaps.front().Width;
		float height = (float)skinFrame->UsedMipmaps.front().Height;
		float size = light->DrawScale() * frame->Frame.FX * 0.2f;
		// The hue's colour, whitened by the saturation, times the fade -- as
		// DrawFrame works it out itself, with none of the light maps'
		// brightness curve (FGetHSV)
		uint8_t hue = light->LightHue();
		vec3 huecolor = hue < 86 ? vec3((85 - hue) / 85.0f, hue / 85.0f, 0.0f) :
			hue < 171 ? vec3(0.0f, (170 - hue) / 85.0f, (hue - 85) / 85.0f) :
			vec3((hue - 170) / 85.0f, 0.0f, (255 - hue) / 84.0f);
		float saturation = light->LightSaturation() / 255.0f;
		vec3 lightcolor = (huecolor + saturation * (vec3(1.0f) - huecolor)) * state.Brightness;

		frame->Device->DrawTile(
			&frame->Frame,
			texinfo, x - size * 0.5f,
			y - size * 0.5f,
			size,
			size,
			0.0f,
			0.0f,
			width,
			height,
			2.0f,
			vec4(lightcolor, 1.0f),
			vec4(0.0f),
			PF_Translucent);
	}
}

// The original's XViewportWindow::Draw (Extension.dll 0x1004a770). An
// origin actor gone -- no longer one of the player's actor references
// (IsActorValid), as a destroyed one is not -- leaves the view where it was
// last drawn from: the window's location set to the last place, relLocation
// cleared, its rotation the last turn less relRotation; a watch actor gone,
// at the last turn, relRotation cleared. Then, the window turned on, the
// view from the origin actor -- up its EyeHeight for a pawn with
// bUseEyeHeight, a player's ViewRotation with bUseViewRotation -- or from
// the window's location; moved by relLocation along the view; turned to the
// watch actor (up its EyeHeight for a pawn with bWatchEyeHeight), then by
// relRotation; the script's CalcView may move and turn it, and the place
// and turn are kept as the last. The frame is the window's clip rectangle,
// the part of it its parents leave in view, in the screen's pixels: none
// with no pixel across or down. Drawn as the player's own view but from
// there: for the draw the player is in behind view (so it and what it owns
// show as in third person) unless bShowWeapons is on the player's own view,
// its field of view is the window's, and the origin actor is hidden unless
// bShowActor; all put back after (dx-reverse-info extension-dll.md, a
// viewport window). Whether a frame was drawn: with none the window tiles
// its default texture (UViewportWindow::DrawWindow).
bool RenderSubsystem::DrawViewport(UViewportWindow* viewport)
{
	if (!engine->Level)
		return false;

	// The actors gone, before anything else (0x1004a7b6, 0x1004a85e)
	if (viewport->originActor() && !viewport->IsActorValid(viewport->originActor()))
	{
		viewport->originActor() = nullptr;
		viewport->Location() = viewport->lastLocation();
		viewport->relLocation() = vec3(0.0f);
		viewport->Rotation() = viewport->lastRotation() - viewport->relRotation();
	}
	if (viewport->watchActor() && !viewport->IsActorValid(viewport->watchActor()))
	{
		viewport->watchActor() = nullptr;
		viewport->Rotation() = viewport->lastRotation();
		viewport->relRotation() = Rotator(0, 0, 0);
	}

	if (!viewport->bEnableViewport())
		return false;

	UActor* originActor = viewport->originActor();
	vec3 location;
	Rotator rotation(0,0,0);
	if (originActor)
	{
		location = originActor->Location();
		UPawn* pawn = UObject::TryCast<UPawn>(originActor);
		if (pawn && viewport->bUseEyeHeight())
			location.z += pawn->EyeHeight();
		UPlayerPawn* originPlayer = UObject::TryCast<UPlayerPawn>(originActor);
		rotation = (originPlayer && viewport->bUseViewRotation()) ? originPlayer->ViewRotation() : originActor->Rotation();
	}
	else
	{
		location = viewport->Location();
		rotation = viewport->Rotation();
	}

	// relLocation's length along the view, turned by its own direction
	vec3 rel = viewport->relLocation();
	float relLength = length(rel);
	if (relLength > 0.0f)
		location += Coords::Rotation(rotation + Rotator::FromVector(rel)).XAxis * relLength;

	if (UActor* watchActor = viewport->watchActor())
	{
		vec3 lookAt = watchActor->Location();
		if (viewport->bWatchEyeHeight())
		{
			if (auto pawn = UObject::TryCast<UPawn>(watchActor))
				lookAt.z += pawn->EyeHeight();
		}
		rotation = Rotator::FromVector(lookAt - location);
	}

	rotation += viewport->relRotation();

	// The script's CalcView, then the place and turn kept (0x1004abb0)
	viewport->CalcView(location, rotation);
	viewport->lastLocation() = location;
	viewport->lastRotation() = rotation;

	// The frame: the window's clip rectangle times the root's multiplier,
	// each truncated (0x1004ac3c); DXEngine's 4:3 root lies across a wider
	// screen, so one over the whole root takes the whole screen, as the
	// scene's does (VisibleFrame::SetupSceneFrame)
	const ClipRect& clip = viewport->winClipRect();
	URootWindow* root = engine->dxRootWindow;
	float scale = UWindow::GetVirtualScale();
	float clipLeft = clip.originX + clip.clipX;
	float clipTop = clip.originY + clip.clipY;
	int frameX, frameY, frameWidth, frameHeight;
	if (clipLeft <= 0.0f && clip.clipWidth >= root->Width())
	{
		frameX = 0;
		frameWidth = engine->viewport->ViewportWidth();
	}
	else
	{
		frameX = (int)((root->UsedX + clipLeft) * scale);
		frameWidth = (int)(clip.clipWidth * scale);
	}
	if (clipTop <= 0.0f && clip.clipHeight >= root->Height())
	{
		frameY = 0;
		frameHeight = engine->viewport->ViewportHeight();
	}
	else
	{
		frameY = (int)((root->UsedY + clipTop) * scale);
		frameHeight = (int)(clip.clipHeight * scale);
	}
	if (frameWidth <= 0 || frameHeight <= 0)
		return false;
	ViewportRect.Set = true;
	ViewportRect.X = frameX;
	ViewportRect.Y = frameY;
	ViewportRect.Width = std::min(frameWidth, 16384);
	ViewportRect.Height = std::min(frameHeight, 16384);

	if (viewport->bClearZ())
		Device->ClearZ();

	UPlayerPawn* player = engine->viewport->Actor();
	bool playerWasBehind = false;
	float playerFov = 0.0f;
	if (player)
	{
		playerWasBehind = player->bBehindView();
		playerFov = player->FovAngle();
		player->bBehindView() = !(viewport->bShowWeapons() && originActor == player);
		player->FovAngle() = viewport->FOV();
	}
	bool originWasHidden = originActor && originActor->bHidden();
	if (originActor)
		originActor->bHidden() = !viewport->bShowActor();
	float cameraFov = engine->CameraFovAngle;
	engine->CameraFovAngle = viewport->FOV();

	mat4 worldToView = Coords::ViewToRenderDev().ToMatrix() * Coords::Rotation(rotation).Inverse().ToMatrix() * Coords::Location(location).ToMatrix();
	// A frame of its own: the main view's stays as the scene left it, for what
	// the HUD draws after (the vision augmentation's GC.DrawActor)
	ViewportFrame.Process(location, worldToView, Coords::Rotation(rotation));
	ViewportFrame.Draw();
	if (engine->renderdev->Coronas)
		ViewportFrame.DrawCoronas();

	// The overlays, as the original's DrawWorld sends RenderOverlays after
	// each frame it draws (Render.dll 0x10b1ce7f; RenderOverlays): with the
	// player out of behind view for the draw -- bShowWeapons on the player's
	// own view -- its weapon draws in the window. The depth buffer cleared,
	// then the call, through the window's frame, the canvas's frame, size
	// and clip the window's for it (Extension.dll 0x1004ad81 to 0x1004ade5),
	// all put back after.
	const int SHOW_Actors = 8;
	if (player && !player->bBehindView() && (player->ShowFlags() & SHOW_Actors))
	{
		SceneNode canvasFrame = Canvas.Frame;
		int sizeX = engine->canvas->SizeX();
		int sizeY = engine->canvas->SizeY();
		float clipX = engine->canvas->ClipX();
		float clipY = engine->canvas->ClipY();
		const SceneNode& frame = ViewportFrame.Frame;
		Canvas.Frame.XB = frame.XB;
		Canvas.Frame.YB = frame.YB;
		Canvas.Frame.X = frame.X;
		Canvas.Frame.Y = frame.Y;
		Canvas.Frame.FX = frame.FX;
		Canvas.Frame.FY = frame.FY;
		Canvas.Frame.FX2 = frame.FX2;
		Canvas.Frame.FY2 = frame.FY2;
		engine->canvas->SizeX() = frame.X;
		engine->canvas->SizeY() = frame.Y;
		engine->canvas->ClipX() = (float)engine->canvas->SizeX();
		engine->canvas->ClipY() = (float)engine->canvas->SizeY();

		Device->SetSceneNode(&ViewportFrame.Frame);
		Device->ClearZ();
		engine->Level->Light.SetEye(ViewportFrame.ViewLocation.xyz(), ViewportFrame.SceneIndex, ViewportFrame.Frame.WorldToView);
		CanvasScene = &ViewportFrame;
		UActor* target = player->ViewTarget() ? player->ViewTarget() : player;
		CallEvent(target, EventName::RenderOverlays, { ExpressionValue::ObjectValue(engine->canvas) });
		CanvasScene = &MainFrame;

		Canvas.Frame = canvasFrame;
		engine->canvas->SizeX() = sizeX;
		engine->canvas->SizeY() = sizeY;
		engine->canvas->ClipX() = clipX;
		engine->canvas->ClipY() = clipY;
	}

	// The main view's eye again, for what the HUD draws after
	engine->Level->Light.SetEye(MainFrame.ViewLocation.xyz(), MainFrame.SceneIndex, MainFrame.Frame.WorldToView);

	Device->SetSceneNode(&Canvas.Frame);

	ViewportRect.Set = false;

	// Kept for ConvertVectorToCoordinates in the window and in those inside it
	viewport->LastFrame.Drawn = true;
	viewport->LastFrame.WorldToView = ViewportFrame.Frame.WorldToView;
	viewport->LastFrame.Projection = ViewportFrame.Frame.Projection;
	viewport->LastFrame.FX = ViewportFrame.Frame.FX;
	viewport->LastFrame.FY = ViewportFrame.Frame.FY;

	engine->CameraFovAngle = cameraFov;
	if (originActor)
		originActor->bHidden() = originWasHidden;
	if (player)
	{
		player->bBehindView() = playerWasBehind;
		player->FovAngle() = playerFov;
	}
	return true;
}
