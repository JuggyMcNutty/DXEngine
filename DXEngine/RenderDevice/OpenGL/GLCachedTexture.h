#pragma once

#include "RenderDevice/RenderDevice.h"

#include "GLHandles.h"

class GLCachedTexture
{
public:
	// STAT HARDWARE's: its format, bytes and the frame it was last set in
	TextureCacheStats::Entry Stat;
	std::shared_ptr<GLTexture2D> Texture;
	int RealtimeChangeCount = 0;
	// Colour 0 cleared in it, as the draw that made it was masked
	bool Masked = false;

	float UScale = 0.0f;
	float VScale = 0.0f;
	float PanX = 0.0f;
	float PanY = 0.0f;
	float UMult = 0.0f;
	float VMult = 0.0f;
};
