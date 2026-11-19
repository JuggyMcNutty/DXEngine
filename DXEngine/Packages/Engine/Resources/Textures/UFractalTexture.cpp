
#include "Precomp.h"
#include "UFractalTexture.h"

void UFractalTexture::Load(ObjectStream* stream)
{
	UTexture::Load(stream);

	// A package keeps no pixels for a fractal texture, only its size: they
	// start as zeros (the original's UTexture::Serialize).
	int width = UsedMipmaps.empty() ? USize() : UsedMipmaps.front().Width;
	int height = UsedMipmaps.empty() ? VSize() : UsedMipmaps.front().Height;

	// The steps write a row at 1 << UBits and wrap by the size less one, as
	// the original's, its masks from the size (dx-reverse-info fire-dll.md,
	// Loading) and its bits the package's: a size that is no power of two,
	// or bits that are not the size's, write past the pixels. A guard of
	// ours (bugs.md PKG.47, PKG.48): such a size refused, as a mip too large
	// is, and the bits taken from the size, every stock fractal texture's
	// being so.
	auto powerOfTwo = [](int size) { return size >= 0 && (size & (size - 1)) == 0; };
	if (!powerOfTwo(width) || !powerOfTwo(height) || (uint64_t)width * height > ((uint64_t)1 << 30))
		Exception::Throw("Texture " + Name.ToString() + " has a mip of " + std::to_string(width) + " by " + std::to_string(height));
	auto bits = [](int size) { int b = 0; while ((1 << b) < size) b++; return b; };

	UsedFormat = TextureFormat::P8;
	UsedMipmaps.resize(1);
	UnrealMipmap& mipmap = UsedMipmaps.front();
	mipmap.Width = width;
	mipmap.Height = height;
	mipmap.UBits = bits(width);
	mipmap.VBits = bits(height);
	mipmap.Data.resize((size_t)width * height);
	memset(mipmap.Data.data(), 0, (size_t)width * height);
}

void UFractalTexture::Save(PackageStreamWriter* stream)
{
	UTexture::Save(stream);
}
