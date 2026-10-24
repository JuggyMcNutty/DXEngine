#pragma once

#include "SoftwareCommands.h"
#include "SoftwareRasterizer.h"
#include <vector>

struct SoftwarePrimitive;
struct SoftwareRasterJob;
typedef void (*SoftwarePipeline)(SoftwareFrame& frame, const SoftwareRasterJob& job, const SoftwareDraw& draw, const SoftwarePrimitive& prim, int x0, int y0, int x1, int y1);

enum class SoftwarePrimitiveType : uint8_t
{
	Triangle,
	Rectangle, // a tile square on the screen: no edges to test
	Line,
	ClearZ,
	Flash
};

// An attribute over the screen: its value at the primitive's reference
// point, and its steps a pixel across and a pixel down
struct SoftwarePlane
{
	float Value = 0.0f;
	float Dx = 0.0f;
	float Dy = 0.0f;
};

// A primitive set up for the raster: the pixels it may cover, within the
// scissor (the frame's viewport), and its attributes as planes over the
// pixel centres, from the vertices as they were snapped
struct SoftwarePrimitive
{
	static constexpr int NumAttributes = 12; // u and v of the four coordinates; r, g, b, a

	SoftwarePrimitiveType Type = SoftwarePrimitiveType::Triangle;
	uint32_t Draw = 0;
	SoftwarePipeline Pipeline = nullptr; // a triangle's or a rectangle's
	int MinX = 0, MinY = 0, MaxX = 0, MaxY = 0; // [Min, Max)
	float RefX = 0.0f, RefY = 0.0f; // the planes' origin, on the screen
	SoftwarePlane Depth;  // the window's depth, linear on the screen
	SoftwarePlane InvW;   // 1/w
	SoftwarePlane Attributes[NumAttributes]; // each over w

	// A triangle's edges, each A x + B y + C at a pixel's whole coordinates,
	// not negative where the pixel's centre is covered: the edge functions
	// of its corners snapped to 1/256 pixel, the top-left rule's tie folded
	// into C
	int64_t EdgeA[3] = {};
	int64_t EdgeB[3] = {};
	int64_t EdgeC[3] = {};

	// A line's ends on the screen, and their depths
	float LineX[2] = {};
	float LineY[2] = {};
	float LineZ[2] = {};
};

// One chunk's primitives, and their indices tile by tile in the order they
// came: a tile draws each chunk's in turn
struct SoftwareBins
{
	std::vector<SoftwarePrimitive> Primitives;
	std::vector<uint32_t> Offsets; // a tile's: Entries[Offsets[tile]] to Entries[Offsets[tile + 1]]
	std::vector<uint32_t> Entries;
	std::vector<uint64_t> Found;   // tile << 32 | primitive, as binned
	std::vector<uint32_t> Cursor;

	void Reset();
	void Add(int tile, uint32_t primitive) { Found.push_back(((uint64_t)tile << 32) | primitive); }
	void Finish(int tileCount);
};

// The front end: a chunk's draws transformed, clipped, mapped to the
// viewport, snapped, set up and binned to the frame's tiles
namespace SoftwareSetup
{
	// generalOnly: every triangle and rectangle drawn by the general
	// pipeline (DXE_SW_GENERAL=1)
	void SetUp(const SoftwareCommands& commands, const SoftwareChunk& chunk, const SoftwareFrame& frame, SoftwareBins& bins, bool generalOnly);
}
