
#include "core/image.h"
#include "core/resourcedata.h"
#include "picopng/picopng.h"
#include "nanosvg/nanosvg.h"
#include "nanosvg/nanosvgrast.h"
#include <cstring>
#include <stdexcept>
#include <cmath>

class ImageImpl : public Image
{
public:
	ImageImpl(int width, int height, ImageFormat format, const void* data) : Width(width), Height(height), Format(format)
	{
		Data = std::make_unique<uint32_t[]>((size_t)width * height);
		if (data)
			memcpy(Data.get(), data, (size_t)width * height * sizeof(uint32_t));
	}

	int GetWidth() const override
	{
		return Width;
	}

	int GetHeight() const override
	{
		return Height;
	}

	ImageFormat GetFormat() const override
	{
		return Format;
	}

	void* GetData() const override
	{
		return Data.get();
	}

	int Width = 0;
	int Height = 0;
	ImageFormat Format = {};
	std::unique_ptr<uint32_t[]> Data;
};

std::shared_ptr<Image> Image::Create(int width, int height, ImageFormat format, const void* data)
{
	return std::make_shared<ImageImpl>(width, height, format, data);
}

std::shared_ptr<Image> Image::LoadResource(const std::string& resourcename, double dpiscale)
{
	size_t extensionpos = resourcename.find_last_of("./\\");
	if (extensionpos == std::string::npos || resourcename[extensionpos] != '.')
		throw std::runtime_error("Unsupported image format");
	std::string extension = resourcename.substr(extensionpos + 1);
	for (char& c : extension)
	{
		if (c >= 'A' && c <= 'Z')
			c = c - 'A' + 'a';
	}

	if (extension == "png")
	{
		auto filedata = ResourceData::ReadAllBytes(resourcename);

		std::vector<unsigned char> pixels;
		unsigned long width = 0, height = 0;
		int result = decodePNG(pixels, width, height, (const unsigned char*)filedata.data(), filedata.size(), true);
		if (result != 0)
			throw std::runtime_error("Could not decode PNG file");
		if (width == 0 || height == 0 || width > 16384 || height > 16384)
			throw std::runtime_error("PNG size out of range");

		return Image::Create((int)width, (int)height, ImageFormat::R8G8B8A8, pixels.data());
	}
	else if (extension == "svg")
	{
		auto filedata = ResourceData::ReadAllBytes(resourcename);
		filedata.push_back(0);

		NSVGimage* svgimage = nsvgParse((char*)filedata.data(), "px", (float)(96.0 * dpiscale));
		if (!svgimage)
			throw std::runtime_error("Could not parse SVG file");

		try
		{
			// Held to a size the allocation holds: the float's cast was
			// undefined out of range, and the width by height overflowed an
			// int (a 65536 x 65536 SVG allocated nothing and was drawn into)
			double w = std::floor(svgimage->width * dpiscale);
			double h = std::floor(svgimage->height * dpiscale);
			if (!(w >= 1.0 && w <= 16384.0 && h >= 1.0 && h <= 16384.0))
				throw std::runtime_error("SVG size out of range");
			int width = (int)w;
			int height = (int)h;
			std::shared_ptr<Image> image = Image::Create(width, height, ImageFormat::R8G8B8A8, nullptr);

			NSVGrasterizer* rast = nsvgCreateRasterizer();
			if (!rast)
				throw std::runtime_error("Could not create SVG rasterizer");

			nsvgRasterize(rast, svgimage, 0.0f, 0.0f, (float)dpiscale, (unsigned char*)image->GetData(), width, height, width * 4);

			nsvgDeleteRasterizer(rast);
			nsvgDelete(svgimage);
			return image;
		}
		catch (...)
		{
			nsvgDelete(svgimage);
			throw;
		}
	}
	else
	{
		throw std::runtime_error("Unsupported image format");
	}
}

/////////////////////////////////////////////////////////////////////////////

class CustomCursorImpl : public CustomCursor
{
public:
	CustomCursorImpl(std::vector<CustomCursorFrame> frames, const Point& hotspot) : Frames(std::move(frames)), Hotspot(hotspot)
	{
	}

	const std::vector<CustomCursorFrame>& GetFrames() const override
	{
		return Frames;
	}

	Point GetHotspot() const override
	{
		return Hotspot;
	}

	std::vector<CustomCursorFrame> Frames;
	Point Hotspot;
};

std::shared_ptr<CustomCursor> CustomCursor::Create(std::vector<CustomCursorFrame> frames, const Point& hotspot)
{
	return std::make_shared<CustomCursorImpl>(std::move(frames), hotspot);
}
