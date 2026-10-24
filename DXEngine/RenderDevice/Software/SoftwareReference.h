#pragma once

#include "SoftwareCommands.h"
#include <cstdint>

// A quad's inputs, two by two pixels: lane 0 at (x, y), 1 at (x + 1, y), 2 at
// (x, y + 1), 3 at (x + 1, y + 1), each interpolated at its pixel's centre.
// Its lanes' differences are the derivatives a texture's mip is chosen by.
struct SoftwareQuad
{
	float U[4][4]; // [texture coordinate][lane]
	float V[4][4];
	float Color[4][4]; // [r, g, b, a][lane]
	float W[4]; // the view depth, 1 / gl_FragCoord.w
};

// The scene shader and the blends in float, a lane at a time, as
// FileResource.cpp's Scene.frag and the Vulkan device's blend states make
// them (with NO_DARKCLAMP, MACRO_TWO_PASS and DETAIL_BANDS): the oracle the
// fixed-point pipelines are proven against, and with DXE_SW_REFERENCE=1 the
// device's own shading.
namespace SoftwareReference
{
	// Each covered lane's colour, 0 to 1 a channel (mask: a bit a lane): the
	// lanes the alpha test keeps come back
	int Shade(const SoftwareDraw& draw, const SoftwareQuad& quad, int mask, float mipFactor, float out[4][4]);

	// A colour blended into the frame's pixel as the draw's blend state
	// blends it, rounded to the frame's bytes, its alpha whole
	uint32_t Blend(const SoftwareDraw& draw, const float src[4], uint32_t dest);
}
