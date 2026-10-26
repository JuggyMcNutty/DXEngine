#pragma once

#include "ShaderManager.h"
#include "Math/mat.h"
#include "Math/vec.h"

class VulkanRenderDevice;

enum { NumBloomLevels = 4 };

class SceneTextures
{
public:
	// The scene's, its post-process images' and the bloom's: 10-bit UNORM, as
	// the GL device's RGB10_A2, so every blend clamps as D3DDrv's 8-bit frame
	// does, darks banded less than in it; a float buffer kept a modulated
	// blend's 2 x src x dst past 1 for the blends after it
	static constexpr VkFormat SceneFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;

	SceneTextures(VulkanRenderDevice* renderer, int width, int height, int multisample);
	~SceneTextures();

	// Current active multisample setting
	VkSampleCountFlagBits SceneSamples = VK_SAMPLE_COUNT_1_BIT;

	// Scene framebuffer color image
	std::unique_ptr<VulkanImage> ColorBuffer;
	std::unique_ptr<VulkanImageView> ColorBufferView;

	// Scene framebuffer depth buffer
	std::unique_ptr<VulkanImage> DepthBuffer;
	std::unique_ptr<VulkanImageView> DepthBufferView;

	// Post processing image buffers
	std::unique_ptr<VulkanImage> PPImage[2];
	std::unique_ptr<VulkanImageView> PPImageView[2];

	// Size of the scene framebuffer
	int Width = 0;
	int Height = 0;
	int Multisample = 0;

	// Bloom post processing buffers
	struct
	{
		std::unique_ptr<VulkanImage> VTexture;
		std::unique_ptr<VulkanImageView> VTextureView;
		std::unique_ptr<VulkanImage> HTexture;
		std::unique_ptr<VulkanImageView> HTextureView;
		int Width = 0;
		int Height = 0;
	} BloomBlurLevels[NumBloomLevels];

private:
	static VkSampleCountFlagBits GetBestSampleCount(VulkanDevice* device, int multisample);
};
