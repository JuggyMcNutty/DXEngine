#pragma once

#include "VisibleFrame.h"
#include <chrono>

class RenderDevice;
class UWindow;
class URootWindow;
struct TextureColor;
class UFont;
class UMover;
class UViewportWindow;

class RenderSubsystem
{
public:
	RenderSubsystem(RenderDevice* renderdevice);

	void PreRenderWindows(UCanvas* canvas);
	void PostRenderWindows(UCanvas* canvas);
	void DrawWindowInfo(UFont* font, UWindow* window, int depth, float& curY);
	void DrawWindow(UWindow* window, float offsetX, float offsetY);
	void ResetWindowGC(UWindow* window, float offsetX, float offsetY);
	// A script's GetGC and ReleaseGC: the GC's state kept, the window's put in
	// it, and put back
	void PushWindowGC(UWindow* window);
	void PopWindowGC();
	Array<std::function<void()>> WindowGCStack;

	void DrawGame(float levelTimeElapsed);
	void OnMapLoaded();

	// The collector's: the coronas, corona lights and iterator actors let
	// go of the dying (GC::IsDying); the device flushed when a freed
	// texture may sit in its cache, which holds textures by address.
	void PurgeDying();
	void FlushDevice();
	bool DrawnSinceFlush = false;
	bool DetailTexturesDrawn = false; // the device's DetailTextures and the client's TextureDetail High: once a frame (DrawGame)

	// The last frame drawn, as the device reads it back. Only between
	// frames: a read inside one would end the frame the device is still
	// recording. False when there is none to read.
	bool ReadLastFrame(Array<TextureColor>& pixels, int& width, int& height);

	// An actor drawn on its own: Canvas.DrawActor (hideAfter: left hidden,
	// as the original's leaves it) and GC.DrawActor (its flag put back), and
	// Canvas.DrawClippedActor through a frame of its own (ActorFrame)
	// With constrain, through the scene's frame narrowed to that rectangle
	// of the screen (GC.DrawActor's bConstrain)
	void DrawActor(UActor* actor, bool ClearZ, bool hideAfter, const Rectf* constrain = nullptr, bool wire = false);
	void DrawClippedActor(UActor* actor, int X, int Y, int XB, int YB, bool ClearZ, bool wire = false);
	void DrawTile(UTexture* Tex, float x, float y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 color, vec4 fog, uint32_t flags);
	void DrawTileClipped(UTexture* Tex, float orgX, float orgY, float curX, float curY, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 color, vec4 fog, uint32_t flags, float clipX, float clipY);
	void DrawText(UFont* font, vec4 color, float orgX, float orgY, float& curX, float& curY, float& curXL, float& curYL, bool newlineAtEnd, const std::string& text, uint32_t polyflags, bool center, float spaceX = 0.0f, float spaceY = 0.0f, float clipX = 100000.0f, float clipY = 100000.0f, bool noDraw = false);
	void DrawTextClipped(UFont* font, vec4 color, float orgX, float orgY, float curX, float curY, const std::string& text, uint32_t polyflags, bool checkHotKey, float clipX, float clipY);
	vec2 GetTextSize(UFont* font, const std::string& text, float spaceX = 0.0f, float spaceY = 0.0f);

	void DrawTile(TextureInfo& Info, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 Color, vec4 Fog, uint32_t PolyFlags);
	void Draw2DLine(vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2, bool useUIScale);
	void UpdateTexture(UTexture* tex);
	// The Matrix cheat's texture: the viewing player's Sprite, which the
	// original draws every surface, decal and corona of a frame in, as
	// every mesh and sprite (Render.dll DrawFrame 0x10b1a2d0); none without.
	// Taken once a frame (DrawGame).
	UTexture* ViewerSprite() { return FrameViewerSprite; }
	UTexture* FrameViewerSprite = nullptr;

	// A ViewportWindow's frame, as the original's XViewportWindow::Draw;
	// whether one was drawn
	bool DrawViewport(UViewportWindow* viewport);

	bool ShowRenderStats = false;

	int TextureFrameCounter = 0;
	int FrameCounter = 0;
	int SceneFirstFrame = 0;   // the main view's FrameCounter: a frame's place in the scene counts from it

	// The actors with a live render iterator, gathered by DrawScene's actor
	// pass; each scene frame draws the items their iterators list
	// (VisibleFrame::ProcessRenderIterators).
	Array<UActor*> IteratorActors;
	// The level whose static actors are filed, as of its last sort, and those
	// of them with a render iterator (UpdateStaticActors)
	ULevel* StaticActorsLevel = nullptr;
	int StaticActorsSorted = -1;
	Array<UActor*> StaticIteratorActors;
	// Its movers with a mirror among their faces, found with them, which
	// VisibleFrame::ProcessMoverMirrors looks at each frame
	Array<UMover*> MirrorMovers;

	// Deus Ex coronas as the original's (dx-reverse-info/render-dll.md, coronas):
	// up to 32 kept from frame to frame, each with a brightness 0 to 1,
	// fading on real time. The dynamic corona lights are gathered by
	// DrawScene's actor pass; the static ones come from the viewer's leaf.
	struct CoronaState
	{
		UActor* Light = nullptr;
		float Brightness = 0.0f;
	};
	Array<CoronaState> CoronaStates;
	std::chrono::steady_clock::time_point CoronaLastUpdate = {};
	std::chrono::steady_clock::time_point LastDrawTime = {};
	void DrawCoronasDX(VisibleFrame* frame);

	vec3* GetTempVertexBuffer(size_t count)
	{
		if (VertexBuffer.size() < count)
			VertexBuffer.resize(count);
		return VertexBuffer.data();
	}

	GouraudVertex* GetTempGouraudVertexBuffer(size_t count)
	{
		if (GouraudVertexBuffer.size() < count)
			GouraudVertexBuffer.resize(count);
		return GouraudVertexBuffer.data();
	}

	void UpdateTextureInfo(TextureInfo& info, BspSurface& surface, UTexture* texture, float ZoneUPanSpeed, float ZoneVPanSpeed);
	void UpdateTextureInfo(TextureInfo& info, const Poly& poly, UTexture* texture, float ZoneUPanSpeed, float ZoneVPanSpeed);
	void UpdateTextureInfo(TextureInfo& info, UTexture* texture);
	static void ApplyDetailLOD(TextureInfo& info);


	RenderDevice* Device = nullptr;

	struct
	{
		Array<UTexture*> textures;
		UTexture* envmap = nullptr;
	} Mesh;

	struct
	{
		int Frames = 0;
		int Surfaces = 0;
		int Actors = 0;
		int LightmapsUpdated = 0;
	} Stats;

	VisibleFrame MainFrame;
	VisibleFrame ViewportFrame; // a ViewportWindow's view: the zoom, the drone, a camera's feed
	VisibleFrame ActorFrame; // Canvas.DrawClippedActor's: the eye at its scene node's origin
	// The frame Canvas.DrawActor draws through: the main view's, or a
	// ViewportWindow's while its overlays draw (DrawViewport)
	VisibleFrame* CanvasScene = &MainFrame;
	SceneNode MainView; // The main scene's view, which a ViewportWindow's draw leaves alone
	// While a ViewportWindow's frame is drawn, the rectangle in the screen's
	// pixels it and its child frames (mirrors, warp zones, the sky) take, in
	// place of the root's render viewport (DrawViewport)
	struct
	{
		bool Set = false;
		int X = 0, Y = 0, Width = 0, Height = 0;
	} ViewportRect;

private:
	void DrawScene();
	void UpdateRenderInterface(UActor* actor);
	void UpdateStaticActors(bool hasRenderIterators);

	void ResetCanvas();
	void PreRender();
	void RenderOverlays();
	void PostRender();
	void PostRenderFlash();
	void DrawRawBackground(URootWindow* root);
	void DrawTimedemoStats();
	void DrawTile(TextureInfo& texinfo, const Rectf& dest, const Rectf& src, const Rectf& clipBox, float Z, vec4 color, vec4 fog, uint32_t flags);

	static Array<std::string> FindTextBlocks(const std::string& text);
	void DrawTextBlockRange(float x, float y, const Array<std::string>& textBlocks, size_t start, size_t end, UFont* font, vec4 color, uint32_t polyflags, float spaceX);

	float LevelTimeElapsed = 0.0f;
	double TextureClock = 0.0; // the textures' clock at this frame (UTexture::ClockNow)
	float AutoUV = 0.0f;

	bool FrameInProgress = false;
	bool FrameDrawn = false;

	struct
	{
		int uiscale = 1;
		int fps = 0;
		int framesDrawn = 0;
		uint64_t startFPSTime = 0;
		SceneNode Frame;
	} Canvas;

	Array<vec3> VertexBuffer;
	Array<GouraudVertex> GouraudVertexBuffer;
};
