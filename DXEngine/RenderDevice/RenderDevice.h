#pragma once

#include "Math/vec.h"
#include "Math/mat.h"
#include "Math/coords.h"

#include "Packages/Engine/Resources/Textures/UTexture.h"

#include <dxwidgets/core/canvas.h>
#include <dxwidgets/core/rect.h>

class UTexture;
class UActor;
class Widget;
enum class RenderAPI;

struct SceneNode
{
	int XB, YB; // viewport top left
	int X, Y; // viewport size
	float FX, FY;
	float FX2, FY2;
	Widget* Viewport = nullptr;
	float FovAngle;

	mat4 ObjectToWorld;
	mat4 WorldToView;
	mat4 Projection;

	vec4 NearClip = vec4(0.0f, 0.0f, 1.0f, -1.0f);
	float Zoom = 1.0f;

	// A 3D frame's: its polygons half a pixel right of and below where the
	// projection puts them, as D3DDrv draws the original's. Render.dll
	// projects a polygon's corners about FX15, (FX + 1) / 2, but a sprite's
	// place and a corona's about FX2, and D3DDrv draws each half a pixel up
	// and left of its coordinates; a tile, and so the canvas, keeps the
	// projection's centre, as a sprite does (VisibleFrame::TileOffset).
	bool PolygonsAtFX15 = false;
};

// What a device puts before a frame's projection: the half pixel of
// SceneNode::PolygonsAtFX15 in clip space, NDC's y down the frame on each.
inline mat4 PolygonShift(const SceneNode* frame)
{
	if (!frame->PolygonsAtFX15 || frame->X <= 0 || frame->Y <= 0)
		return mat4::identity();
	return mat4::translate(1.0f / frame->X, 1.0f / frame->Y, 0.0f);
}

// Hor+: a view wider than 4:3 keeps the height a 4:3 view of the angle
// given would have and sees more at the sides, where the original keeps the
// angle across the width whatever the screen (docs/NATIVES.md, the
// view's width). The angle the view's frame takes.
inline float HorPlusFovAngle(float fovAngle, float width, float height)
{
	float widen = std::max(1.0f, (width / height) / (4.0f / 3.0f));
	return degrees(2.0f * std::atan(std::tan(radians(fovAngle) * 0.5f) * widen));
}

struct GouraudVertex
{
	vec3 Point;
	vec3 Light;
	vec2 UV;
	vec4 Fog;
};

struct SurfaceFacet
{
	Coords MapCoords;
	vec3* Vertices;
	uint32_t VertexCount;
};

struct TextureColor
{
	TextureColor() = default;
	TextureColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : R(r), G(g), B(b), A(a) { }

	uint8_t R, G, B, A;
};

enum class TextureFormat : uint32_t;

struct TextureInfo
{
	uint64_t CacheID = 0;
	bool bRealtimeChanged = false;

	// With bRealtimeChanged, the part of a one-mip texture that changed, when
	// not all of it did (UpdateWidth > 0): a device that already holds the
	// texture uploads only that part
	int UpdateX = 0;
	int UpdateY = 0;
	int UpdateWidth = 0;
	int UpdateHeight = 0;

	UTexture* Texture = nullptr;
	float UScale = 1.0f;
	float VScale = 1.0f;
	vec2 Pan = { 0.0f };

	// to do: give these correct values
	TextureFormat Format = {};
	int USize = 1;
	int VSize = 1;
	int NumMips = 0;
	UnrealMipmap* Mips = nullptr;
	TextureColor* Palette = nullptr;
};

inline float GetUMult(const TextureInfo& Info) { return 1.0f / (Info.UScale * Info.USize); }
inline float GetVMult(const TextureInfo& Info) { return 1.0f / (Info.VScale * Info.VSize); }

struct SurfaceInfo
{
	uint32_t PolyFlags = 0;
	TextureInfo* Texture = nullptr;
	TextureInfo* LightMap = nullptr;
	TextureInfo* MacroTexture = nullptr;
	TextureInfo* DetailTexture = nullptr;
	TextureInfo* FogMap = nullptr;
	// Drawn a step toward the viewer, as the Vulkan device draws translucent
	// surfaces: a Deus Ex mover's (VisibleBrush), so that one flush with the
	// level's surfaces shows over them instead of fighting them in the
	// depth buffer
	bool DepthBias = false;
};

class OutputDevice
{
public:
	void Log(const std::string& text) { }
};

class RenderDevice
{
public:
	static std::unique_ptr<RenderDevice> Create(Widget* viewport, RenderAPI renderAPI);

	RenderDevice();
	virtual ~RenderDevice() = default;

	virtual void Flush(bool AllowPrecache) = 0;
	virtual bool Exec(std::string Cmd, OutputDevice& Ar) { return false; }
	virtual void Lock(vec4 FlashScale, vec4 FlashFog, vec4 ScreenClear) = 0;
	virtual void Unlock(bool Blit) = 0;
	virtual void DrawComplexSurface(SceneNode* Frame, SurfaceInfo& Surface, SurfaceFacet& Facet) = 0;
	virtual void DrawGouraudPolygon(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags) = 0;

	// NumTris triangles, three vertices each, all with one texture and one
	// set of flags: what that many DrawGouraudPolygon calls draw, which a
	// device may set up once. The texture's modified flag goes with the first.
	virtual void DrawGouraudTriangles(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumTris, uint32_t PolyFlags)
	{
		for (int i = 0; i < NumTris; i++)
		{
			DrawGouraudPolygon(Frame, Info, Pts + i * 3, 3, PolyFlags);
			Info.bRealtimeChanged = false;
		}
	}
	virtual void DrawTile(SceneNode* Frame, TextureInfo& Info, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 Color, vec4 Fog, uint32_t PolyFlags) = 0;

	// The flags a tile is drawn with, as D3DDrv's DrawTile changes them
	// (RenderDevice.cpp): each device's DrawTile takes them first, and a
	// sprite, drawn as a polygon, takes its own
	static uint32_t TileFlags(const TextureInfo& Info, uint32_t PolyFlags);

	virtual void Draw3DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2) = 0;
	virtual void Draw2DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2) = 0;
	virtual void ClearZ() = 0;
	// The last frame's pixels, as many as LastFrameSize gives: the scene the
	// device drew, which a window resized since no longer matches.
	virtual void ReadPixels(TextureColor* Pixels) = 0;
	virtual void LastFrameSize(int& width, int& height) const { width = GetRenderWidth(); height = GetRenderHeight(); }
	// A row of a ten-bit scene buffer, red in each pixel's low bits, made
	// bytes for ReadPixels (RenderDevice.cpp); y is the row's place in the
	// shot, presented whether the present pass drew it
	static void TenBitRowToBytes(const uint32_t* src, TextureColor* dest, int width, int y, bool presented);
	virtual void EndFlash() = 0;
	// The world's frame and the weapon's drawn: with Bloom on, its bloom
	// taken from them now, the HUD's and the menus' windows drawn after it
	// left sharp, and added to the frame whole at Unlock. A device without
	// a bloom draws on.
	virtual void TakeBloom() { }
	virtual void SetSceneNode(SceneNode* Frame) = 0;
	virtual void PrecacheTexture(TextureInfo& Info, uint32_t PolyFlags) = 0;
	virtual bool SupportsTextureFormat(TextureFormat Format) = 0;
	virtual void UpdateTextureRect(TextureInfo& Info, int U, int V, int UL, int VL) = 0;

	// A device that draws the scene offscreen and scales it to the window when
	// presenting can draw it smaller than the window (Settings.json
	// Performance.RenderScale). The game -- the viewport, the canvas, the UI --
	// then sees the render size, not the window's.
	virtual bool SupportsRenderScale() const { return false; }
	float GetRenderScale() const;
	int GetRenderWidth() const;
	int GetRenderHeight() const;

	bool ParseCommand(std::string* cmd, const std::string& keyword) { return false; }

	Widget* Viewport = nullptr;
	bool PrecacheOnFlip = false;
	float Brightness = 0.5f;
	// The display gamma a brightness gives: GammaScale x Brightness. Deus
	// Ex's D3DDrv makes it 2.5, so 0.4 is neutral (dx-reverse-info's
	// d3ddrv-dll.md, gamma).
	static constexpr float GammaScale = 2.5f;
	// Surreal Engine's darkClamp, 3.1/255 off every texture's colour in the scene
	// shader. Deus Ex goes without: its D3DDrv draws a texture's colours as
	// they are, in 32-bit textures on any display of 24 bits or more
	// (dx-reverse-info/d3ddrv-dll.md, textures).
	static constexpr bool DarkClamp = false;
	// A surface with a macro texture drawn as Deus Ex's D3DDrv draws it, in
	// its two passes: the macro in the modulated blend, twice its colour,
	// then the light map, twice as bright as on one pass
	// (dx-reverse-info/d3ddrv-dll.md, the light maps' brightness).
	static constexpr bool MacroTwoPass = true;
	// A world surface's detail texture drawn as D3DDrv's passes draw it, a
	// band each nearer than 380, 90 and 21.3 units, in the scene shader's one
	// pass (dx-reverse-info/d3ddrv-dll.md, detail textures).
	static constexpr bool DetailBands = true;

	// Settings
	bool UseVSync = true;
	float GammaOffset = 0.0f;
	float GammaOffsetRed = 0.0f;
	float GammaOffsetGreen = 0.0f;
	float GammaOffsetBlue = 0.0f;
	// The present's colour correction: 1 each the picture unchanged
	float Contrast = 1.0f;
	float Saturation = 1.0f;
	bool Hdr = false;
	uint8_t HdrScale = 128;
	bool Bloom = false;
	uint8_t BloomAmount = 128;
	// The textures' mip bias, by default the one D3DDrv's frames choose their
	// mips by, as Wine's Direct3D 7 draws them on RADV: fitted against
	// LaserConsole's receding floor (its RMSE 0.66 at -0.5, 0.88 and 1.05 a
	// quarter either way). At -0.5 the software device's mip is
	// floor(log2 of the scale factor), its texels no larger than the
	// screen's pixels, and the factor below exactly 1.
	float LODBias = -0.5f;
	uint8_t AntialiasMode = 2; // 4x multisample
	uint8_t GammaMode = 0;
	uint8_t LightMode = 0;
	bool GammaCorrectScreenshots = true;
	bool UseDebugLayer = false;
	bool CPUDriver = false; // Mesa's CPU driver asked for: lavapipe, llvmpipe
};

class RenderDeviceTexture : public CanvasTexture
{
public:
	TextureInfo Info;
	UnrealMipmap Mip;
};

class RenderDeviceCanvas : public Canvas
{
public:
	RenderDeviceCanvas(RenderDevice* device);

	void begin(const Colorf& color) override;
	void end() override;

	void begin3d() override;
	void end3d() override;

	RenderDevice* GetRenderDevice() { return device; }

protected:
	std::unique_ptr<CanvasTexture> createTexture(int width, int height, const void* pixels, ImageFormat format) override;
	void drawLineAntialiased(float x0, float y0, float x1, float y1, Colorf color) override;
	void fillTile(float x, float y, float width, float height, Colorf color) override;
	void drawTile(CanvasTexture* texture, float x, float y, float width, float height, float u, float v, float uvwidth, float uvheight, Colorf color) override;
	void drawGlyph(CanvasTexture* texture, float x, float y, float width, float height, float u, float v, float uvwidth, float uvheight, Colorf color) override;

private:
	RenderDevice* device = nullptr;
	SceneNode frame;
};
