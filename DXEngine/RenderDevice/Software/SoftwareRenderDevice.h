#pragma once

#include "RenderDevice/RenderDevice.h"
#include "SoftwareCommands.h"
#include "SoftwareRasterizer.h"
#include "SoftwarePresent.h"
#include "SoftwareSetup.h"
#include "SoftwareTextureManager.h"
#include "SoftwareWorkers.h"
#include <memory>

// The device that draws on the CPU, for a machine without a GPU's driver:
// the scene as the GPU devices draw it, sampled as D3DDrv samples it, into
// an 8-bit frame presented as a bitmap (docs/ENGINE.md, rendering). It
// records a frame's calls, and sets them up, bins them to tiles and draws
// the tiles at Unlock, on every core.
class SoftwareRenderDevice : public RenderDevice
{
public:
	SoftwareRenderDevice(Widget* viewport);
	~SoftwareRenderDevice();

	void Flush(bool AllowPrecache) override;
	void Lock(vec4 FlashScale, vec4 FlashFog, vec4 ScreenClear) override;
	void Unlock(bool Blit) override;
	void DrawComplexSurface(SceneNode* Frame, SurfaceInfo& Surface, SurfaceFacet& Facet) override;
	void DrawGouraudPolygon(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags) override;
	void DrawGouraudTriangles(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumTris, uint32_t PolyFlags) override;
	void DrawTile(SceneNode* Frame, TextureInfo& Info, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 Color, vec4 Fog, uint32_t PolyFlags) override;
	void Draw3DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2) override;
	void Draw2DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2) override;
	void ClearZ() override;
	void ReadPixels(TextureColor* Pixels) override;
	void LastFrameSize(int& width, int& height) const override;
	void EndFlash() override;
	void SetSceneNode(SceneNode* Frame) override;
	void PrecacheTexture(TextureInfo& Info, uint32_t PolyFlags) override;
	bool SupportsTextureFormat(TextureFormat Format) override;
	void UpdateTextureRect(TextureInfo& Info, int U, int V, int UL, int VL) override;
	bool SupportsRenderScale() const override { return true; }

private:
	// A run of chunks drawn together into every tile: the frame's clear
	// first if it has not had it; each tile, once drawn, mapped into the
	// window's pixels (SoftwarePresent::MapTile) with mapTiles
	struct Wave
	{
		uint32_t FirstChunk = 0;
		std::vector<SoftwareChunk> Chunks;
		bool Clear = false;
		bool MapTiles = false;
	};

	// What is recorded and not yet drawn, drawn: the wave the workers may
	// be drawing waited for, then the rest on every thread; true where the
	// tiles were mapped. Nothing recorded is left undrawn after it.
	bool DrawRecorded(bool mapTiles);
	// The chunks closed since the last wave, handed to the workers to set up
	// and draw while the game's thread records what follows
	void LaunchWave();
	// A wave's chunks set up and drawn: callerShares, the calling thread
	// takes its share (the game's); else it waits (the background thread)
	void SetUpWave(const Wave& wave, bool callerShares);
	void DrawWave(const Wave& wave, SoftwareFrame& frame, bool reference, bool callerShares);
	void EnsureBins(uint32_t chunks);
	void CompareWithReference();

	// A draw's textures named as read until the next DrawRecorded; a light
	// map's by the rectangle its coordinates reach
	void Name(SoftwareTexture* tex);
	void NameRect(SoftwareTexture* tex, const SoftwareVertex* vertices, uint32_t count, int coord);
	void BeforeTextureWrite(SoftwareTexture* tex, int x, int y, int w, int h);

	SoftwareDraw& AddDraw(SoftwareDrawType type, uint32_t vertexCount, SoftwareVertex*& vertices);
	SoftwareDraw& AddGouraud(SoftwareDrawType type, SoftwareTexture* tex, TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags);
	void AddLine(vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2);
	vec4 ApplyInverseGamma(vec4 color);
	static void SetBlend(SoftwareDraw& draw, uint32_t PolyFlags);

	SoftwareFrame Frame;
	SoftwarePresent Present;
	SoftwareTextureManager Textures;
	SoftwareCommands Commands;
	std::vector<std::unique_ptr<SoftwareBins>> Bins; // a chunk's each, where they stay
	std::vector<const SoftwareBins*> BinPointers;

	// The threads a frame's drawing is shared among, the game's with them,
	// and the tiles in the order they are handed out: the most binned first
	std::unique_ptr<SoftwareWorkers> Workers;
	std::vector<int> TileOrder;
	std::vector<uint32_t> TileCost;

	// Waves drawn while the game's thread records (with three threads or
	// more, not with the reference shading): the chunks handed out so far,
	// the wave the workers have
	bool Streaming = false;
	uint32_t ChunksLaunched = 0;
	Wave Background;

	// What the draws not yet drawn read, named in this epoch
	// (SoftwareTexture::NamedEpoch); each DrawRecorded begins another
	uint64_t NameEpoch = 1;

	// The current scene node's, kept from frame to frame: a draw before a
	// frame's first SetSceneNode takes the last one
	SoftwareState State;

	bool Reference = false;
	bool GeneralOnly = false;

	// DXE_SW_REFERENCE=check: each frame drawn with the reference shading
	// too, into its own frame, and the two compared
	bool CheckReference = false;
	SoftwareFrame CheckFrame;
	struct
	{
		int Frames = 0;
		int Largest = 0;
		uint64_t Over = 0;
		uint64_t Pixels = 0;
	} Check;
	bool IsLocked = false;
	bool ClearPending = false;
	uint32_t ClearColor = 0xff000000;
	bool UsePrecache = true;
	vec4 FlashScale = vec4(0.5f, 0.5f, 0.5f, 0.0f);
	vec4 FlashFog = vec4(0.0f);
};
