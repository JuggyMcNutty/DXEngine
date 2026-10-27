#pragma once

#include "SoftwareRasterizer.h"
#include "SoftwareWorkers.h"
#include <vector>

class RenderDevice;

// The frame's way to the window: the gamma ramp, the scale from the render
// size to the window's, and the frame read back for a shot.
class SoftwarePresent
{
public:
	// The ramp of the present shader's gamma (Vulkan's Present.frag) for an
	// 8-bit frame, as D3DDrv's display ramp maps its frame: in D3D9's mode
	// (i/255)^(1/gamma) a channel, in XOpenGL's the pixel brightened by its
	// largest channel first. Made again only when the device's settings
	// change.
	void SetGamma(const RenderDevice& device);

	// The window's size for the frame about to be drawn: true where it is
	// the frame's, each tile then mapping its own pixels (MapTile) as the
	// last step of its drawing.
	bool Prepare(const SoftwareFrame& frame, int windowWidth, int windowHeight);

	// A tile's pixels through the ramp into the window's
	void MapTile(SoftwareFrame& frame, int tileX, int tileY);

	// The frame in the window's pixels, the ramp applied: as it is when the
	// window is the frame's size, else scaled up and letterboxed as the
	// Vulkan device's DrawPresentTexture scales it, bilinear and held at
	// the edges. The rows shared out among the workers, unless each tile
	// mapped its own (mapped).
	const uint32_t* Present(SoftwareFrame& frame, SoftwareWorkers& workers, bool mapped);

	// The frame's pixels, top row first, as B, G, R, A bytes, the ramp
	// applied with GammaCorrectScreenshots.
	void ReadPixels(SoftwareFrame& frame, uint8_t* dest, bool gamma) const;

private:
	uint32_t Map(uint32_t pixel) const;
	void MapRow(const uint32_t* src, uint32_t* dest, int count) const;
	void ScaleRow(SoftwareFrame& frame, int y, uint32_t* dest, int count);
	void PresentRow(SoftwareFrame& frame, int y);

	struct GammaKey
	{
		float Brightness = -1.0f;
		int Mode = -1;
		float Contrast = -1.0f;
		float Saturation = -1.0f;
		float Offsets[4] = {};
		bool operator==(const GammaKey& other) const;
	} Key;

	bool Identity = true;
	// The saturation's mix, its weight in 256ths, where it is not 1
	bool Saturate = false;
	int SaturationQ = 256;
	uint8_t Ramp[3][256] = {};
	// XOpenGL's: each channel's level by the pixel's largest channel and its
	// own, [largest][channel], one table a channel
	std::vector<uint8_t> ByLargest[3];

	SoftwareBuffer<uint32_t> Output;
	int OutputWidth = 0;
	int OutputHeight = 0;
	bool Unscaled = true; // the window the frame's size

	// The letterbox the frame is scaled into, below a render scale of 1
	int BoxX = 0;
	int BoxY = 0;
	int BoxWidth = 0;
	int BoxHeight = 0;

	// The upscale's columns and rows: the two texels each output pixel
	// reads and the second's weight, 0 to 256
	struct Tap
	{
		int First = 0;
		int Second = 0;
		int Weight = 0;
	};
	std::vector<Tap> Columns;
	std::vector<Tap> Rows;
	int ScaleKey[4] = {};
};
