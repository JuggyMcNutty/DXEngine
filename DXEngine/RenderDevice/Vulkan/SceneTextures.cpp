
#include "Precomp.h"
#include "SceneTextures.h"
#include "VulkanRenderDevice.h"
#include "Utils/Logger.h"
#include <dxgpu/vulkanbuilders.h>

SceneTextures::SceneTextures(VulkanRenderDevice* renderer, int width, int height, int multisample) : Width(width), Height(height), Multisample(multisample)
{
	if (renderer->SceneSamplesCap > 0)
		multisample = std::min(multisample, renderer->SceneSamplesCap);
	SceneSamples = GetBestSampleCount(renderer->Device.get(), multisample);

	auto createSceneImages = [&]()
	{
		ColorBuffer = ImageBuilder()
			.Size(width, height)
			.Samples(SceneSamples)
			.Format(SceneFormat)
			.Usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
			.DebugName("colorBuffer")
			.Create(renderer->Device.get());

		ColorBufferView = ImageViewBuilder()
			.Image(ColorBuffer.get(), SceneFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.DebugName("colorBufferView")
			.Create(renderer->Device.get());

		DepthBuffer = ImageBuilder()
			.Size(width, height)
			.Samples(SceneSamples)
			.Format(VK_FORMAT_D32_SFLOAT)
			.Usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
			.DebugName("depthBuffer")
			.Create(renderer->Device.get());

		DepthBufferView = ImageViewBuilder()
			.Image(DepthBuffer.get(), VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT)
			.DebugName("depthBufferView")
			.Create(renderer->Device.get());
	};

	// Multisampled images the GPU cannot make (its memory short at a large
	// size) are made again with one sample, and with no more from then on,
	// logged, as the GL device halves a refused storage's, where the error
	// ended the game (bugs.md REN.76)
	try
	{
		createSceneImages();
	}
	catch (const std::exception& e)
	{
		if (SceneSamples == VK_SAMPLE_COUNT_1_BIT)
			throw;
		LogMessage("VulkanDrv: the scene's images could not be made at " + std::to_string((int)SceneSamples) + " samples (" + e.what() + "); one sample in their place");
		renderer->SceneSamplesCap = 1;
		SceneSamples = VK_SAMPLE_COUNT_1_BIT;
		createSceneImages();
	}

	for (int i = 0; i < 2; i++)
	{
		PPImage[i] = ImageBuilder()
			.Size(width, height)
			.Samples(VK_SAMPLE_COUNT_1_BIT)
			.Format(SceneFormat)
			.Usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
			.DebugName("ppImage")
			.Create(renderer->Device.get());

		PPImageView[i] = ImageViewBuilder()
			.Image(PPImage[i].get(), SceneFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.DebugName("ppImageView")
			.Create(renderer->Device.get());
	}

	int bloomWidth = width;
	int bloomHeight = height;
	for (int level = 0; level < NumBloomLevels; level++)
	{
		bloomWidth = (bloomWidth + 1) / 2;
		bloomHeight = (bloomHeight + 1) / 2;

		BloomBlurLevels[level].Width = bloomWidth;
		BloomBlurLevels[level].Height = bloomHeight;

		BloomBlurLevels[level].VTexture = ImageBuilder()
			.Size(bloomWidth, bloomHeight)
			.Samples(VK_SAMPLE_COUNT_1_BIT)
			.Format(SceneFormat)
			.Usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
			.DebugName("BloomVTexture")
			.Create(renderer->Device.get());

		BloomBlurLevels[level].HTexture = ImageBuilder()
			.Size(bloomWidth, bloomHeight)
			.Samples(VK_SAMPLE_COUNT_1_BIT)
			.Format(SceneFormat)
			.Usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
			.DebugName("BloomHTexture")
			.Create(renderer->Device.get());

		BloomBlurLevels[level].VTextureView = ImageViewBuilder()
			.Image(BloomBlurLevels[level].VTexture.get(), SceneFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.DebugName("BloomVTextureView")
			.Create(renderer->Device.get());

		BloomBlurLevels[level].HTextureView = ImageViewBuilder()
			.Image(BloomBlurLevels[level].HTexture.get(), SceneFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.DebugName("BloomHTextureView")
			.Create(renderer->Device.get());
	}

	PipelineBarrier barrier;

	barrier.AddImage(
		ColorBuffer.get(),
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		0,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT);

	barrier.AddImage(
		DepthBuffer.get(),
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		0,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT);

	for (int level = 0; level < NumBloomLevels; level++)
	{
		barrier.AddImage(
			BloomBlurLevels[level].VTexture.get(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			0,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT);
		barrier.AddImage(
			BloomBlurLevels[level].HTexture.get(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			0,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT);
	}

	barrier.Execute(
		renderer->Commands->GetDrawCommands(),
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
}

SceneTextures::~SceneTextures()
{
}

VkSampleCountFlagBits SceneTextures::GetBestSampleCount(VulkanDevice* device, int multisample)
{
	const auto& limits = device->PhysicalDevice.Properties.Properties.limits;
	int requestedSamples = clamp(multisample, 1, 64);
	// Every attachment of the scene's framebuffer takes the count, and is
	// sampled: the colour and the depth, D32_SFLOAT, which has no stencil
	VkSampleCountFlags deviceSampleCounts =
		limits.sampledImageColorSampleCounts & limits.sampledImageDepthSampleCounts &
		limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;

	int samples = 1;
	VkSampleCountFlags bit = VK_SAMPLE_COUNT_1_BIT;
	VkSampleCountFlags best = bit;
	while (samples <= requestedSamples)
	{
		if (deviceSampleCounts & bit)
		{
			best = bit;
		}
		samples <<= 1;
		bit <<= 1;
	}

	return (VkSampleCountFlagBits)best;
}
