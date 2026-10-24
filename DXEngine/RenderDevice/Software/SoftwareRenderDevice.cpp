
#include "Precomp.h"
#include "SoftwareRenderDevice.h"
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
	if (AllowPrecache && UsePrecache)
		PrecacheOnFlip = true;
}

void SoftwareRenderDevice::Lock(vec4 InFlashScale, vec4 InFlashFog, vec4 ScreenClear, uint8_t* InHitData, int* InHitSize)
{
	HitData = InHitData;
	HitSize = InHitSize;
	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	// The frame at the render size, cleared as the scene pass's start clears
	// the GPU devices' buffers
	Frame.Resize(GetRenderWidth(), GetRenderHeight());
	Frame.Clear(FrameColor(ScreenClear), 1.0f);

	IsLocked = true;
}

void SoftwareRenderDevice::Unlock(bool Blit)
{
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
}

void SoftwareRenderDevice::Draw3DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
}

void SoftwareRenderDevice::Draw2DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
}

void SoftwareRenderDevice::Draw2DPoint(SceneNode* Frame, vec4 Color, uint32_t LineFlags, float X1, float Y1, float X2, float Y2, float Z)
{
}

void SoftwareRenderDevice::ClearZ()
{
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
}

void SoftwareRenderDevice::SetSceneNode(SceneNode* Frame)
{
}

void SoftwareRenderDevice::PrecacheTexture(TextureInfo& Info, uint32_t PolyFlags)
{
}

bool SoftwareRenderDevice::SupportsTextureFormat(TextureFormat Format)
{
	return true;
}

void SoftwareRenderDevice::UpdateTextureRect(TextureInfo& Info, int U, int V, int UL, int VL)
{
}
