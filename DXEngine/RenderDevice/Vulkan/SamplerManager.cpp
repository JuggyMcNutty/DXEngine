
#include "SamplerManager.h"
#include "VulkanRenderDevice.h"
#include <dxgpu/vulkanbuilders.h>

SamplerManager::SamplerManager(VulkanRenderDevice* renderer) : renderer(renderer)
{
	CreateSceneSamplers();

	PPNearestRepeat = SamplerBuilder()
		.MinFilter(VK_FILTER_NEAREST)
		.MagFilter(VK_FILTER_NEAREST)
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
		.AddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
		.DebugName("ppNearestRepeat")
		.Create(renderer->Device.get());

	PPLinearClamp = SamplerBuilder()
		.MinFilter(VK_FILTER_LINEAR)
		.MagFilter(VK_FILTER_LINEAR)
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
		.AddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
		.DebugName("ppLinearClamp")
		.Create(renderer->Device.get());
}

void SamplerManager::CreateSceneSamplers()
{
	for (int i = 0; i < 4; i++)
	{
		SamplerBuilder builder;
		// Anisotropy on the smooth samplers alone: a NoSmooth texture is
		// point-sampled, pixel-exact at a slant as D3DDrv draws it, where a
		// driver's anisotropy averages its nearest texels (bugs.md D.40)
		if ((i & 1) == 0)
			builder.Anisotropy(8.0f);
		builder.MipLodBias(renderer->LODBias);

		if ((i & 1) != 0)
		{
			builder.MinFilter(VK_FILTER_NEAREST);
			builder.MagFilter(VK_FILTER_NEAREST);
			builder.MipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
		}
		else
		{
			builder.MinFilter(VK_FILTER_LINEAR);
			builder.MagFilter(VK_FILTER_LINEAR);
			builder.MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR);
		}

		if ((i & 2) != 0)
		{
			builder.AddressMode(VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE);
		}
		else
		{
			builder.AddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT);
		}

		builder.DebugName("SceneSampler");

		Samplers[i] = builder.Create(renderer->Device.get());
	}
}
