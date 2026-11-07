
#include "Precomp.h"
#include "RenderPassManager.h"
#include "VulkanRenderDevice.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include <dxgpu/vulkanbuilders.h>
#include <dxgpu/vulkanswapchain.h>

RenderPassManager::RenderPassManager(VulkanRenderDevice* renderer) : renderer(renderer)
{
	CreateScenePipelineLayout();
	CreatePostprocessRenderPass();
	CreatePresentPipelineLayout();
	CreateScreenshotPipeline();
	CreateBloomPipelineLayout();
	CreateBloomPipeline();
}

RenderPassManager::~RenderPassManager()
{
}

void RenderPassManager::CreateScenePipelineLayout()
{
	Scene.PipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(renderer->DescriptorSets->GetTextureSetLayout())
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants))
		.DebugName("ScenePipelineLayout")
		.Create(renderer->Device.get());
}

void RenderPassManager::CreatePresentPipelineLayout()
{
	Present.PipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(renderer->DescriptorSets->GetPresentLayout())
		.AddPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PresentPushConstants))
		.DebugName("PresentPipelineLayout")
		.Create(renderer->Device.get());
}

void RenderPassManager::CreateBloomPipelineLayout()
{
	Bloom.PipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(renderer->DescriptorSets->GetBloomLayout())
		.AddPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BloomPushConstants))
		.DebugName("BloomPipelineLayout")
		.Create(renderer->Device.get());
}

PipelineState* RenderPassManager::GetPipeline(uint32_t PolyFlags)
{
	int index;
	if (PolyFlags & PF_Translucent)
	{
		index = 0;
	}
	else if (PolyFlags & PF_Modulated)
	{
		index = 1;
	}
	else if (PolyFlags & PF_Highlighted)
	{
		index = 2;
	}
	else
	{
		index = 3;
	}

	if (PolyFlags & PF_Invisible)
	{
		index |= 4;
	}
	if (PolyFlags & PF_Occlude)
	{
		index |= 8;
	}
	if (PolyFlags & PF_Masked)
	{
		index |= 16;
	}

	if (PolyFlags == PF_SubpixelFont)
	{
		index = 32;
	}

	return &(MirrorClip ? Scene.ClipPipeline : Scene.Pipeline)[index];
}

PipelineState* RenderPassManager::GetEndFlashPipeline()
{
	return &Scene.Pipeline[2];
}

void RenderPassManager::CreatePipelines()
{
	CreateScenePipelines(renderer->Shaders->Scene, Scene.Pipeline, Scene.LinePipeline);
	if (!renderer->Shaders->SceneClip.VertexShader.empty())
		CreateScenePipelines(renderer->Shaders->SceneClip, Scene.ClipPipeline, Scene.ClipLinePipeline);
}

void RenderPassManager::CreateScenePipelines(const ShaderManager::SceneShaders& shaders, PipelineState* pipelines, PipelineState* linePipelines)
{
	const std::vector<uint32_t>& vertShader = shaders.VertexShader;
	const std::vector<uint32_t>& fragShader = shaders.FragmentShader;
	const std::vector<uint32_t>& fragShaderAlphaTest = shaders.FragmentShaderAlphaTest;
	VulkanPipelineLayout* layout = Scene.PipelineLayout.get();
	static const char* debugName = "ScenePipeline";

	// The 33 scene pipelines. The first 32 take their depth bias per draw: a
	// step toward the viewer for a mover's surfaces (SurfaceInfo::DepthBias),
	// none for the rest, as the GL device's and D3DDrv's draws take none -- a
	// translucent draw biased by its slope came through the opaque one before
	// it from afar (an augmentation canister's inside through its shell).
	for (int i = 0; i < 33; i++)
	{
		GraphicsPipelineBuilder builder;
		builder.AddVertexShader(vertShader);
		builder.Viewport(0.0f, 0.0f, (float)renderer->Textures->Scene->Width, (float)renderer->Textures->Scene->Height);
		builder.Scissor(0, 0, renderer->Textures->Scene->Width, renderer->Textures->Scene->Height);
		builder.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		builder.Cull(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
		builder.AddVertexBufferBinding(0, sizeof(SceneVertex));
		builder.AddVertexAttribute(0, 0, VK_FORMAT_R32_UINT, offsetof(SceneVertex, Flags));
		builder.AddVertexAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneVertex, Position));
		builder.AddVertexAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord));
		builder.AddVertexAttribute(3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord2));
		builder.AddVertexAttribute(4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord3));
		builder.AddVertexAttribute(5, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord4));
		builder.AddVertexAttribute(6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SceneVertex, Color));
		builder.AddVertexAttribute(7, 0, VK_FORMAT_R32G32B32A32_SINT, offsetof(SceneVertex, TextureBinds));
		builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
		builder.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
		builder.Layout(layout);
		builder.RenderPass(Scene.RenderPass.get());

		// No depth clamp: what lies nearer than the near plane, view depth 1,
		// or past the far one is clipped there, as D3DDrv's depth for it, under 0
		// or over 1, is cut (dx-reverse-info d3ddrv-dll.md, the screen's pixels;
		// Render.dll clips a frame at the view's four sides alone)

		if (i < 32)
		{
			ColorBlendAttachmentBuilder colorblend;
			switch (i & 3)
			{
			case 0: // PF_Translucent
				colorblend.BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR);
				break;
			case 1: // PF_Modulated
				colorblend.BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_SRC_COLOR);
				break;
			case 2: // PF_Highlighted
				colorblend.BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
				break;
			case 3:
				colorblend.BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO); // Hmm, is it faster to keep the blend mode enabled or to toggle it?
				break;
			}
			builder.DepthBias(true, 0.0f, 0.0f, 0.0f);
			builder.AddDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);

			if (i & 4) // PF_Invisible
			{
				colorblend.ColorWriteMask(0);
			}

			if (i & 8) // PF_Occlude
			{
				builder.DepthStencilEnable(true, true, false);
			}
			else
			{
				builder.DepthStencilEnable(true, false, false);
			}

			if (i & 16) // PF_Masked
				builder.AddFragmentShader(fragShaderAlphaTest);
			else
				builder.AddFragmentShader(fragShader);

			builder.AddColorBlendAttachment(colorblend.Create());
		}
		else // PF_SubpixelFont
		{
			builder.AddFragmentShader(fragShaderAlphaTest);
			builder.AddColorBlendAttachment(ColorBlendAttachmentBuilder()
				.BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_CONSTANT_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR)
				.Create());
		}

		builder.RasterizationSamples(renderer->Textures->Scene->SceneSamples);
		builder.DebugName(debugName);

		pipelines[i].Pipeline = builder.Create(renderer->Device.get());
		pipelines[i].DynamicDepthBias = i < 32;
	}

	// Line pipeline
	for (int i = 0; i < 2; i++)
	{
		GraphicsPipelineBuilder builder;
		builder.AddVertexShader(vertShader);
		builder.Viewport(0.0f, 0.0f, (float)renderer->Textures->Scene->Width, (float)renderer->Textures->Scene->Height);
		builder.Scissor(0, 0, renderer->Textures->Scene->Width, renderer->Textures->Scene->Height);
		builder.Topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
		builder.Cull(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
		builder.AddVertexBufferBinding(0, sizeof(SceneVertex));
		builder.AddVertexAttribute(0, 0, VK_FORMAT_R32_UINT, offsetof(SceneVertex, Flags));
		builder.AddVertexAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneVertex, Position));
		builder.AddVertexAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord));
		builder.AddVertexAttribute(3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord2));
		builder.AddVertexAttribute(4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord3));
		builder.AddVertexAttribute(5, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord4));
		builder.AddVertexAttribute(6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SceneVertex, Color));
		builder.AddVertexAttribute(7, 0, VK_FORMAT_R32G32B32A32_SINT, offsetof(SceneVertex, TextureBinds));
		builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
		builder.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
		builder.Layout(layout);
		builder.RenderPass(Scene.RenderPass.get());

		builder.AddColorBlendAttachment(ColorBlendAttachmentBuilder().BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA).Create());

		builder.DepthStencilEnable(true, true, false);
		builder.AddFragmentShader(fragShader);

		builder.RasterizationSamples(renderer->Textures->Scene->SceneSamples);
		builder.DebugName(debugName);

		linePipelines[i].Pipeline = builder.Create(renderer->Device.get());

		if (i == 0)
		{
			linePipelines[i].MinDepth = 0.0f;
			linePipelines[i].MaxDepth = 0.1f;
		}
	}

}

void RenderPassManager::CreateRenderPass()
{
	Scene.RenderPass = RenderPassBuilder()
		.AddAttachment(
			SceneTextures::SceneFormat,
			renderer->Textures->Scene->SceneSamples,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddDepthStencilAttachment(
			VK_FORMAT_D32_SFLOAT,
			renderer->Textures->Scene->SceneSamples,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddSubpassDepthStencilAttachmentRef(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		.DebugName("SceneRenderPass")
		.Create(renderer->Device.get());

	Scene.RenderPassContinue = RenderPassBuilder()
		.AddAttachment(
			SceneTextures::SceneFormat,
			renderer->Textures->Scene->SceneSamples,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddDepthStencilAttachment(
			VK_FORMAT_D32_SFLOAT,
			renderer->Textures->Scene->SceneSamples,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddSubpassDepthStencilAttachmentRef(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		.DebugName("SceneRenderPassContinue")
		.Create(renderer->Device.get());
}

void RenderPassManager::CreatePresentRenderPass()
{
	Present.RenderPass = RenderPassBuilder()
		.AddAttachment(
			renderer->Commands->SwapChain->Format().format,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.DebugName("PresentRenderPass")
		.Create(renderer->Device.get());
}

void RenderPassManager::CreatePresentPipeline()
{
	for (int i = 0; i < 8; i++)
	{
		Present.Pipeline[i] = GraphicsPipelineBuilder()
			.AddVertexShader(renderer->Shaders->Postprocess.VertexShader)
			.AddFragmentShader(renderer->Shaders->Postprocess.FragmentPresentShader[i])
			.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
			.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
			.Layout(Present.PipelineLayout.get())
			.RenderPass(Present.RenderPass.get())
			.DebugName("PresentPipeline")
			.Create(renderer->Device.get());
	}
}

void RenderPassManager::CreateScreenshotPipeline()
{
	for (int i = 0; i < 8; i++)
	{
		Present.ScreenshotPipeline[i] = GraphicsPipelineBuilder()
			.AddVertexShader(renderer->Shaders->Postprocess.VertexShader)
			.AddFragmentShader(renderer->Shaders->Postprocess.FragmentPresentShader[i])
			.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
			.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
			.Layout(Present.PipelineLayout.get())
			.RenderPass(Postprocess.RenderPass.get())
			.DebugName("ScreenshotPipeline")
			.Create(renderer->Device.get());
	}
}

void RenderPassManager::CreatePostprocessRenderPass()
{
	Postprocess.RenderPass = RenderPassBuilder()
		.AddAttachment(
			SceneTextures::SceneFormat,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.DebugName("PPRenderPass")
		.Create(renderer->Device.get());

	Postprocess.RenderPassCombine = RenderPassBuilder()
		.AddAttachment(
			SceneTextures::SceneFormat,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.DebugName("PPRenderPassCombine")
		.Create(renderer->Device.get());
}

void RenderPassManager::CreateBloomPipeline()
{
	Bloom.Extract = GraphicsPipelineBuilder()
		.AddVertexShader(renderer->Shaders->Postprocess.VertexShader)
		.AddFragmentShader(renderer->Shaders->Bloom.Extract)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
		.Layout(Bloom.PipelineLayout.get())
		.RenderPass(Postprocess.RenderPass.get())
		.DebugName("Bloom.Extract")
		.Create(renderer->Device.get());

	Bloom.Combine = GraphicsPipelineBuilder()
		.AddVertexShader(renderer->Shaders->Postprocess.VertexShader)
		.AddFragmentShader(renderer->Shaders->Bloom.Combine)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
		.AddColorBlendAttachment(ColorBlendAttachmentBuilder().BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE).Create())
		.Layout(Bloom.PipelineLayout.get())
		.RenderPass(Postprocess.RenderPass.get())
		.DebugName("Bloom.Combine")
		.Create(renderer->Device.get());

	Bloom.Scale = GraphicsPipelineBuilder()
		.AddVertexShader(renderer->Shaders->Postprocess.VertexShader)
		.AddFragmentShader(renderer->Shaders->Bloom.Combine)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
		.Layout(Bloom.PipelineLayout.get())
		.RenderPass(Postprocess.RenderPass.get())
		.DebugName("Bloom.Copy")
		.Create(renderer->Device.get());

	Bloom.BlurVertical = GraphicsPipelineBuilder()
		.AddVertexShader(renderer->Shaders->Postprocess.VertexShader)
		.AddFragmentShader(renderer->Shaders->Bloom.BlurVertical)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
		.Layout(Bloom.PipelineLayout.get())
		.RenderPass(Postprocess.RenderPass.get())
		.DebugName("Bloom.BlurVertical")
		.Create(renderer->Device.get());

	Bloom.BlurHorizontal = GraphicsPipelineBuilder()
		.AddVertexShader(renderer->Shaders->Postprocess.VertexShader)
		.AddFragmentShader(renderer->Shaders->Bloom.BlurHorizontal)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
		.Layout(Bloom.PipelineLayout.get())
		.RenderPass(Postprocess.RenderPass.get())
		.DebugName("Bloom.BlurHorizontal")
		.Create(renderer->Device.get());
}
