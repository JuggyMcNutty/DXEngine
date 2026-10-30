#pragma once

#include "GLHandles.h"

class GLCachedTexture
{
public:
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
