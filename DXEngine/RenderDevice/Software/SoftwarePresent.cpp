
#include "Precomp.h"
#include "SoftwarePresent.h"
#include "RenderDevice/RenderDevice.h"
#include <algorithm>
#include <cmath>
#include <cstring>

bool SoftwarePresent::GammaKey::operator==(const GammaKey& other) const
{
	return Brightness == other.Brightness && Mode == other.Mode && std::memcmp(Offsets, other.Offsets, sizeof(Offsets)) == 0;
}

void SoftwarePresent::SetGamma(const RenderDevice& device)
{
	GammaKey key;
	key.Brightness = device.Brightness;
	key.Mode = device.GammaMode;
	key.Offsets[0] = device.GammaOffset;
	key.Offsets[1] = device.GammaOffsetRed;
	key.Offsets[2] = device.GammaOffsetGreen;
	key.Offsets[3] = device.GammaOffsetBlue;
	if (key == Key)
		return;
	Key = key;

	// The exponents and XOpenGL's brightness, as GetPresentPushConstants
	// makes them
	float exponent[3] = { 1.0f, 1.0f, 1.0f };
	float brighten = 1.0f;
	float brightness = std::clamp(key.Brightness * RenderDevice::GammaScale, 0.05f, 2.99f);
	for (int c = 0; c < 3; c++)
	{
		float offset = key.Offsets[0] + key.Offsets[1 + c];
		if (key.Mode == 1)
			exponent[c] = (offset + 2.0f) > 0.0f ? 1.0f / (offset + 1.0f) : 1.0f;
		else
			exponent[c] = 1.0f / std::max(brightness + offset, 0.001f);
	}
	if (key.Mode == 1)
		brighten = brightness;

	auto level = [](float v) { return (uint8_t)std::clamp((int)std::floor(v * 255.0f + 0.5f), 0, 255); };

	for (int c = 0; c < 3; c++)
	{
		for (int i = 0; i < 256; i++)
			Ramp[c][i] = level(std::pow(i / 255.0f, exponent[c]));
	}

	if (brighten != 1.0f)
	{
		// XOpenGL's gammaCorrect: above 1 each channel scaled by the largest
		// one's parabola, below it all of them scaled down; then the ramp
		for (int c = 0; c < 3; c++)
		{
			ByLargest[c].resize(256 * 256);
			for (int largest = 0; largest < 256; largest++)
			{
				float m = std::max(largest / 255.0f, 0.001f);
				float scale = brighten;
				if (brighten > 1.0f)
				{
					float cc = (1.0f - (2.0f * m - 1.0f) * (2.0f * m - 1.0f)) * 0.25f * (brighten - 1.0f);
					scale = (m + cc) / m;
				}
				for (int i = 0; i < 256; i++)
				{
					float v = std::clamp(i / 255.0f * scale, 0.0f, 1.0f);
					ByLargest[c][largest * 256 + i] = level(std::pow(v, exponent[c]));
				}
			}
		}
	}
	else
	{
		for (auto& table : ByLargest)
			table = {};
	}

	Identity = ByLargest[0].empty();
	for (int c = 0; c < 3 && Identity; c++)
	{
		for (int i = 0; i < 256 && Identity; i++)
			Identity = Ramp[c][i] == i;
	}
}

inline uint32_t SoftwarePresent::Map(uint32_t pixel) const
{
	uint32_t r = (pixel >> 16) & 0xff;
	uint32_t g = (pixel >> 8) & 0xff;
	uint32_t b = pixel & 0xff;
	if (!ByLargest[0].empty())
	{
		uint32_t largest = std::max(std::max(r, g), b) << 8;
		r = ByLargest[0][largest + r];
		g = ByLargest[1][largest + g];
		b = ByLargest[2][largest + b];
	}
	else
	{
		r = Ramp[0][r];
		g = Ramp[1][g];
		b = Ramp[2][b];
	}
	return 0xff000000 | (r << 16) | (g << 8) | b;
}

void SoftwarePresent::MapRow(const uint32_t* src, uint32_t* dest, int count) const
{
	if (Identity)
	{
		std::memcpy(dest, src, (size_t)count * sizeof(uint32_t));
		return;
	}
	for (int x = 0; x < count; x++)
		dest[x] = Map(src[x]);
}

bool SoftwarePresent::Prepare(const SoftwareFrame& frame, int windowWidth, int windowHeight)
{
	windowWidth = std::max(windowWidth, 1);
	windowHeight = std::max(windowHeight, 1);
	if (OutputWidth != windowWidth || OutputHeight != windowHeight)
	{
		Output.Resize((size_t)windowWidth * windowHeight);
		OutputWidth = windowWidth;
		OutputHeight = windowHeight;
	}
	Unscaled = windowWidth == frame.Width && windowHeight == frame.Height;
	if (Unscaled)
		return true;

	// The letterbox and the taps, as DrawPresentTexture places the frame and
	// the linear sampler reads it at each pixel's centre
	float scale = std::min(windowWidth / (float)frame.Width, windowHeight / (float)frame.Height);
	BoxWidth = std::max((int)std::round(frame.Width * scale), 1);
	BoxHeight = std::max((int)std::round(frame.Height * scale), 1);
	BoxX = (windowWidth - BoxWidth) / 2;
	BoxY = (windowHeight - BoxHeight) / 2;
	if (ScaleKey[0] != frame.Width || ScaleKey[1] != frame.Height || ScaleKey[2] != BoxWidth || ScaleKey[3] != BoxHeight)
	{
		ScaleKey[0] = frame.Width;
		ScaleKey[1] = frame.Height;
		ScaleKey[2] = BoxWidth;
		ScaleKey[3] = BoxHeight;
		auto taps = [](std::vector<Tap>& list, int count, int size)
		{
			list.resize(count);
			for (int i = 0; i < count; i++)
			{
				float t = (i + 0.5f) * size / (float)count - 0.5f;
				float first = std::floor(t);
				Tap& tap = list[i];
				tap.First = std::clamp((int)first, 0, size - 1);
				tap.Second = std::clamp((int)first + 1, 0, size - 1);
				tap.Weight = std::clamp((int)std::floor((t - first) * 256.0f + 0.5f), 0, 256);
			}
		};
		taps(Columns, BoxWidth, frame.Width);
		taps(Rows, BoxHeight, frame.Height);
	}
	return false;
}

void SoftwarePresent::MapTile(SoftwareFrame& frame, int tileX, int tileY)
{
	int x0 = tileX * SoftwareFrame::TileSize;
	int y0 = tileY * SoftwareFrame::TileSize;
	int width = std::min(SoftwareFrame::TileSize, frame.Width - x0);
	int y1 = std::min(y0 + SoftwareFrame::TileSize, frame.Height);
	for (int y = y0; y < y1; y++)
		MapRow(frame.ColorRow(y) + x0, Output.Data() + (size_t)y * OutputWidth + x0, width);
}

const uint32_t* SoftwarePresent::Present(SoftwareFrame& frame, SoftwareWorkers& workers, bool mapped)
{
	if (!mapped)
	{
		// The window's rows in bands, a band a worker's item
		const int band = 16;
		workers.Run((OutputHeight + band - 1) / band, [&](int item, int)
		{
			int y1 = std::min((item + 1) * band, OutputHeight);
			for (int y = item * band; y < y1; y++)
				PresentRow(frame, y);
		});
	}
	return Output.Data();
}

void SoftwarePresent::PresentRow(SoftwareFrame& frame, int y)
{
	uint32_t* dest = Output.Data() + (size_t)y * OutputWidth;
	if (Unscaled)
	{
		MapRow(frame.ColorRow(y), dest, frame.Width);
	}
	else if (y < BoxY || y >= BoxY + BoxHeight)
	{
		std::fill(dest, dest + OutputWidth, 0xff000000);
	}
	else
	{
		std::fill(dest, dest + BoxX, 0xff000000);
		ScaleRow(frame, y - BoxY, dest + BoxX, BoxWidth);
		std::fill(dest + BoxX + BoxWidth, dest + OutputWidth, 0xff000000);
	}
}

void SoftwarePresent::ScaleRow(SoftwareFrame& frame, int y, uint32_t* dest, int count)
{
	const Tap& row = Rows[y];
	const uint32_t* line0 = frame.ColorRow(row.First);
	const uint32_t* line1 = frame.ColorRow(row.Second);
	uint32_t wy1 = row.Weight;
	uint32_t wy0 = 256 - wy1;
	for (int x = 0; x < count; x++)
	{
		const Tap& column = Columns[x];
		uint32_t wx1 = column.Weight;
		uint32_t wx0 = 256 - wx1;
		uint32_t c00 = line0[column.First], c01 = line0[column.Second];
		uint32_t c10 = line1[column.First], c11 = line1[column.Second];
		uint32_t pixel = 0xff000000;
		for (int shift = 0; shift < 24; shift += 8)
		{
			uint32_t top = ((c00 >> shift) & 0xff) * wx0 + ((c01 >> shift) & 0xff) * wx1;
			uint32_t bottom = ((c10 >> shift) & 0xff) * wx0 + ((c11 >> shift) & 0xff) * wx1;
			pixel |= ((top * wy0 + bottom * wy1 + 32768) >> 16) << shift;
		}
		dest[x] = Identity ? pixel : Map(pixel);
	}
}

void SoftwarePresent::ReadPixels(SoftwareFrame& frame, uint8_t* dest, bool gamma) const
{
	for (int y = 0; y < frame.Height; y++)
	{
		uint32_t* line = (uint32_t*)(dest + (size_t)y * frame.Width * 4);
		if (gamma)
			MapRow(frame.ColorRow(y), line, frame.Width);
		else
			std::memcpy(line, frame.ColorRow(y), (size_t)frame.Width * 4);
	}
}
