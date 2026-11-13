#pragma once

#include "RenderDevice/RenderDevice.h"

#include <d3d11.h>

class D3D11CachedTexture
{
public:
	// STAT HARDWARE's: its format, bytes and the frame it was last set in
	TextureCacheStats::Entry Stat;
	ComPtr<ID3D11Texture2D> Texture;
	ComPtr<ID3D11ShaderResourceView> View;
	int RealtimeChangeCount = 0;
	// Colour 0 cleared in it, as the draw that made it was masked
	bool Masked = false;
	int DummyMipmapCount = 0;

	float UScale = 0.0f;
	float VScale = 0.0f;
	float PanX = 0.0f;
	float PanY = 0.0f;
	float UMult = 0.0f;
	float VMult = 0.0f;
};
