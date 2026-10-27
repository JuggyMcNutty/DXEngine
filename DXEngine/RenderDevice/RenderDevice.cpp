
#include "Precomp.h"
#include "RenderDevice.h"
#include "LauncherSettings.h"
#include "Vulkan/VulkanRenderDevice.h"
#include "OpenGL/GLRenderDevice.h"
#include "Software/SoftwareRenderDevice.h"
#ifdef WIN32
#include "D3D11/D3D11RenderDevice.h"
#endif
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include <dxwidgets/core/colorf.h>
#include <dxwidgets/core/widget.h>

RenderDevice::RenderDevice()
{
	const auto& settings = LauncherSettings::Get();
	UseVSync = settings.RenderDevice.UseVSync;
	Hdr = settings.RenderDevice.Hdr;
	HdrScale = settings.RenderDevice.HdrScale;
	Bloom = settings.RenderDevice.Bloom;
	BloomAmount = settings.RenderDevice.BloomAmount;
	AntialiasMode = (int)settings.RenderDevice.Antialias;
	GammaMode = (int)settings.RenderDevice.Gamma;
	LightMode = (int)settings.RenderDevice.Light;
	GammaCorrectScreenshots = settings.RenderDevice.GammaCorrectScreenshots;
	UseDebugLayer = settings.RenderDevice.UseDebugLayer;
	CPUDriver = settings.RenderDevice.CPUDriver;
	// Saturation to 1 at most, so the software present's grey and colour mix
	// within the frame's range, as the GPU devices' do
	Contrast = std::clamp(settings.RenderDevice.Contrast, 0.1f, 4.0f);
	Saturation = std::clamp(settings.RenderDevice.Saturation, 0.0f, 1.0f);
	GammaOffset = std::clamp(settings.RenderDevice.GammaOffset, -1.0f, 1.0f);
	GammaOffsetRed = std::clamp(settings.RenderDevice.GammaOffsetRed, -1.0f, 1.0f);
	GammaOffsetGreen = std::clamp(settings.RenderDevice.GammaOffsetGreen, -1.0f, 1.0f);
	GammaOffsetBlue = std::clamp(settings.RenderDevice.GammaOffsetBlue, -1.0f, 1.0f);
	// Within the 2 every Vulkan device takes (maxSamplerLodBias)
	LODBias = std::clamp(settings.RenderDevice.LODBias, -2.0f, 2.0f);
}

// D3DDrv's DrawTile (D3DDrv.dll 0x10005ba0), which every tile goes through,
// a canvas's tile or text, a window's or a sprite: a texture whose palette
// has alpha, its entry 128's under 255 (the menu's TrueType font), is drawn
// PF_Highlighted, its alpha blending it, unless translucent; a modulated
// tile is drawn modulated and masked alone, every other flag dropped,
// PF_NoSmooth with them, so its colour 0 is cut out and the rest filtered,
// doubling what is behind it (in white: each device's DrawTile)
uint32_t RenderDevice::TileFlags(const TextureInfo& Info, uint32_t PolyFlags)
{
	if (PolyFlags == PF_SubpixelFont)
		return PolyFlags;
	if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & PF_Translucent))
		PolyFlags |= PF_Highlighted;
	if (PolyFlags & PF_Modulated)
		return PF_Modulated | PF_Masked;
	return PolyFlags;
}

// Ten bits a channel to a byte by a 4x4 ordered threshold, so that a region's
// mean is the buffer's: the driver's rounding to the nearest byte reads the
// darks a tenth of a level under, as 1023 is not four times 255 and below
// level 42 the step nearest each half-level lies under it (MeshConsole's pier
// 0.7% darker than with RGBA8 buffers). A gamma-corrected shot's levels were
// dithered to 8 bits by the present pass already: read back by rounding, not
// thresholded again, so the shot is the picture presented. Blue, green, red,
// alpha in each pixel's bytes, as the devices hand the engine its shots.
void RenderDevice::TenBitRowToBytes(const uint32_t* src, TextureColor* dest, int width, int y, bool presented)
{
	static const int bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
	for (int x = 0; x < width; x++)
	{
		float threshold = presented ? 0.5f : (bayer[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.0f;
		uint32_t c = src[x];
		dest[x].R = (uint8_t)(((c >> 20) & 1023) * (255.0f / 1023.0f) + threshold);
		dest[x].G = (uint8_t)(((c >> 10) & 1023) * (255.0f / 1023.0f) + threshold);
		dest[x].B = (uint8_t)((c & 1023) * (255.0f / 1023.0f) + threshold);
		dest[x].A = (uint8_t)((c >> 30) * 85);
	}
}

std::unique_ptr<RenderDevice> RenderDevice::Create(Widget* viewport, RenderAPI renderAPI)
{
	if (renderAPI == RenderAPI::Vulkan)
	{
		return std::make_unique<VulkanRenderDevice>(viewport);
	}
	else if (renderAPI == RenderAPI::OpenGL || renderAPI == RenderAPI::GLES)
	{
		// The GL device runs on both a desktop core context and an OpenGL ES
		// one (RenderAPI::GLES); it detects which it got at init.
		return std::make_unique<GLRenderDevice>(viewport);
	}
	else if (renderAPI == RenderAPI::Bitmap)
	{
		// The software device, which presents its frame as a bitmap
		return std::make_unique<SoftwareRenderDevice>(viewport);
	}
#ifdef WIN32
	else if (renderAPI == RenderAPI::D3D11)
	{
		return std::make_unique<D3D11RenderDevice>(viewport);
	}
#endif
	else
	{
		Exception::Throw("Render API not available on this platform");
	}
}

float RenderDevice::GetRenderScale() const
{
	if (!SupportsRenderScale())
		return 1.0f;
	float scale = LauncherSettings::Get().Performance.RenderScale;
	return std::isfinite(scale) ? std::clamp(scale, 0.25f, 1.0f) : 1.0f;
}

int RenderDevice::GetRenderWidth() const
{
	return std::max((int)std::lround(Viewport->GetNativePixelWidth() * GetRenderScale()), 1);
}

int RenderDevice::GetRenderHeight() const
{
	return std::max((int)std::lround(Viewport->GetNativePixelHeight() * GetRenderScale()), 1);
}

////////////////////////////////////////////////////////////////////////////

RenderDeviceCanvas::RenderDeviceCanvas(RenderDevice* device) : device(device)
{
}

void RenderDeviceCanvas::begin(const Colorf& color)
{
	device->Lock(vec4(0.0f), vec4(0.0f), vec4(color.r, color.g, color.b, color.a));

	frame.XB = 0;
	frame.YB = 0;
	frame.X = device->GetRenderWidth();
	frame.Y = device->GetRenderHeight();
	frame.FX = (float)device->GetRenderWidth();
	frame.FY = (float)device->GetRenderHeight();
	frame.FX2 = (float)device->GetRenderWidth() * 0.5f;
	frame.FY2 = (float)device->GetRenderHeight() * 0.5f;
	frame.Viewport = device->Viewport;
	frame.FovAngle = 90.0f;
	frame.ObjectToWorld = mat4::identity();
	frame.WorldToView = mat4::identity();
	float Aspect = frame.FY / frame.FX;
	float RProjZ = (float)std::tan(radians(frame.FovAngle) * 0.5f);
	float RFX2 = 2.0f * RProjZ / frame.FX;
	float RFY2 = 2.0f * RProjZ * Aspect / frame.FY;
	frame.Projection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	device->SetSceneNode(&frame);
}

void RenderDeviceCanvas::end()
{
	device->Unlock(true);
}

void RenderDeviceCanvas::begin3d()
{
	device->ClearZ();
}

void RenderDeviceCanvas::end3d()
{
	device->SetSceneNode(&frame);
	device->ClearZ();
}

std::unique_ptr<CanvasTexture> RenderDeviceCanvas::createTexture(int width, int height, const void* pixels, ImageFormat format)
{
	auto texture = std::make_unique<RenderDeviceTexture>();
	texture->Width = width;
	texture->Height = height;
	texture->Info.CacheID = (uint64_t)(ptrdiff_t)texture.get();
	texture->Info.Format = TextureFormat::BGRA8;
	texture->Info.USize = width;
	texture->Info.VSize = height;
	texture->Info.NumMips = 1;
	texture->Info.Mips = &texture->Mip;
	texture->Mip.Width = width;
	texture->Mip.Height = height;
	texture->Mip.Data.resize(width * height * 4);
	if (format == ImageFormat::B8G8R8A8)
	{
		memcpy(texture->Mip.Data.data(), pixels, width * height * 4);
	}
	else // Convert it to BGRA8
	{
		const uint32_t* src = (const uint32_t*)pixels;
		uint32_t* dest = (uint32_t*)texture->Mip.Data.data();
		int count = width * height;
		for (int i = 0; i < count; i++)
		{
			uint32_t a = (src[i] >> 24) & 0xff;
			uint32_t b = (src[i] >> 16) & 0xff;
			uint32_t g = (src[i] >> 8) & 0xff;
			uint32_t r = src[i] & 0xff;
			dest[i] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}
	return texture;
}

void RenderDeviceCanvas::drawLineAntialiased(float x0, float y0, float x1, float y1, Colorf color)
{
	device->Draw2DLine(&frame, vec4(color.r, color.g, color.b, color.a), 0, vec3(x0, y0, 1.0f), vec3(x1, y1, 1.0f));
}

void RenderDeviceCanvas::fillTile(float x, float y, float width, float height, Colorf color)
{
	device->DrawTile(&frame, static_cast<RenderDeviceTexture*>(whiteTexture.get())->Info, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, vec4(color.r, color.g, color.b, color.a), vec4(0.0f), PF_Highlighted);
}

void RenderDeviceCanvas::drawTile(CanvasTexture* texture, float x, float y, float width, float height, float u, float v, float uvwidth, float uvheight, Colorf color)
{
	device->DrawTile(&frame, static_cast<RenderDeviceTexture*>(texture)->Info, x, y, width, height, u, v, uvwidth, uvheight, 1.0f, vec4(color.r, color.g, color.b, color.a), vec4(0.0f), PF_Highlighted);
}

void RenderDeviceCanvas::drawGlyph(CanvasTexture* texture, float x, float y, float width, float height, float u, float v, float uvwidth, float uvheight, Colorf color)
{
	device->DrawTile(&frame, static_cast<RenderDeviceTexture*>(texture)->Info, x, y, width, height, u, v, uvwidth, uvheight, 1.0f, vec4(color.r, color.g, color.b, color.a), vec4(0.0f), PF_SubpixelFont);
}
