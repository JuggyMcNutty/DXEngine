#pragma once

#include <dxgpu/vulkanobjects.h>

struct TextureInfo;
class UnrealMipmap;
struct TextureColor;
enum class TextureFormat : uint32_t;

class TextureUploader
{
public:
	TextureUploader(VkFormat format) : Format(format) { }
	virtual ~TextureUploader() = default;

	virtual int GetUploadSize(int x, int y, int w, int h) = 0;
	virtual void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) = 0;

	VkFormat GetVkFormat() const { return Format; }

	// The rows an upload's band is a multiple of: a block's for a block
	// format.
	virtual int GetBlockRows() const { return 1; }

	// With physicalDevice == VK_NULL_HANDLE this returns the canonical uploader
	// for the format. With a device it first checks that the GPU can actually
	// sample the format (BC1 and RGB8 are not universally supported on mobile
	// GPUs) and substitutes a CPU-decoding uploader where it can't. A
	// format without a decoder returns nullptr, which the upload path turns
	// into a white fallback texture.
	static TextureUploader* GetUploader(TextureFormat format, VkPhysicalDevice physicalDevice = VK_NULL_HANDLE);

private:
	VkFormat Format;
};

class TextureUploader_P8 : public TextureUploader
{
public:
	TextureUploader_P8() : TextureUploader(VK_FORMAT_R8G8B8A8_UNORM) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};

class TextureUploader_BGRA8_LM : public TextureUploader
{
public:
	TextureUploader_BGRA8_LM() : TextureUploader(VK_FORMAT_R8G8B8A8_UNORM) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};

class TextureUploader_Simple : public TextureUploader
{
public:
	TextureUploader_Simple(VkFormat format, int bytesPerPixel) : TextureUploader(format), BytesPerPixel(bytesPerPixel) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;

private:
	int BytesPerPixel;
};

class TextureUploader_4x4Block : public TextureUploader
{
public:
	TextureUploader_4x4Block(VkFormat format, int bytesPerBlock) : TextureUploader(format), BytesPerBlock(bytesPerBlock) { }

	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
	int GetBlockRows() const override { return 4; }

private:
	int BytesPerBlock;
};

// CPU-decoding uploaders for GPUs that cannot sample a format natively.
// They all decode into simple unpacked formats mobile GPUs do support.

class TextureUploader_BC1_Decode : public TextureUploader
{
public:
	TextureUploader_BC1_Decode() : TextureUploader(VK_FORMAT_R8G8B8A8_UNORM) { }
	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
	int GetBlockRows() const override { return 4; }
};

class TextureUploader_RGB8_Decode : public TextureUploader
{
public:
	TextureUploader_RGB8_Decode() : TextureUploader(VK_FORMAT_R8G8B8A8_UNORM) { }
	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};

class TextureUploader_RGBA32F_Decode : public TextureUploader
{
public:
	// For GPUs that either cannot sample RGBA32F or cannot linearly filter it
	// (both are required for correct results and the PowerVR Rogue has neither).
	// Lightmaps and fog maps clamp their result to 0..1 in the shader, so the
	// 8-bit decode loses nothing.
	TextureUploader_RGBA32F_Decode() : TextureUploader(VK_FORMAT_R8G8B8A8_UNORM) { }
	int GetUploadSize(int x, int y, int w, int h) override;
	void UploadRect(void* dst, UnrealMipmap* mip, int x, int y, int w, int h, TextureColor* palette, bool masked) override;
};
