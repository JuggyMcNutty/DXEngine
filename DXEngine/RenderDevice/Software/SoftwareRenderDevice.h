#pragma once

#include "RenderDevice/RenderDevice.h"
#include "SoftwareCommands.h"
#include "SoftwareRasterizer.h"
#include "SoftwarePresent.h"
#include "SoftwareSetup.h"
#include "SoftwareTextureManager.h"

// The device that draws on the CPU, for a machine without a GPU's driver:
// the scene as the GPU devices draw it, sampled as D3DDrv samples it, into
// an 8-bit frame presented as a bitmap (docs/ENGINE.md, rendering). It
// records a frame's calls, and sets them up, bins them to tiles and draws
// the tiles at Unlock.
class SoftwareRenderDevice : public RenderDevice
{
public:
	SoftwareRenderDevice(Widget* viewport);
	~SoftwareRenderDevice();

	void Flush(bool AllowPrecache) override;
	void Lock(vec4 FlashScale, vec4 FlashFog, vec4 ScreenClear, uint8_t* HitData, int* HitSize) override;
	void Unlock(bool Blit) override;
	void DrawComplexSurface(SceneNode* Frame, SurfaceInfo& Surface, SurfaceFacet& Facet) override;
	void DrawGouraudPolygon(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags) override;
	void DrawGouraudTriangles(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumTris, uint32_t PolyFlags) override;
	void DrawTile(SceneNode* Frame, TextureInfo& Info, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 Color, vec4 Fog, uint32_t PolyFlags) override;
	void Draw3DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2) override;
	void Draw2DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2) override;
	void Draw2DPoint(SceneNode* Frame, vec4 Color, uint32_t LineFlags, float X1, float Y1, float X2, float Y2, float Z) override;
	void ClearZ() override;
	void PushHit(const uint8_t* Data, int Count) override;
	void PopHit(int Count, bool bForce) override;
	void ReadPixels(TextureColor* Pixels) override;
	void LastFrameSize(int& width, int& height) const override;
	void EndFlash() override;
	void SetSceneNode(SceneNode* Frame) override;
	void PrecacheTexture(TextureInfo& Info, uint32_t PolyFlags) override;
	bool SupportsTextureFormat(TextureFormat Format) override;
	void UpdateTextureRect(TextureInfo& Info, int U, int V, int UL, int VL) override;
	bool SupportsRenderScale() const override { return true; }

private:
	// What is recorded, drawn into the frame: the clear first if the frame
	// has not had it
	void DrawRecorded();

	SoftwareDraw& AddDraw(SoftwareDrawType type, uint32_t vertexCount, SoftwareVertex*& vertices);
	SoftwareDraw& AddGouraud(SoftwareDrawType type, SoftwareTexture* tex, TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags);
	void AddLine(vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2);
	vec4 ApplyInverseGamma(vec4 color);
	static void SetBlend(SoftwareDraw& draw, uint32_t PolyFlags);

	SoftwareFrame Frame;
	SoftwarePresent Present;
	SoftwareTextureManager Textures;
	SoftwareCommands Commands;
	std::vector<SoftwareBins> Bins;

	// The current scene node's, kept from frame to frame: a draw before a
	// frame's first SetSceneNode takes the last one
	SoftwareState State;

	bool IsLocked = false;
	bool ClearPending = false;
	uint32_t ClearColor = 0xff000000;
	bool UsePrecache = true;
	vec4 FlashScale = vec4(0.5f, 0.5f, 0.5f, 0.0f);
	vec4 FlashFog = vec4(0.0f);
	uint8_t* HitData = nullptr;
	int* HitSize = nullptr;
};
