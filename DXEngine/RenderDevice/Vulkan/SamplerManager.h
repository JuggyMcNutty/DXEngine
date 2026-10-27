#pragma once

#include <dxgpu/vulkanobjects.h>

class VulkanRenderDevice;

class SamplerManager
{
public:
	SamplerManager(VulkanRenderDevice* renderer);

	void CreateSceneSamplers();

	std::unique_ptr<VulkanSampler> Samplers[4];

	std::unique_ptr<VulkanSampler> PPNearestRepeat;
	std::unique_ptr<VulkanSampler> PPLinearClamp;


private:
	VulkanRenderDevice* renderer = nullptr;
};
