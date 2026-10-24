#pragma once

#include <cstdint>

struct TextureInfo;
class SoftwareTexture;
enum class TextureFormat : uint32_t;

// A texture's mips converted into its copy (SoftwareMip): each format read
// as the Vulkan device's uploads and its CPU decoders read it.
class SoftwareTextureUploader
{
public:
	static bool Supports(TextureFormat format);

	// Every mip, each the size of its own data. A texture of a format the
	// device lacks, or without a mip, is a white texel, as on the GPU
	// devices.
	static void Upload(SoftwareTexture* tex, const TextureInfo& info, bool masked);

	// A part of mip 0, and the repeated column and row where the part
	// touches column 0 or row 0. A copy whose mip 0 is not the info's is
	// made again whole.
	static void UploadRect(SoftwareTexture* tex, const TextureInfo& info, bool masked, int x, int y, int w, int h);

	static void MakeWhite(SoftwareTexture* tex);
};
