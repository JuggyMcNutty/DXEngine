#pragma once

#include <dxgpu/vulkanobjects.h>
#include "ShaderManager.h"

class VulkanRenderDevice;

struct PipelineState
{
	std::unique_ptr<VulkanPipeline> Pipeline;
	float MinDepth = 0.1f;
	float MaxDepth = 1.0f;
	bool DynamicDepthBias = false; // its depth bias set per draw (vkCmdSetDepthBias)
};

class RenderPassManager
{
public:
	RenderPassManager(VulkanRenderDevice* renderer);
	~RenderPassManager();

	void CreateRenderPass();
	void CreatePipelines();
	void CreateScenePipelines(const ShaderManager::SceneShaders& shaders, PipelineState* pipelines, PipelineState* linePipelines);

	void CreatePresentRenderPass();
	void CreatePresentPipeline();
	void CreateScreenshotPipeline();

	void CreatePostprocessRenderPass();
	void CreateBloomPipeline();

	PipelineState* GetPipeline(uint32_t polyflags);
	PipelineState* GetEndFlashPipeline();
	PipelineState* GetLinePipeline(bool occludeLines) { return &(MirrorClip ? Scene.ClipLinePipeline : Scene.LinePipeline)[occludeLines]; }

	// The frame drawn now a mirror's, its draws the pipelines that discard
	// behind its plane: on a device without shaderClipDistance alone
	void SetMirrorFrame(bool mirror) { MirrorClip = mirror && Scene.ClipPipeline[0].Pipeline; }

	struct
	{
		std::unique_ptr<VulkanPipelineLayout> PipelineLayout;
		std::unique_ptr<VulkanRenderPass> RenderPass;
		std::unique_ptr<VulkanRenderPass> RenderPassContinue;
		PipelineState Pipeline[33];
		PipelineState LinePipeline[2];
		PipelineState ClipPipeline[33];       // ShaderManager's SceneClip's: none with clip distances
		PipelineState ClipLinePipeline[2];
	} Scene;
	bool MirrorClip = false;

	struct
	{
		std::unique_ptr<VulkanPipelineLayout> PipelineLayout;
		std::unique_ptr<VulkanRenderPass> RenderPass;
		std::unique_ptr<VulkanPipeline> Pipeline[8];
		std::unique_ptr<VulkanPipeline> ScreenshotPipeline[8];
	} Present;

	struct
	{
		std::unique_ptr<VulkanPipelineLayout> PipelineLayout;
		std::unique_ptr<VulkanPipeline> Extract;
		std::unique_ptr<VulkanPipeline> Combine;
		// The first level added into the scene's colour, in its pass and at
		// its samples (TakeBloom): made with the scene's pipelines
		std::unique_ptr<VulkanPipeline> CombineScene;
		std::unique_ptr<VulkanPipeline> Scale;
		std::unique_ptr<VulkanPipeline> BlurVertical;
		std::unique_ptr<VulkanPipeline> BlurHorizontal;
	} Bloom;

	struct
	{
		std::unique_ptr<VulkanRenderPass> RenderPass;
		std::unique_ptr<VulkanRenderPass> RenderPassCombine;
	} Postprocess;

private:
	void CreateScenePipelineLayout();
	void CreatePresentPipelineLayout();
	void CreateBloomPipelineLayout();

	VulkanRenderDevice* renderer = nullptr;
};
