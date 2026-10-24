#pragma once

#include <cstdint>
#include <cstddef>
#include <new>

// A buffer of the frame's, aligned to a cache line: what each tile reads and
// writes, and the SIMD loads, start on one.
template<typename T>
class SoftwareBuffer
{
public:
	SoftwareBuffer() = default;
	SoftwareBuffer(const SoftwareBuffer&) = delete;
	SoftwareBuffer& operator=(const SoftwareBuffer&) = delete;
	~SoftwareBuffer() { Free(); }

	void Resize(size_t count)
	{
		if (count == Count)
			return;
		Free();
		if (count > 0)
			Ptr = static_cast<T*>(::operator new(count * sizeof(T), std::align_val_t(64)));
		Count = count;
	}

	T* Data() { return Ptr; }
	const T* Data() const { return Ptr; }
	size_t Size() const { return Count; }

private:
	void Free()
	{
		if (Ptr)
			::operator delete(Ptr, std::align_val_t(64));
		Ptr = nullptr;
		Count = 0;
	}

	T* Ptr = nullptr;
	size_t Count = 0;
};

// The frame the device draws into, at the render size: BGRA8, as D3DDrv's
// 8-bit frame, each pixel 0xAARRGGBB (the windows' bitmap order), its alpha
// always 0xFF; and a float depth buffer, cleared to 1, tested LEQUAL. Both
// padded to whole tiles, so a tile never reads or writes outside them.
struct SoftwareFrame
{
	static constexpr int TileSize = 64;

	int Width = 0;
	int Height = 0;
	int Pitch = 0; // pixels a row, a whole number of tiles
	int TilesX = 0;
	int TilesY = 0;

	SoftwareBuffer<uint32_t> Color;
	SoftwareBuffer<float> Depth;

	void Resize(int width, int height);
	void Clear(uint32_t color, float depth);

	uint32_t* ColorRow(int y) { return Color.Data() + (size_t)y * Pitch; }
	float* DepthRow(int y) { return Depth.Data() + (size_t)y * Pitch; }
};

struct SoftwareBins;
class SoftwareCommands;

// What a tile's raster reads: the frame's draws, each chunk's bins, and
// whether the frame's clear comes first
struct SoftwareRasterJob
{
	const SoftwareCommands* Commands = nullptr;
	const SoftwareBins* Bins = nullptr; // one a chunk
	size_t ChunkCount = 0;
	bool Clear = false;
	uint32_t ClearColor = 0xff000000;
	float LODBias = 0.0f;
};

namespace SoftwareRasterizer
{
	// One tile of the frame: the clear, then each chunk's primitives binned
	// to the tile, in turn
	void DrawTile(SoftwareFrame& frame, const SoftwareRasterJob& job, int tileX, int tileY);
}
