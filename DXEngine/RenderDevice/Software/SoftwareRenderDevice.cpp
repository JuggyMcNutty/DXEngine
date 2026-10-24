
#include "Precomp.h"
#include "SoftwareRenderDevice.h"
#include "SoftwareTextureUploader.h"
#include "SoftwareSampler.h"
#include "SoftwareSimd.h"
#include "Utils/Logger.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include <dxwidgets/core/widget.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

// A colour's bytes in the frame's order, 0xAARRGGBB, its alpha whole
static uint32_t FrameColor(vec4 color)
{
	auto level = [](float v) { return (uint32_t)std::clamp((int)std::floor(v * 255.0f + 0.5f), 0, 255); };
	return 0xff000000 | (level(color.r) << 16) | (level(color.g) << 8) | level(color.b);
}

SoftwareRenderDevice::SoftwareRenderDevice(Widget* InViewport)
{
	Viewport = InViewport;

	// Vsync where the window's present can wait for it: SDL's renderer
	Viewport->SetBitmapSwapInterval(UseVSync ? 1 : 0);

	State.ObjectToProjection = mat4::identity();

#if defined(SOFTWARE_SSE2)
	const char* simd = "SSE2";
#elif defined(SOFTWARE_NEON)
	const char* simd = "NEON";
#else
	const char* simd = "scalar";
#endif
	// The float shading in the pipelines' place, the oracle they are proven
	// against; the general pipeline alone, which the specialized ones match
	const char* reference = std::getenv("DXE_SW_REFERENCE");
	const char* general = std::getenv("DXE_SW_GENERAL");
	Reference = reference && std::string(reference) == "1";
	CheckReference = reference && std::string(reference) == "check";
	GeneralOnly = general && std::string(general) == "1";

	Workers = std::make_unique<SoftwareWorkers>(SoftwareWorkers::DefaultThreads());
	int threads = Workers->Threads();
	LogMessage("Software device: " + std::to_string(threads) + (threads == 1 ? " thread, " : " threads, ") + simd + (Reference ? ", the reference shading" : "") + (CheckReference ? ", the pipelines checked against the reference shading" : "") + (GeneralOnly ? ", the general pipeline alone" : ""));
}

SoftwareRenderDevice::~SoftwareRenderDevice()
{
}

void SoftwareRenderDevice::Flush(bool AllowPrecache)
{
	// What is recorded drawn first: its textures are about to go
	if (IsLocked)
		DrawRecorded(false);
	Textures.ClearCache();

	if (AllowPrecache && UsePrecache)
		PrecacheOnFlip = true;
}

void SoftwareRenderDevice::Lock(vec4 InFlashScale, vec4 InFlashFog, vec4 ScreenClear, uint8_t* InHitData, int* InHitSize)
{
	HitData = InHitData;
	HitSize = InHitSize;
	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	// The frame at the render size, its tiles cleared as they are drawn, as
	// the scene pass's start clears the GPU devices' buffers
	Frame.Resize(GetRenderWidth(), GetRenderHeight());
	ClearColor = FrameColor(ScreenClear);
	ClearPending = true;

	Commands.Reset();
	Commands.AddState(State);

	IsLocked = true;
}

void SoftwareRenderDevice::Unlock(bool Blit)
{
	if (Blit)
	{
		// The ramp and the window's size first: where the window is the
		// frame's size, each tile goes through the ramp as it is finished
		Present.SetGamma(*this);
		int width = std::max(Viewport->GetNativePixelWidth(), 1);
		int height = std::max(Viewport->GetNativePixelHeight(), 1);
		bool mapped = DrawRecorded(Present.Prepare(Frame, width, height));
		const uint32_t* pixels = Present.Present(Frame, *Workers, mapped);
		Viewport->PresentBitmap(width, height, pixels);
	}
	else
	{
		DrawRecorded(false);
	}

	// No hit testing: the game asks for none (DXEngine has no editor)
	if (HitData && HitSize)
		*HitSize = 0;
	HitData = nullptr;
	HitSize = nullptr;

	IsLocked = false;
}

bool SoftwareRenderDevice::DrawRecorded(bool mapTiles)
{
	Commands.CloseChunk();
	size_t chunks = Commands.Chunks.size();
	if (chunks == 0 && !ClearPending)
		return false;

	// The front end, a chunk at a time
	if (Bins.size() < chunks)
		Bins.resize(chunks);
	Workers->Run((int)chunks, [&](int i, int) { SoftwareSetup::SetUp(Commands, Commands.Chunks[i], Frame, Bins[i], GeneralOnly); });

	SoftwareRasterJob job;
	job.Commands = &Commands;
	job.Bins = Bins.data();
	job.ChunkCount = chunks;
	job.Clear = ClearPending;
	job.ClearColor = ClearColor;
	job.MipFactor = SoftwareSampler::MipFactor(SoftwareSampler::LODBias);
	job.Reference = Reference;

	// The tiles by what was binned to them, the most first, so that the
	// last ones handed out are short
	const int tiles = Frame.TilesX * Frame.TilesY;
	TileCost.assign(tiles, 0);
	for (size_t c = 0; c < chunks; c++)
	{
		const uint32_t* offsets = Bins[c].Offsets.data();
		for (int t = 0; t < tiles; t++)
			TileCost[t] += offsets[t + 1] - offsets[t];
	}
	TileOrder.resize(tiles);
	for (int t = 0; t < tiles; t++)
		TileOrder[t] = t;
	std::stable_sort(TileOrder.begin(), TileOrder.end(), [&](int a, int b) { return TileCost[a] > TileCost[b]; });

	if (CheckReference)
	{
		// The frame as it stands drawn on with the reference shading too,
		// from a copy, for the pipelines' pixels to be compared with
		CheckFrame.Resize(Frame.Width, Frame.Height);
		std::copy(Frame.Color.Data(), Frame.Color.Data() + Frame.Color.Size(), CheckFrame.Color.Data());
		std::copy(Frame.Depth.Data(), Frame.Depth.Data() + Frame.Depth.Size(), CheckFrame.Depth.Data());
		SoftwareRasterJob reference = job;
		reference.Reference = true;
		Workers->Run(tiles, [&](int i, int)
		{
			int t = TileOrder[i];
			SoftwareRasterizer::DrawTile(CheckFrame, reference, t % Frame.TilesX, t / Frame.TilesX);
		});
	}

	Workers->Run(tiles, [&](int i, int)
	{
		int t = TileOrder[i];
		int x = t % Frame.TilesX, y = t / Frame.TilesX;
		SoftwareRasterizer::DrawTile(Frame, job, x, y);
		if (mapTiles)
			Present.MapTile(Frame, x, y);
	});

	if (CheckReference)
		CompareWithReference();

	ClearPending = false;
	Commands.Reset();
	Commands.AddState(State);
	return mapTiles;
}

// The pipelines' frame against the reference's: the largest difference of
// a channel, and the pixels more than a level apart, logged every 60 frames
void SoftwareRenderDevice::CompareWithReference()
{
	for (int y = 0; y < Frame.Height; y++)
	{
		const uint32_t* a = Frame.ColorRow(y);
		const uint32_t* b = CheckFrame.ColorRow(y);
		for (int x = 0; x < Frame.Width; x++)
		{
			if (a[x] == b[x])
				continue;
			int largest = 0;
			for (int shift = 0; shift < 24; shift += 8)
				largest = std::max(largest, std::abs((int)((a[x] >> shift) & 0xff) - (int)((b[x] >> shift) & 0xff)));
			Check.Largest = std::max(Check.Largest, largest);
			if (largest > 1)
				Check.Over++;
		}
	}
	Check.Pixels += (uint64_t)Frame.Width * Frame.Height;
	if (++Check.Frames == 60)
	{
		LogMessage("Software device: the pipelines against the reference, 60 frames: at most " + std::to_string(Check.Largest) + " levels apart, " + std::to_string(Check.Over) + " pixels of " + std::to_string(Check.Pixels) + " more than one");
		Check = {};
	}
}

SoftwareDraw& SoftwareRenderDevice::AddDraw(SoftwareDrawType type, uint32_t vertexCount, SoftwareVertex*& vertices)
{
	return Commands.AddDraw(type, vertexCount, vertices);
}

// The blend, the depth's test and write, the alpha test and the colour's
// write, as the Vulkan device's pipeline for the flags has them
// (RenderPassManager::GetPipeline)
void SoftwareRenderDevice::SetBlend(SoftwareDraw& draw, uint32_t PolyFlags)
{
	if (PolyFlags == PF_SubpixelFont)
	{
		draw.Blend = SoftwareBlend::SubpixelFont;
		draw.ColorWrite = true;
		draw.DepthTest = false;
		draw.DepthWrite = false;
		draw.AlphaTest = true;
		return;
	}

	if (PolyFlags & PF_Translucent)
		draw.Blend = SoftwareBlend::Translucent;
	else if (PolyFlags & PF_Modulated)
		draw.Blend = SoftwareBlend::Modulated;
	else if (PolyFlags & PF_Highlighted)
		draw.Blend = SoftwareBlend::Highlighted;
	else
		draw.Blend = SoftwareBlend::Solid;
	draw.ColorWrite = !(PolyFlags & PF_Invisible);
	draw.DepthTest = true;
	draw.DepthWrite = (PolyFlags & PF_Occlude) != 0;
	draw.AlphaTest = (PolyFlags & PF_Masked) != 0;
}

void SoftwareRenderDevice::DrawComplexSurface(SceneNode* Frame, SurfaceInfo& Surface, SurfaceFacet& Facet)
{
	uint32_t PolyFlags = ApplyPrecedenceRules(Surface.PolyFlags);

	// The textures first, drawn or not: a light map's change, perhaps only a
	// part of it, is taken once
	SoftwareTexture* tex = Textures.GetTexture(Surface.Texture, !!(PolyFlags & PF_Masked));
	SoftwareTexture* lightmap = Textures.GetTexture(Surface.LightMap, false);
	SoftwareTexture* macrotex = Textures.GetTexture(Surface.MacroTexture, false);
	SoftwareTexture* detailtex = Textures.GetTexture(Surface.DetailTexture, false);
	SoftwareTexture* fogmap = (Surface.FogMap && Surface.FogMap->NumMips > 0 && !Surface.FogMap->Mips[0].Data.empty()) ? Textures.GetTexture(Surface.FogMap, false) : nullptr;

	if (Facet.VertexCount < 3)
		return;

	if (Surface.DetailTexture && Surface.FogMap) detailtex = nullptr;

	// Each texture's coordinates as the Vulkan device's DrawComplexSurface
	// lays them
	float UDot = dot(Facet.MapCoords.XAxis, Facet.MapCoords.Origin);
	float VDot = dot(Facet.MapCoords.YAxis, Facet.MapCoords.Origin);

	float UPan = tex ? UDot + Surface.Texture->Pan.x : 0.0f;
	float VPan = tex ? VDot + Surface.Texture->Pan.y : 0.0f;
	float UMult = tex ? GetUMult(*Surface.Texture) : 0.0f;
	float VMult = tex ? GetVMult(*Surface.Texture) : 0.0f;
	float LMUPan = lightmap ? UDot + Surface.LightMap->Pan.x - 0.5f * Surface.LightMap->UScale : 0.0f;
	float LMVPan = lightmap ? VDot + Surface.LightMap->Pan.y - 0.5f * Surface.LightMap->VScale : 0.0f;
	float LMUMult = lightmap ? GetUMult(*Surface.LightMap) : 0.0f;
	float LMVMult = lightmap ? GetVMult(*Surface.LightMap) : 0.0f;
	float MacroUPan = macrotex ? UDot + Surface.MacroTexture->Pan.x : 0.0f;
	float MacroVPan = macrotex ? VDot + Surface.MacroTexture->Pan.y : 0.0f;
	float MacroUMult = macrotex ? GetUMult(*Surface.MacroTexture) : 0.0f;
	float MacroVMult = macrotex ? GetVMult(*Surface.MacroTexture) : 0.0f;
	float DetailUPan = detailtex ? UDot + Surface.DetailTexture->Pan.x : 0.0f;
	float DetailVPan = detailtex ? VDot + Surface.DetailTexture->Pan.y : 0.0f;
	float DetailUMult = detailtex ? GetUMult(*Surface.DetailTexture) : 0.0f;
	float DetailVMult = detailtex ? GetVMult(*Surface.DetailTexture) : 0.0f;

	uint32_t flags = 0;
	if (lightmap) flags |= 1;
	if (macrotex) flags |= 2;
	if (detailtex && !fogmap) flags |= 4;
	if (fogmap) flags |= 8;

	if (LightMode == 1) flags |= 64;

	if (fogmap) // in the detail texture's place
	{
		detailtex = fogmap;
		DetailUPan = UDot + Surface.FogMap->Pan.x - 0.5f * Surface.FogMap->UScale;
		DetailVPan = VDot + Surface.FogMap->Pan.y - 0.5f * Surface.FogMap->VScale;
		DetailUMult = GetUMult(*Surface.FogMap);
		DetailVMult = GetVMult(*Surface.FogMap);
	}

	SoftwareVertex* v;
	SoftwareDraw& draw = AddDraw(SoftwareDrawType::Polygon, Facet.VertexCount, v);
	SetBlend(draw, PolyFlags);
	draw.DepthBias = Surface.DepthBias;
	draw.ShaderFlags = flags;
	draw.Texture = tex ? tex : Textures.GetWhite();
	draw.LightMap = lightmap;
	draw.MacroTexture = macrotex;
	draw.DetailTexture = detailtex;
	draw.NoSmooth = (PolyFlags & PF_NoSmooth) != 0;

	for (uint32_t i = 0; i < Facet.VertexCount; i++)
	{
		vec3 point = Facet.Vertices[i];
		float u = dot(Facet.MapCoords.XAxis, point);
		float t = dot(Facet.MapCoords.YAxis, point);
		v[i].Position = point;
		v[i].TexCoord[0] = vec2((u - UPan) * UMult, (t - VPan) * VMult);
		v[i].TexCoord[1] = vec2((u - LMUPan) * LMUMult, (t - LMVPan) * LMVMult);
		v[i].TexCoord[2] = vec2((u - MacroUPan) * MacroUMult, (t - MacroVPan) * MacroVMult);
		v[i].TexCoord[3] = vec2((u - DetailUPan) * DetailUMult, (t - DetailVPan) * DetailVMult);
		v[i].Color = vec4(1.0f);
	}
}

// A mesh's or a sprite's draw, its flags, texture and vertices, as the
// Vulkan device's DrawGouraudPolygon sets them: the light the vertex colour,
// white for a modulated draw; the vertex fog in the second and third
// coordinates
SoftwareDraw& SoftwareRenderDevice::AddGouraud(SoftwareDrawType type, SoftwareTexture* tex, TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags)
{
	float UMult = GetUMult(Info);
	float VMult = GetVMult(Info);
	uint32_t flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;
	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && LightMode == 2) flags |= 32;

	SoftwareVertex* v;
	SoftwareDraw& draw = AddDraw(type, NumPts, v);
	SetBlend(draw, PolyFlags);
	draw.ShaderFlags = flags;
	draw.Texture = tex ? tex : Textures.GetWhite();
	draw.NoSmooth = (PolyFlags & PF_NoSmooth) != 0;

	bool modulated = (PolyFlags & PF_Modulated) != 0;
	for (int i = 0; i < NumPts; i++)
	{
		const GouraudVertex* P = &Pts[i];
		v[i].Position = P->Point;
		v[i].TexCoord[0] = vec2(P->UV.x * UMult, P->UV.y * VMult);
		v[i].TexCoord[1] = vec2(P->Fog.x, P->Fog.y);
		v[i].TexCoord[2] = vec2(P->Fog.z, P->Fog.w);
		v[i].TexCoord[3] = vec2(0.0f);
		v[i].Color = modulated ? vec4(1.0f) : vec4(P->Light, 1.0f);
	}
	return draw;
}

void SoftwareRenderDevice::DrawGouraudPolygon(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags)
{
	if (NumPts < 3) return;

	PolyFlags = ApplyPrecedenceRules(PolyFlags);
	SoftwareTexture* tex = Textures.GetTexture(&Info, !!(PolyFlags & PF_Masked));
	AddGouraud(SoftwareDrawType::Polygon, tex, Info, Pts, NumPts, PolyFlags);
}

void SoftwareRenderDevice::DrawGouraudTriangles(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumTris, uint32_t PolyFlags)
{
	if (NumTris <= 0) return;

	// The texture once, then draws of a chunk's triangles at most
	PolyFlags = ApplyPrecedenceRules(PolyFlags);
	SoftwareTexture* tex = Textures.GetTexture(&Info, !!(PolyFlags & PF_Masked));
	for (int first = 0; first < NumTris; first += (int)SoftwareCommands::MaxTriangles)
	{
		int count = std::min(NumTris - first, (int)SoftwareCommands::MaxTriangles);
		AddGouraud(SoftwareDrawType::Triangles, tex, Info, Pts + first * 3, count * 3, PolyFlags);
	}
}

void SoftwareRenderDevice::DrawTile(SceneNode* Frame, TextureInfo& Info, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 Color, vec4 Fog, uint32_t PolyFlags)
{
	PolyFlags = ApplyPrecedenceRules(TileFlags(Info, PolyFlags));

	// The subpixel font's colour is its blend's constant, the tile white
	vec4 constant(0.0f);
	if (PolyFlags == PF_SubpixelFont)
	{
		constant = Color;
		Color = vec4(1.0f);
	}

	SoftwareTexture* tex = Textures.GetTexture(&Info, !!(PolyFlags & PF_Masked));
	float UMult = tex ? GetUMult(Info) : 0.0f;
	float VMult = tex ? GetVMult(Info) : 0.0f;
	float u0 = U * UMult;
	float v0 = V * VMult;
	float u1 = (U + UL) * UMult;
	float v1 = (V + VL) * VMult;
	bool clamp = (u0 >= 0.0f && u1 <= 1.00001f && v0 >= 0.0f && v1 <= 1.00001f);

	vec4 color = (PolyFlags & PF_Modulated) ? vec4(1.0f) : vec4(Color.x, Color.y, Color.z, 1.0f);

	SoftwareVertex* v;
	SoftwareDraw& draw = AddDraw(SoftwareDrawType::Tile, 4, v);
	SetBlend(draw, PolyFlags);
	draw.Texture = tex ? tex : Textures.GetWhite();
	draw.NoSmooth = (PolyFlags & PF_NoSmooth) != 0;
	draw.Clamp = clamp;
	draw.Constant = constant;

	// The corners about the frame's centre at the depth Z, by the current
	// node's projection, clockwise from the top left
	float RFX2 = State.RFX2;
	float RFY2 = State.RFY2;
	v[0].Position = vec3(RFX2 * Z * (X - Frame->FX2), RFY2 * Z * (Y - Frame->FY2), Z);
	v[1].Position = vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y - Frame->FY2), Z);
	v[2].Position = vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z);
	v[3].Position = vec3(RFX2 * Z * (X - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z);
	v[0].TexCoord[0] = vec2(u0, v0);
	v[1].TexCoord[0] = vec2(u1, v0);
	v[2].TexCoord[0] = vec2(u1, v1);
	v[3].TexCoord[0] = vec2(u0, v1);
	for (int i = 0; i < 4; i++)
		v[i].Color = color;
}

// The colour a line or a point is drawn in: under the present's gamma, so
// that it shows as given (the Vulkan device's ApplyInverseGamma)
vec4 SoftwareRenderDevice::ApplyInverseGamma(vec4 color)
{
	if (IsOrtho)
		return color;
	float brightness = std::clamp(Brightness * GammaScale, 0.05f, 2.99f);
	float gammaRed = std::max(brightness + GammaOffset + GammaOffsetRed, 0.001f);
	float gammaGreen = std::max(brightness + GammaOffset + GammaOffsetGreen, 0.001f);
	float gammaBlue = std::max(brightness + GammaOffset + GammaOffsetBlue, 0.001f);
	return vec4(std::pow(color.r, gammaRed), std::pow(color.g, gammaGreen), std::pow(color.b, gammaBlue), color.a);
}

// A line as the Vulkan device's line pipeline draws it: the premultiplied
// blend, depth tested and written, in front of the scene unless depth cued
void SoftwareRenderDevice::AddLine(vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
	bool occlude = !!(LineFlags & LINE_DepthCued);
	vec4 color = ApplyInverseGamma(vec4(Color.x, Color.y, Color.z, 1.0f));

	SoftwareVertex* v;
	SoftwareDraw& draw = AddDraw(SoftwareDrawType::Line, 2, v);
	draw.Blend = SoftwareBlend::Highlighted;
	draw.DepthWrite = true;
	draw.DepthMin = occlude ? 0.1f : 0.0f;
	draw.DepthMax = occlude ? 1.0f : 0.1f;
	draw.Texture = Textures.GetWhite();
	v[0].Position = P1;
	v[1].Position = P2;
	v[0].Color = color;
	v[1].Color = color;
}

void SoftwareRenderDevice::Draw3DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
	if (IsOrtho)
	{
		P1.x = (P1.x) / Frame->Zoom + Frame->FX2;
		P1.y = (P1.y) / Frame->Zoom + Frame->FY2;
		P1.z = 1;
		P2.x = (P2.x) / Frame->Zoom + Frame->FX2;
		P2.y = (P2.y) / Frame->Zoom + Frame->FY2;
		P2.z = 1;

		if (std::abs(P2.x - P1.x) + std::abs(P2.y - P1.y) >= 0.2f)
		{
			Draw2DLine(Frame, Color, LineFlags, P1, P2);
		}
		else if (IsOrthoLowDetail)
		{
			Draw2DPoint(Frame, Color, LINE_None, P1.x - 1, P1.y - 1, P1.x + 1, P1.y + 1, P1.z);
		}
	}
	else
	{
		AddLine(Color, LineFlags, P1, P2);
	}
}

void SoftwareRenderDevice::Draw2DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
	float RFX2 = State.RFX2;
	float RFY2 = State.RFY2;
	AddLine(Color, LineFlags,
		vec3(RFX2 * P1.z * (P1.x - Frame->FX2), RFY2 * P1.z * (P1.y - Frame->FY2), P1.z),
		vec3(RFX2 * P2.z * (P2.x - Frame->FX2), RFY2 * P2.z * (P2.y - Frame->FY2), P2.z));
}

void SoftwareRenderDevice::Draw2DPoint(SceneNode* Frame, vec4 Color, uint32_t LineFlags, float X1, float Y1, float X2, float Y2, float Z)
{
	// A square of the colour, as the Vulkan device's point pipeline draws it:
	// the premultiplied blend, depth written, in front of the scene unless
	// the point is depth cued
	bool occlude = !!(LineFlags & LINE_DepthCued);
	vec4 color = ApplyInverseGamma(vec4(Color.x, Color.y, Color.z, 1.0f));

	SoftwareVertex* v;
	SoftwareDraw& draw = AddDraw(SoftwareDrawType::Tile, 4, v);
	draw.Blend = SoftwareBlend::Highlighted;
	draw.DepthWrite = true;
	draw.DepthMin = occlude ? 0.1f : 0.0f;
	draw.DepthMax = occlude ? 1.0f : 0.1f;
	draw.Texture = Textures.GetWhite();

	float RFX2 = State.RFX2;
	float RFY2 = State.RFY2;
	v[0].Position = vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z);
	v[1].Position = vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z);
	v[2].Position = vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z);
	v[3].Position = vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z);
	for (int i = 0; i < 4; i++)
		v[i].Color = color;
}

void SoftwareRenderDevice::ClearZ()
{
	SoftwareVertex* v;
	AddDraw(SoftwareDrawType::ClearZ, 0, v);
}

void SoftwareRenderDevice::PushHit(const uint8_t* Data, int Count)
{
}

void SoftwareRenderDevice::PopHit(int Count, bool bForce)
{
}

void SoftwareRenderDevice::ReadPixels(TextureColor* Pixels)
{
	Present.SetGamma(*this);
	Present.ReadPixels(Frame, (uint8_t*)Pixels, GammaCorrectScreenshots);
}

void SoftwareRenderDevice::LastFrameSize(int& width, int& height) const
{
	if (Frame.Width > 0)
	{
		width = Frame.Width;
		height = Frame.Height;
	}
	else
	{
		RenderDevice::LastFrameSize(width, height);
	}
}

void SoftwareRenderDevice::EndFlash()
{
	// The Vulkan device's flash: a quad over the viewport in the fog's
	// colour, its alpha 1 - min(2 x FlashScale, 1), premultiplied, at the
	// scene's nearest depth, tested and not written
	if (FlashScale != vec4(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != vec4(0.0f, 0.0f, 0.0f, 0.0f))
	{
		SoftwareVertex* v;
		SoftwareDraw& draw = AddDraw(SoftwareDrawType::Flash, 0, v);
		draw.Blend = SoftwareBlend::Highlighted;
		draw.DepthWrite = false;
		draw.Constant = vec4(FlashFog.x, FlashFog.y, FlashFog.z, 1.0f - std::min(FlashScale.x * 2.0f, 1.0f));
	}
}

void SoftwareRenderDevice::SetSceneNode(SceneNode* Frame)
{
	float Aspect = Frame->FY / Frame->FX;
	float RProjZ = (float)std::tan(radians(Frame->FovAngle) * 0.5);
	State.RFX2 = 2.0f * RProjZ / Frame->FX;
	State.RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;
	State.ViewportX = Frame->XB;
	State.ViewportY = Frame->YB;
	State.ViewportWidth = Frame->X;
	State.ViewportHeight = Frame->Y;

	// As the Vulkan device's push constants: the projection, the half pixel
	// of a 3D frame's polygons, the node's transforms
	mat4 objectToProjection = PolygonShift(Frame) * mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	State.ObjectToProjection = objectToProjection * Frame->WorldToView * Frame->ObjectToWorld;

	// The near clip plane in object space: its distance from a point is the
	// vertex shader's gl_ClipDistance
	mat4 objectToView = Frame->WorldToView * Frame->ObjectToWorld;
	for (int i = 0; i < 4; i++)
	{
		float sum = 0.0f;
		for (int j = 0; j < 4; j++)
			sum += Frame->NearClip.v[j] * objectToView.matrix[i * 4 + j];
		State.ClipPlane.v[i] = sum;
	}

	if (IsLocked)
		Commands.AddState(State);
}

void SoftwareRenderDevice::PrecacheTexture(TextureInfo& Info, uint32_t PolyFlags)
{
	PolyFlags = ApplyPrecedenceRules(PolyFlags);
	Textures.GetTexture(&Info, !!(PolyFlags & PF_Masked));
}

bool SoftwareRenderDevice::SupportsTextureFormat(TextureFormat Format)
{
	return SoftwareTextureUploader::Supports(Format);
}

void SoftwareRenderDevice::UpdateTextureRect(TextureInfo& Info, int U, int V, int UL, int VL)
{
	Textures.UpdateTextureRect(&Info, U, V, UL, VL);
}
