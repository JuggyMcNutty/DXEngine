
#include "Precomp.h"
#include "RenderSubsystem.h"
#include "RenderDevice/RenderDevice.h"
#include "GameWindow.h"
#include "Packages/Engine/UDXEngineClient.h"
#include "Packages/Engine/Subsystems/UDXEngineRenderDevice.h"
#include "Packages/Engine/UConsole.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/UPalette.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/UPolys.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include "Packages/Engine/Actors/Brush/UMover.h"

RenderSubsystem::RenderSubsystem(RenderDevice* renderdevice) : Device(renderdevice)
{
}

void RenderSubsystem::DrawGame(float levelTimeElapsed)
{
	DrawnSinceFlush = true;
	LevelTimeElapsed = levelTimeElapsed;
	TextureClock = UTexture::ClockNow();
	UPlayerPawn* viewer = UObject::TryCast<UPlayerPawn>(engine->viewport->Actor());
	FrameViewerSprite = viewer && viewer->Sprite() && !viewer->Sprite()->UsedMipmaps.empty() ? viewer->Sprite() : nullptr;

	// A surface's detail texture goes to the device only while its
	// DetailTextures is on and the client's TextureDetail is High, as
	// Render.dll hands it over (dx-reverse-info render-dll.md, a frame).
	DetailTexturesDrawn = engine->renderdev->DetailTextures && NameString(engine->client->TextureDetail) == NameString("High");
	AutoUV += levelTimeElapsed * 64.0f;
	// The viewport's clock, which the meshes' lighting takes: the real time
	// since the last frame drawn, a load's painted frame among them, as the
	// original's viewport takes it at each lock (Engine.dll UViewport::Lock
	// and Unlock)
	auto now = std::chrono::steady_clock::now();
	float frameElapsed = LastDrawTime == std::chrono::steady_clock::time_point() ? 0.0f : std::chrono::duration<float>(now - LastDrawTime).count();
	LastDrawTime = now;
	engine->Level->Light.Tick(frameElapsed);

	Stats.Frames = 0;
	Stats.Surfaces = 0;
	Stats.Actors = 0;
	Stats.LightmapsUpdated = 0;

	// The screen flash as the original's game engine hands it to the
	// device: the viewport's player's, the scale halved, both clamped to 0-1
	// with no fourth component -- none with the client's ScreenFlashes off,
	// which a net game overrides (dx-reverse-info/d3ddrv-dll.md, the screen
	// flash). The devices draw nothing for a scale of 0.5 and a black fog.
	vec3 flashScale = 0.5f;
	vec3 flashFog = vec3(0.0f, 0.0f, 0.0f);

	UPlayerPawn* player = engine->viewport ? engine->viewport->Actor() : nullptr;
	bool flashes = engine->client->ScreenFlashes || (engine->LevelInfo && engine->LevelInfo->NetMode() != NM_Standalone);
	if (player && flashes)
	{
		vec3 scale = player->FlashScale() * 0.5f;
		vec3 fog = player->FlashFog();
		flashScale = vec3(clamp(scale.x, 0.0f, 1.0f), clamp(scale.y, 0.0f, 1.0f), clamp(scale.z, 0.0f, 1.0f));
		flashFog = vec3(clamp(fog.x, 0.0f, 1.0f), clamp(fog.y, 0.0f, 1.0f), clamp(fog.z, 0.0f, 1.0f));
	}

	Device->Brightness = engine->client->BrightnessInUse;
	Device->Lock(vec4(flashScale, 0.0f), vec4(flashFog, 0.0f), vec4(0.0f));
	FrameInProgress = true;
	// Every frame, the world drawn or not: a texture the windows or the
	// canvas alone draw (a menu over the snapshot, the credits) steps on, as
	// the original's by its own time (fire-dll.md, stepping), and one changed
	// once is not uploaded again each frame
	TextureFrameCounter++;

	ResetCanvas();
	PreRender();

	if (engine->console->bNoDrawWorld() == false)
	{
		// With the root window's rendering off (a menu over the snapshot,
		// the black background, the credits) the original's scene frame is
		// empty: nothing of the world or the overlays is drawn, and the
		// windows still are, from PostRenderFlash
		// (dx-reverse-info/extension-dll.md, the raw background).
		// Nothing of the world while a joining client's player waits in
		// the Entry level, the renderer's level the game's.
		UPlayerPawn* viewer = engine->viewport->Actor();
		if (viewer && viewer->XLevel() == engine->Level && (!engine->dxRootWindow || engine->dxRootWindow->bRender()))
		{
			DrawScene();
			RenderOverlays();
			Device->TakeBloom();
		}
		PostRenderFlash();
		Device->EndFlash();
	}

	PostRender();

	Device->Unlock(true);
	FrameInProgress = false;
	FrameDrawn = true;
}

bool RenderSubsystem::ReadLastFrame(Array<TextureColor>& pixels, int& width, int& height)
{
	if (FrameInProgress || !FrameDrawn)
		return false;

	Device->LastFrameSize(width, height);
	if (width <= 0 || height <= 0)
		return false;

	pixels.resize((size_t)width * height);
	Device->ReadPixels(pixels.data());
	return true;
}

// A texture steps once a frame it is drawn, by the time since its own last
// update, as the original's UTexture::Update passes it (fire-dll.md,
// stepping): one drawn on some frames only counts the frames between too,
// and a MaxFrameRate texture drawn every other frame steps as often as one
// drawn every frame. The time is the real time the frame was begun at, as
// the original's viewport hands it, whatever the level's TimeDilation or
// pause
void RenderSubsystem::UpdateTexture(UTexture* tex)
{
	if (tex && tex->FrameCounter != TextureFrameCounter)
	{
		tex->Update((float)(TextureClock - tex->LastUpdateTime));
		tex->LastUpdateTime = TextureClock;
		tex->FrameCounter = TextureFrameCounter;
	}
}

void RenderSubsystem::UpdateTextureInfo(TextureInfo& info, BspSurface& surface, UTexture* texture, float ZoneUPanSpeed, float ZoneVPanSpeed)
{
	UpdateTextureInfo(info, texture);

	info.Pan.x = -(float)surface.PanU;
	info.Pan.y = -(float)surface.PanV;
	if (surface.PolyFlags & PF_AutoUPan) info.Pan.x -= AutoUV * ZoneUPanSpeed;
	if (surface.PolyFlags & PF_AutoVPan) info.Pan.y -= AutoUV * ZoneVPanSpeed;
}

void RenderSubsystem::UpdateTextureInfo(TextureInfo& info, const Poly& poly, UTexture* texture, float ZoneUPanSpeed, float ZoneVPanSpeed)
{
	UpdateTextureInfo(info, texture);

	info.Pan.x = -(float)poly.PanU;
	info.Pan.y = -(float)poly.PanV;
	if (poly.PolyFlags & PF_AutoUPan) info.Pan.x -= AutoUV * ZoneUPanSpeed;
	if (poly.PolyFlags & PF_AutoVPan) info.Pan.y -= AutoUV * ZoneVPanSpeed;
}

void RenderSubsystem::UpdateTextureInfo(TextureInfo& info, UTexture* texture)
{
	info.Texture = texture;
	info.CacheID = (uint64_t)(ptrdiff_t)texture;

	if (!info.Texture)
		return;

	info.UScale = texture->DrawScale();
	info.VScale = texture->DrawScale();
	info.Format = texture->UsedFormat;
	info.Mips = texture->UsedMipmaps.data();
	info.NumMips = (int)texture->UsedMipmaps.size();
	info.USize = texture->USize();
	info.VSize = texture->VSize();
	if (texture->Palette())
		info.Palette = (TextureColor*)texture->Palette()->Colors.data();
	ApplyDetailLOD(info);

	info.bRealtimeChanged = texture->ChangedThisFrame(TextureFrameCounter);
}

// The texture from its detail level's mip down, as the original's
// UTexture::Lock hands it over (Engine.dll 0x103ecc90): that mip's size, the
// scale grown by as much, so a texel lands where it did, and a cache entry of
// its own.
void RenderSubsystem::ApplyDetailLOD(TextureInfo& info)
{
	if (!info.Texture || info.Mips != info.Texture->UsedMipmaps.data())
		return;
	int lod = info.Texture->DetailLOD();
	if (lod <= 0 || lod >= info.NumMips)
		return;
	const UnrealMipmap& top = info.Mips[0];
	const UnrealMipmap& mip = info.Mips[lod];
	info.UScale *= (float)top.Width / (float)mip.Width;
	info.VScale *= (float)top.Height / (float)mip.Height;
	info.USize = mip.Width;
	info.VSize = mip.Height;
	info.Mips += lod;
	info.NumMips -= lod;
	// The level in bits no address or tag reaches: added to the texture's
	// address it could end in 1, 2 or 3, a light map's, fog map's or atlas
	// page's tag (LightmapAtlas), or make another texture's address
	info.CacheID |= (uint64_t)lod << 56;
}

void RenderSubsystem::PurgeDying()
{
	CoronaStates.erase(std::remove_if(CoronaStates.begin(), CoronaStates.end(), [](const CoronaState& state) { return GC::IsDying(state.Light); }), CoronaStates.end());
	IteratorActors.erase(std::remove_if(IteratorActors.begin(), IteratorActors.end(), [](UActor* actor) { return GC::IsDying(actor); }), IteratorActors.end());
	StaticIteratorActors.erase(std::remove_if(StaticIteratorActors.begin(), StaticIteratorActors.end(), [](UActor* actor) { return GC::IsDying(actor); }), StaticIteratorActors.end());
	MirrorMovers.erase(std::remove_if(MirrorMovers.begin(), MirrorMovers.end(), [](UMover* mover) { return GC::IsDying(mover); }), MirrorMovers.end());
	if (FrameViewerSprite && GC::IsDying(FrameViewerSprite))
		FrameViewerSprite = nullptr;
}

void RenderSubsystem::FlushDevice()
{
	Device->Flush(true);
	DrawnSinceFlush = false;
}

void RenderSubsystem::OnMapLoaded()
{
	Device->Flush(true);
	DrawnSinceFlush = false;
	engine->Level->Light.OnMapLoaded();
	// Nothing of the level before: its lights are not this one's, nor its
	// static actors, filed again at the next frame.
	IteratorActors.clear();
	CoronaStates.clear();
	StaticIteratorActors.clear();
	MirrorMovers.clear();
	StaticActorsLevel = nullptr;
}
