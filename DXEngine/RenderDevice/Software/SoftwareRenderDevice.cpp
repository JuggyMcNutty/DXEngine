
#include "Precomp.h"
#include "SoftwareRenderDevice.h"
#include "SoftwareTextureUploader.h"
#include "Utils/Logger.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include <dxwidgets/core/widget.h>
#include <algorithm>
#include <cmath>

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

#if defined(USE_SSE2)
	const char* simd = "SSE2";
#elif defined(USE_NEON)
	const char* simd = "NEON";
#else
	const char* simd = "scalar";
#endif
	LogMessage(std::string("Software device: 1 thread, ") + simd);
}

SoftwareRenderDevice::~SoftwareRenderDevice()
{
}

void SoftwareRenderDevice::Flush(bool AllowPrecache)
{
	// What is recorded drawn first: its textures are about to go
	if (IsLocked)
		DrawRecorded();
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
	DrawRecorded();

	if (Blit)
	{
		Present.SetGamma(*this);
		int width = Viewport->GetNativePixelWidth();
		int height = Viewport->GetNativePixelHeight();
		const uint32_t* pixels = Present.Present(Frame, width, height);
		Viewport->PresentBitmap(std::max(width, 1), std::max(height, 1), pixels);
	}

	// No hit testing: the game asks for none (DXEngine has no editor)
	if (HitData && HitSize)
		*HitSize = 0;
	HitData = nullptr;
	HitSize = nullptr;

	IsLocked = false;
}

void SoftwareRenderDevice::DrawRecorded()
{
	Commands.CloseChunk();
	size_t chunks = Commands.Chunks.size();
	if (chunks == 0 && !ClearPending)
		return;

	if (Bins.size() < chunks)
		Bins.resize(chunks);
	for (size_t i = 0; i < chunks; i++)
		SoftwareSetup::SetUp(Commands, Commands.Chunks[i], Frame, Bins[i]);

	SoftwareRasterJob job;
	job.Commands = &Commands;
	job.Bins = Bins.data();
	job.ChunkCount = chunks;
	job.Clear = ClearPending;
	job.ClearColor = ClearColor;
	job.LODBias = 0.0f;
	for (int y = 0; y < Frame.TilesY; y++)
	{
		for (int x = 0; x < Frame.TilesX; x++)
			SoftwareRasterizer::DrawTile(Frame, job, x, y);
	}

	ClearPending = false;
	Commands.Reset();
	Commands.AddState(State);
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
}

void SoftwareRenderDevice::DrawGouraudPolygon(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags)
{
}

void SoftwareRenderDevice::DrawGouraudTriangles(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumTris, uint32_t PolyFlags)
{
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

void SoftwareRenderDevice::Draw3DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
}

void SoftwareRenderDevice::Draw2DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
}

void SoftwareRenderDevice::Draw2DPoint(SceneNode* Frame, vec4 Color, uint32_t LineFlags, float X1, float Y1, float X2, float Y2, float Z)
{
	// A square of the colour, as the Vulkan device's point pipeline draws it:
	// the premultiplied blend, depth written, in front of the scene unless
	// the point is depth cued
	bool occlude = !!(LineFlags & LINE_DepthCued);
	vec4 color(Color.x, Color.y, Color.z, 1.0f);
	if (!IsOrtho)
	{
		float brightness = std::clamp(Brightness * GammaScale, 0.05f, 2.99f);
		color.x = std::pow(color.x, std::max(brightness + GammaOffset + GammaOffsetRed, 0.001f));
		color.y = std::pow(color.y, std::max(brightness + GammaOffset + GammaOffsetGreen, 0.001f));
		color.z = std::pow(color.z, std::max(brightness + GammaOffset + GammaOffsetBlue, 0.001f));
	}

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
