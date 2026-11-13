
#include "Precomp.h"
#include "VulkanRenderDevice.h"
#include "CachedTexture.h"
#include "Utils/Logger.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include <dxgpu/vulkanbuilders.h>
#include <dxgpu/vulkanswapchain.h>
#include <dxgpu/vulkansurface.h>
#include <dxwidgets/core/widget.h>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

VulkanRenderDevice::VulkanRenderDevice(Widget* InViewport)
{
	Viewport = InViewport;

	try
	{
		std::shared_ptr<VulkanInstance> instance = VulkanInstanceBuilder()
			.RequireExtensions(Viewport->GetVulkanInstanceExtensions())
			.OptionalSwapchainColorspace()
			.DebugLayer(UseDebugLayer)
			.Create();

		auto surface = std::make_shared<VulkanSurface>(instance, Viewport->CreateVulkanSurface(instance->Instance));
		if (!surface)
			throw std::runtime_error("No vulkan surface found");

		auto deviceBuilder = VulkanDeviceBuilder();
		deviceBuilder.OptionalDescriptorIndexing();
		deviceBuilder.OptionalRayQuery();
		deviceBuilder.Surface(surface);
		// A clamped tile's sampler: mirrored once, then clamped, where the GPU
		// has it; else clamped to the edge, the same texels for a tile within
		// its texture, as the GL device falls back (SamplerManager), so a GPU
		// without it (a Mali, PowerVR or Adreno board's) keeps the Vulkan device
		deviceBuilder.OptionalExtension(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME);
		deviceBuilder.SelectDevice(VkDeviceIndex);
		if (CPUDriver)
			deviceBuilder.SelectCPUDevice();
		Device = deviceBuilder.Create(surface->Instance);

		// Descriptor indexing is missing on several mobile GPUs (PowerVR Rogue, for one).
		// Without it the renderer falls back to a descriptor set per texture combination.
		// The bindless set's binding is partially bound, of a variable count
		// and updated after bind (DescriptorSetManager): each feature, and room
		// for its MaxBindlessTextures image samplers updated after bind.
		const auto& indexing = Device->EnabledFeatures.DescriptorIndexing;
		const auto& indexingLimits = Device->PhysicalDevice.Properties.DescriptorIndexing;
		const uint32_t bindlessCount = DescriptorSetManager::MaxBindlessTextures;
		SupportsBindless =
			indexing.descriptorBindingPartiallyBound &&
			indexing.runtimeDescriptorArray &&
			indexing.shaderSampledImageArrayNonUniformIndexing &&
			indexing.descriptorBindingSampledImageUpdateAfterBind &&
			indexing.descriptorBindingVariableDescriptorCount &&
			indexingLimits.maxDescriptorSetUpdateAfterBindSampledImages >= bindlessCount &&
			indexingLimits.maxDescriptorSetUpdateAfterBindSamplers >= bindlessCount &&
			indexingLimits.maxPerStageDescriptorUpdateAfterBindSampledImages >= bindlessCount &&
			indexingLimits.maxPerStageDescriptorUpdateAfterBindSamplers >= bindlessCount &&
			indexingLimits.maxPerStageUpdateAfterBindResources >= bindlessCount;

		// DXE_VK_NO_BINDLESS exercises that fallback on hardware that doesn't need it.
		if (std::getenv("DXE_VK_NO_BINDLESS"))
			SupportsBindless = false;

		Commands.reset(new CommandBufferManager(this));
		Samplers.reset(new SamplerManager(this));
		Textures.reset(new TextureManager(this));
		Buffers.reset(new BufferManager(this));
		Shaders.reset(new ShaderManager(this));
		Uploads.reset(new UploadManager(this));
		DescriptorSets.reset(new DescriptorSetManager(this));
		RenderPasses.reset(new RenderPassManager(this));
		Framebuffers.reset(new FramebufferManager(this));

		const auto& props = Device->PhysicalDevice.Properties.Properties;

		std::string deviceType;
		switch (props.deviceType)
		{
		case VK_PHYSICAL_DEVICE_TYPE_OTHER: deviceType = "other"; break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = "integrated gpu"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceType = "discrete gpu"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceType = "virtual gpu"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceType = "cpu"; break;
		default: deviceType = std::to_string((int)props.deviceType); break;
		}

		std::string apiVersion = std::to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." + std::to_string(VK_VERSION_MINOR(props.apiVersion)) + "." + std::to_string(VK_VERSION_PATCH(props.apiVersion));
		std::string driverVersion = std::to_string(VK_VERSION_MAJOR(props.driverVersion)) + "." + std::to_string(VK_VERSION_MINOR(props.driverVersion)) + "." + std::to_string(VK_VERSION_PATCH(props.driverVersion));

		LogMessage(std::string("Vulkan device: ") + props.deviceName);
		LogMessage("Vulkan device type: " + deviceType);
		LogMessage("Vulkan version: " + apiVersion + " (api) " + driverVersion + " (driver)");
		LogMessage(std::string("Vulkan textures: ") + (SupportsBindless ? "bindless" : "per-batch descriptor sets (no descriptor indexing)"));
		if (!Device->SupportsExtension(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME))
			LogMessage("Vulkan: no VK_KHR_sampler_mirror_clamp_to_edge; a clamped tile clamps to the edge");

		// Mesa's CPU driver alone, lavapipe, not asked for: no GPU here, and
		// the software device draws faster on the CPU (ENGINE.md, the
		// software device against Mesa's)
		if (!CPUDriver && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
			throw std::runtime_error(std::string("its only device draws on the CPU: ") + props.deviceName);
	}
	catch (const std::exception& e)
	{
		Exception::Throw(std::string("Could not create vulkan renderer: ") + e.what());
	}
}

VulkanRenderDevice::~VulkanRenderDevice()
{
	if (Device)
		vkDeviceWaitIdle(Device->device);

	Framebuffers.reset();
	RenderPasses.reset();
	DescriptorSets.reset();
	Uploads.reset();
	Shaders.reset();
	Buffers.reset();
	Textures.reset();
	Samplers.reset();
	Commands.reset();

	Device.reset();
}

void VulkanPrintLog(const char* typestr, const std::string& msg)
{
	LogMessage(std::string("[") + typestr + "] " + msg);
}

void VulkanError(const char* text)
{
	throw std::runtime_error(text);
}

void VulkanRenderDevice::SubmitAndWait(bool present, int presentWidth, int presentHeight)
{
	Submit(present, presentWidth, presentHeight, true);
}

void VulkanRenderDevice::Submit(bool present, int presentWidth, int presentHeight, bool wait)
{
	if (SupportsBindless)
		DescriptorSets->UpdateBindlessSet();

	Commands->SubmitCommands(present, presentWidth, presentHeight, wait);

	DepthBiasSet = DepthBiasUnknown;
	Batch.SceneIndexStart = 0;
	SceneVertexPos = 0;
	SceneIndexPos = 0;
}

void VulkanRenderDevice::Flush(bool AllowPrecache)
{
	if (IsLocked)
	{
		DrawBatch(Commands->GetDrawCommands());
		Commands->GetDrawCommands()->endRenderPass();
		SubmitAndWait(false, 0, 0);

		ClearTextureCache();

		auto cmdbuffer = Commands->GetDrawCommands();

		VkAccessFlags srcColorAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		VkAccessFlags dstColorAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		VkAccessFlags srcDepthAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		VkAccessFlags dstDepthAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

		PipelineBarrier()
			.AddImage(Textures->Scene->ColorBuffer.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, srcColorAccess, dstColorAccess)
			.AddImage(Textures->Scene->DepthBuffer.get(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, srcDepthAccess, dstDepthAccess, VK_IMAGE_ASPECT_DEPTH_BIT)
			.Execute(cmdbuffer, srcStages, dstStages);

		RenderPassBegin()
			.RenderPass(RenderPasses->Scene.RenderPassContinue.get())
			.Framebuffer(Framebuffers->SceneFramebuffer.get())
			.RenderArea(0, 0, Textures->Scene->Width, Textures->Scene->Height)
			.AddClearColor(0.0f, 0.0f, 0.0f, 0.0f)
			.AddClearDepthStencil(1.0f, 0)
			.Execute(cmdbuffer);

		VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
		cmdbuffer->bindIndexBuffer(Buffers->SceneIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
		// A new command buffer keeps no dynamic state: the scene node's
		// viewport again, as FlushDrawBatchAndWait sets it
		cmdbuffer->setViewport(0, 1, &viewportdesc);
	}
	else
	{
		ClearTextureCache();
	}

	if (AllowPrecache && UsePrecache)
		PrecacheOnFlip = 1;
}

void VulkanRenderDevice::Lock(vec4 InFlashScale, vec4 InFlashFog, vec4 ScreenClear)
{
	// The frame's counts begin again (GetFrameStats)
	Stats = {};
	DrawTimes = {};
	Textures->CacheStats.BeginFrame();

	// The previous frame may still be drawing (Unlock does not wait for it).
	Commands->WaitForFrame();

	// A new mip bias: the scene samplers made again, with nothing drawing,
	// every texture's descriptor written again with them (SetLODBias)
	if (LODBiasChanged)
	{
		ClearTextureCache();
		Samplers->CreateSceneSamplers();
		LODBiasChanged = false;
	}

	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	// If frame textures no longer match the window or user settings, recreate them along with the swap chain
	if (!Textures->Scene || Textures->Scene->Width != GetRenderWidth() || Textures->Scene->Height != GetRenderHeight() ||Textures->Scene->Multisample != GetSettingsMultisample())
	{
		Framebuffers->DestroySceneFramebuffer();
		Textures->Scene.reset();
		Textures->Scene.reset(new SceneTextures(this, GetRenderWidth(), GetRenderHeight(), GetSettingsMultisample()));
		RenderPasses->CreateRenderPass();
		RenderPasses->CreatePipelines();
		Framebuffers->CreateSceneFramebuffer();
		DescriptorSets->UpdateFrameDescriptors();
	}

	auto cmdbuffer = Commands->GetDrawCommands();

	// Special thanks to Khronos and AMD for making this absolute hell to use.
	VkAccessFlags srcColorAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	VkAccessFlags dstColorAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	VkAccessFlags srcDepthAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	VkAccessFlags dstDepthAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

	PipelineBarrier()
		.AddImage(Textures->Scene->ColorBuffer.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, srcColorAccess, dstColorAccess)
		.AddImage(Textures->Scene->DepthBuffer.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, srcDepthAccess, dstDepthAccess, VK_IMAGE_ASPECT_DEPTH_BIT)
		.Execute(cmdbuffer, srcStages, dstStages);

	RenderPassBegin()
		.RenderPass(RenderPasses->Scene.RenderPass.get())
		.Framebuffer(Framebuffers->SceneFramebuffer.get())
		.RenderArea(0, 0, Textures->Scene->Width, Textures->Scene->Height)
		.AddClearColor(ScreenClear.x, ScreenClear.y, ScreenClear.z, ScreenClear.w)
		.AddClearDepthStencil(1.0f, 0)
		.Execute(cmdbuffer);

	VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
	VkDeviceSize offsets[] = { 0 };
	cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
	cmdbuffer->bindIndexBuffer(Buffers->SceneIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

	IsLocked = true;
}

void VulkanRenderDevice::FlushDrawBatchAndWait()
{
	DrawBatch(Commands->GetDrawCommands());
	Commands->GetDrawCommands()->endRenderPass();
	SubmitAndWait(false, 0, 0);

	auto drawcommands = Commands->GetDrawCommands();

	VkAccessFlags srcColorAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	VkAccessFlags dstColorAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	VkAccessFlags srcDepthAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	VkAccessFlags dstDepthAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

	PipelineBarrier()
		.AddImage(Textures->Scene->ColorBuffer.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, srcColorAccess, dstColorAccess)
		.AddImage(Textures->Scene->DepthBuffer.get(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, srcDepthAccess, dstDepthAccess, VK_IMAGE_ASPECT_DEPTH_BIT)
		.Execute(drawcommands, srcStages, dstStages);

	RenderPassBegin()
		.RenderPass(RenderPasses->Scene.RenderPassContinue.get())
		.Framebuffer(Framebuffers->SceneFramebuffer.get())
		.RenderArea(0, 0, Textures->Scene->Width, Textures->Scene->Height)
		.Execute(drawcommands);

	VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
	VkDeviceSize offsets[] = { 0 };
	drawcommands->bindVertexBuffers(0, 1, vertexBuffers, offsets);
	drawcommands->bindIndexBuffer(Buffers->SceneIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
	drawcommands->setViewport(0, 1, &viewportdesc);
}

void VulkanRenderDevice::Unlock(bool Blit)
{
	DrawBatch(Commands->GetDrawCommands());
	Commands->GetDrawCommands()->endRenderPass();

	BlitSceneToPostprocess();

	int windowWidth = Viewport->GetNativePixelWidth();
	int windowHeight = Viewport->GetNativePixelHeight();

	// Leave the frame in flight, so the next game tick runs while the GPU
	// draws.
	Submit(Blit ? true : false, windowWidth, windowHeight, false);

	Batch.Pipeline = nullptr;
	Batch.DepthBias = 0;
	Batch.DescriptorSet = nullptr;

	IsLocked = false;
}

bool VulkanRenderDevice::SupportsTextureFormat(TextureFormat Format)
{
	return Uploads->SupportsTextureFormat(Format);
}

void VulkanRenderDevice::UpdateTextureRect(TextureInfo& Info, int U, int V, int UL, int VL)
{
	Textures->UpdateTextureRect(&Info, U, V, UL, VL);
}

void VulkanRenderDevice::DrawBatch(VulkanCommandBuffer* cmdbuffer)
{
	size_t icount = SceneIndexPos - Batch.SceneIndexStart;
	if (icount > 0)
	{
		if (viewportdesc.minDepth != Batch.Pipeline->MinDepth || viewportdesc.maxDepth != Batch.Pipeline->MaxDepth)
		{
			viewportdesc.minDepth = Batch.Pipeline->MinDepth;
			viewportdesc.maxDepth = Batch.Pipeline->MaxDepth;
			cmdbuffer->setViewport(0, 1, &viewportdesc);
		}

		auto layout = RenderPasses->Scene.PipelineLayout.get();
		cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, Batch.Pipeline->Pipeline.get());
		if (Batch.Pipeline->DynamicDepthBias)
		{
			int bias = Batch.DepthBias;
			if (DepthBiasSet != bias)
			{
				// Toward the viewer -1, away 1
				float factor = (float)-bias;
				cmdbuffer->setDepthBias(factor, 0.0f, factor); // constant, clamp, slope
				DepthBiasSet = bias;
			}
		}
		else
		{
			DepthBiasSet = DepthBiasUnknown;
		}
		cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, Batch.DescriptorSet);
		cmdbuffer->pushConstants(layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants), &pushconstants);
		cmdbuffer->setBlendConstants(Batch.BlendConstants);
		cmdbuffer->drawIndexed((uint32_t)icount, 1, (uint32_t)Batch.SceneIndexStart, 0, 0);
		Batch.SceneIndexStart = SceneIndexPos;
		Stats.DrawCalls++;
	}
}

void VulkanRenderDevice::DrawComplexSurface(SceneNode* Frame, SurfaceInfo& Surface, SurfaceFacet& Facet)
{
	StatClock drawClock(DrawTimes.Surfaces, TimeDraws);
	uint32_t PolyFlags = ApplyPrecedenceRules(Surface.PolyFlags);

	// The textures first, drawn or not: a lightmap's change, perhaps only a
	// part of it, is handed over once
	CachedTexture* tex = Textures->GetTexture(Surface.Texture, !!(PolyFlags & PF_Masked));
	CachedTexture* lightmap = Textures->GetTexture(Surface.LightMap, false);
	CachedTexture* macrotex = Textures->GetTexture(Surface.MacroTexture, false);
	CachedTexture* detailtex = Textures->GetTexture(Surface.DetailTexture, false);
	CachedTexture* fogmap = (Surface.FogMap && Surface.FogMap->NumMips > 0 && !Surface.FogMap->Mips[0].Data.empty()) ? Textures->GetTexture(Surface.FogMap, false) : nullptr;

	if (Facet.VertexCount < 3)
		return;

	if (Surface.DetailTexture && Surface.FogMap) detailtex = nullptr;

	float UDot = dot(Facet.MapCoords.XAxis, Facet.MapCoords.Origin);
	float VDot = dot(Facet.MapCoords.YAxis, Facet.MapCoords.Origin);

	float UPan = tex ? UDot + Surface.Texture->Pan.x : 0.0f;
	float VPan = tex ? VDot + Surface.Texture->Pan.y : 0.0f;
	float UMult = tex ? GetUMult(*Surface.Texture) : 0.0f;
	float VMult = tex ? GetVMult(*Surface.Texture) : 0.0f;
	float LMUPan = lightmap ? UDot + Surface.LightMap->Pan.x - 0.5f * Surface.LightMap->UScale : 0.0f;
	float LMVPan = lightmap ? VDot + Surface.LightMap->Pan.y - 0.5f * Surface.LightMap->VScale : 0.0f;
	float LMUMult = lightmap ? GetUMult(*Surface.LightMap) : 0.0f;
	float LMVMult = lightmap ? GetVMult(*Surface.LightMap) : 0.0f;
	float MacroUPan = macrotex ? UDot + Surface.MacroTexture->Pan.x : 0.0f;
	float MacroVPan = macrotex ? VDot + Surface.MacroTexture->Pan.y : 0.0f;
	float MacroUMult = macrotex ? GetUMult(*Surface.MacroTexture) : 0.0f;
	float MacroVMult = macrotex ? GetVMult(*Surface.MacroTexture) : 0.0f;
	// The detail texture by its own pan, as the GL device's: Deus Ex's is
	// not panned with the surface (VisibleNode)
	float DetailUPan = detailtex ? UDot + Surface.DetailTexture->Pan.x : 0.0f;
	float DetailVPan = detailtex ? VDot + Surface.DetailTexture->Pan.y : 0.0f;
	float DetailUMult = detailtex ? GetUMult(*Surface.DetailTexture) : 0.0f;
	float DetailVMult = detailtex ? GetVMult(*Surface.DetailTexture) : 0.0f;

	uint32_t flags = 0;
	if (lightmap) flags |= 1;
	if (macrotex) flags |= 2;
	if (detailtex && !fogmap) flags |= 4;
	if (fogmap) flags |= 8;

	if (LightMode == 1) flags |= 64;

	if (fogmap) // if Surface.FogMap exists, use instead of detail texture
	{
		detailtex = fogmap;
		DetailUPan = UDot + Surface.FogMap->Pan.x - 0.5f * Surface.FogMap->UScale;
		DetailVPan = VDot + Surface.FogMap->Pan.y - 0.5f * Surface.FogMap->VScale;
		DetailUMult = GetUMult(*Surface.FogMap);
		DetailVMult = GetVMult(*Surface.FogMap);
	}

	SetPipeline(RenderPasses->GetPipeline(PolyFlags), Surface.DepthBias);

	ivec4 textureBinds = GetTextureIndexes(PolyFlags, tex, lightmap, macrotex, detailtex);
	vec4 color(1.0f);

	auto pts = Facet.Vertices;
	uint32_t vcount = Facet.VertexCount;

	uint32_t icount = (vcount - 2) * 3;
	auto alloc = ReserveVertices(vcount, icount);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		for (uint32_t i = 0; i < vcount; i++)
		{
			vec3 point = pts[i];
			float u = dot(Facet.MapCoords.XAxis, point);
			float v = dot(Facet.MapCoords.YAxis, point);

			vptr->Flags = flags;
			vptr->Position.x = point.x;
			vptr->Position.y = point.y;
			vptr->Position.z = point.z;
			vptr->TexCoord.s = (u - UPan) * UMult;
			vptr->TexCoord.t = (v - VPan) * VMult;
			vptr->TexCoord2.s = (u - LMUPan) * LMUMult;
			vptr->TexCoord2.t = (v - LMVPan) * LMVMult;
			vptr->TexCoord3.s = (u - MacroUPan) * MacroUMult;
			vptr->TexCoord3.t = (v - MacroVPan) * MacroVMult;
			vptr->TexCoord4.s = (u - DetailUPan) * DetailUMult;
			vptr->TexCoord4.t = (v - DetailVPan) * DetailVMult;
			vptr->Color = color;
			vptr->TextureBinds = textureBinds;
			vptr++;
		}

		for (uint32_t i = vpos + 2; i < vpos + vcount; i++)
		{
			*(iptr++) = vpos;
			*(iptr++) = i - 1;
			*(iptr++) = i;
		}

		UseVertices(vcount, icount);
	}

	Stats.ComplexSurfaces++;
}

void VulkanRenderDevice::DrawGouraudPolygon(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags)
{
	StatClock drawClock(DrawTimes.Polygons, TimeDraws);
	if (NumPts < 3) return; // This can apparently happen!!

	PolyFlags = ApplyPrecedenceRules(PolyFlags);

	SetPipeline(RenderPasses->GetPipeline(PolyFlags));

	CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	ivec4 textureBinds = GetTextureIndexes(PolyFlags, tex);

	float UMult = GetUMult(Info);
	float VMult = GetVMult(Info);
	int flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;

	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && LightMode == 2) flags |= 32;

	auto alloc = ReserveVertices(NumPts, (NumPts - 2) * 3);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		WriteGouraudVertices(vptr, Pts, NumPts, PolyFlags, flags, UMult, VMult, textureBinds);

		uint32_t vstart = vpos;
		uint32_t vcount = NumPts;
		for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
		{
			*(iptr++) = vstart;
			*(iptr++) = i - 1;
			*(iptr++) = i;
		}

		UseVertices(NumPts, (NumPts - 2) * 3);
	}

	Stats.GouraudPolygons++;
}

void VulkanRenderDevice::WriteGouraudVertices(SceneVertex* vptr, const GouraudVertex* Pts, int NumPts, uint32_t PolyFlags, int flags, float UMult, float VMult, ivec4 textureBinds)
{
	if (PolyFlags & PF_Modulated)
	{
		SceneVertex* vertex = vptr;
		for (int i = 0; i < NumPts; i++)
		{
			const GouraudVertex* P = &Pts[i];
			vertex->Flags = flags;
			vertex->Position.x = P->Point.x;
			vertex->Position.y = P->Point.y;
			vertex->Position.z = P->Point.z;
			vertex->TexCoord.s = P->UV.x * UMult;
			vertex->TexCoord.t = P->UV.y * VMult;
			vertex->TexCoord2.s = P->Fog.x;
			vertex->TexCoord2.t = P->Fog.y;
			vertex->TexCoord3.s = P->Fog.z;
			vertex->TexCoord3.t = P->Fog.w;
			vertex->TexCoord4.s = 0.0f;
			vertex->TexCoord4.t = 0.0f;
			vertex->Color.r = 1.0f;
			vertex->Color.g = 1.0f;
			vertex->Color.b = 1.0f;
			vertex->Color.a = 1.0f;
			vertex->TextureBinds = textureBinds;
			vertex++;
		}
	}
	else
	{
		SceneVertex* vertex = vptr;
		for (int i = 0; i < NumPts; i++)
		{
			const GouraudVertex* P = &Pts[i];
			vertex->Flags = flags;
			vertex->Position.x = P->Point.x;
			vertex->Position.y = P->Point.y;
			vertex->Position.z = P->Point.z;
			vertex->TexCoord.s = P->UV.x * UMult;
			vertex->TexCoord.t = P->UV.y * VMult;
			vertex->TexCoord2.s = P->Fog.x;
			vertex->TexCoord2.t = P->Fog.y;
			vertex->TexCoord3.s = P->Fog.z;
			vertex->TexCoord3.t = P->Fog.w;
			vertex->TexCoord4.s = 0.0f;
			vertex->TexCoord4.t = 0.0f;
			vertex->Color.r = P->Light.x;
			vertex->Color.g = P->Light.y;
			vertex->Color.b = P->Light.z;
			vertex->Color.a = 1.0f;
			vertex->TextureBinds = textureBinds;
			vertex++;
		}
	}
}

void VulkanRenderDevice::DrawGouraudTriangles(SceneNode* Frame, TextureInfo& Info, const GouraudVertex* Pts, int NumTris, uint32_t PolyFlags)
{
	StatClock drawClock(DrawTimes.Polygons, TimeDraws);
	if (NumTris <= 0) return;

	// What DrawGouraudPolygon does for each triangle, with the pipeline, the
	// texture and its binding looked up once for all of them
	uint32_t requestedFlags = PolyFlags;
	PolyFlags = ApplyPrecedenceRules(PolyFlags);

	SetPipeline(RenderPasses->GetPipeline(PolyFlags));

	CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	ivec4 textureBinds = GetTextureIndexes(PolyFlags, tex);

	float UMult = GetUMult(Info);
	float VMult = GetVMult(Info);
	int flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;

	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && LightMode == 2) flags |= 32;

	int NumPts = NumTris * 3;
	auto alloc = ReserveVertices(NumPts, NumPts);
	if (!alloc.vptr)
	{
		// More than the whole buffer: one triangle at a time, as before
		RenderDevice::DrawGouraudTriangles(Frame, Info, Pts, NumTris, requestedFlags);
		return;
	}

	WriteGouraudVertices(alloc.vptr, Pts, NumPts, PolyFlags, flags, UMult, VMult, textureBinds);

	uint32_t* iptr = alloc.iptr;
	for (uint32_t i = 0; i < (uint32_t)NumPts; i++)
		*(iptr++) = alloc.vpos + i;

	UseVertices(NumPts, NumPts);

	Stats.GouraudPolygons += NumTris;
}

void VulkanRenderDevice::DrawTile(SceneNode* Frame, TextureInfo& Info, float X, float Y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 Color, vec4 Fog, uint32_t PolyFlags)
{
	StatClock drawClock(DrawTimes.Tiles, TimeDraws);
	PolyFlags = ApplyPrecedenceRules(TileFlags(Info, PolyFlags));

	if (PolyFlags == PF_SubpixelFont)
	{
		DrawBatch(Commands->GetDrawCommands());
		Batch.BlendConstants[0] = Color.x;
		Batch.BlendConstants[1] = Color.y;
		Batch.BlendConstants[2] = Color.z;
		Batch.BlendConstants[3] = Color.w;
		Color = vec4(1.0f);
	}

	CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	float UMult = tex ? GetUMult(Info) : 0.0f;
	float VMult = tex ? GetVMult(Info) : 0.0f;
	float u0 = U * UMult;
	float v0 = V * VMult;
	float u1 = (U + UL) * UMult;
	float v1 = (V + VL) * VMult;
	bool clamp = (u0 >= 0.0f && u1 <= 1.00001f && v0 >= 0.0f && v1 <= 1.00001f);

	SetPipeline(RenderPasses->GetPipeline(PolyFlags));
	ivec4 textureBinds = GetTextureIndexes(PolyFlags, tex, clamp);

	float r, g, b, a;
	if (PolyFlags & PF_Modulated)
	{
		r = 1.0f;
		g = 1.0f;
		b = 1.0f;
	}
	else
	{
		r = Color.x;
		g = Color.y;
		b = Color.z;
	}
	a = 1.0f;

	if (Textures->Scene->Multisample > 1)
	{
		XL = std::floor(X + XL + 0.5f);
		YL = std::floor(Y + YL + 0.5f);
		X = std::floor(X + 0.5f);
		Y = std::floor(Y + 0.5f);
		XL = XL - X;
		YL = YL - Y;
	}

	auto alloc = ReserveVertices(4, 6);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		vptr[0] = { 0, vec3(RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y - Frame->FY2),      Z), vec2(u0, v0), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), textureBinds };
		vptr[1] = { 0, vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y - Frame->FY2),      Z), vec2(u1, v0), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), textureBinds };
		vptr[2] = { 0, vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z), vec2(u1, v1), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), textureBinds };
		vptr[3] = { 0, vec3(RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y + YL - Frame->FY2), Z), vec2(u0, v1), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), textureBinds };

		iptr[0] = vpos;
		iptr[1] = vpos + 1;
		iptr[2] = vpos + 2;
		iptr[3] = vpos;
		iptr[4] = vpos + 2;
		iptr[5] = vpos + 3;

		UseVertices(4, 6);
	}

	Stats.Tiles++;
}

// A line's colour goes under the present's gamma with the rest of the
// frame, as D3DDrv's lines go through its gamma ramp (d3ddrv-dll.md, gamma)
void VulkanRenderDevice::Draw3DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
	bool occlude = !!(LineFlags & LINE_DepthCued);
	SetPipeline(RenderPasses->GetLinePipeline(occlude));
	ivec4 textureBinds = GetTextureIndexes(PF_Highlighted, nullptr);
	vec4 color = vec4(Color.x, Color.y, Color.z, 1.0f);

	auto alloc = ReserveVertices(2, 2);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		vptr[0] = { 0, vec3(P1.x, P1.y, P1.z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };
		vptr[1] = { 0, vec3(P2.x, P2.y, P2.z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };

		iptr[0] = vpos;
		iptr[1] = vpos + 1;

		UseVertices(2, 2);
	}
}

void VulkanRenderDevice::Draw2DLine(SceneNode* Frame, vec4 Color, uint32_t LineFlags, vec3 P1, vec3 P2)
{
	bool occlude = !!(LineFlags & LINE_DepthCued);
	SetPipeline(RenderPasses->GetLinePipeline(occlude));
	ivec4 textureBinds = GetTextureIndexes(PF_Highlighted, nullptr);
	vec4 color = vec4(Color.x, Color.y, Color.z, 1.0f);

	auto alloc = ReserveVertices(2, 2);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		vptr[0] = { 0, vec3(RFX2 * P1.z * (P1.x - Frame->FX2), RFY2 * P1.z * (P1.y - Frame->FY2), P1.z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };
		vptr[1] = { 0, vec3(RFX2 * P2.z * (P2.x - Frame->FX2), RFY2 * P2.z * (P2.y - Frame->FY2), P2.z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };

		iptr[0] = vpos;
		iptr[1] = vpos + 1;

		UseVertices(2, 2);
	}
}

void VulkanRenderDevice::ClearZ()
{
	DrawBatch(Commands->GetDrawCommands());

	VkClearAttachment attachment = {};
	VkClearRect rect = {};
	attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	attachment.clearValue.depthStencil.depth = 1.0f;
	rect.layerCount = 1;
	rect.rect.extent.width = Textures->Scene->Width;
	rect.rect.extent.height = Textures->Scene->Height;
	Commands->GetDrawCommands()->clearAttachments(1, &attachment, 1, &rect);
}

void VulkanRenderDevice::ReadPixels(TextureColor* Pixels)
{
	auto cmdbuffer = Commands->GetDrawCommands();

	DrawBatch(cmdbuffer);

	if (GammaCorrectScreenshots)
	{
		PresentPushConstants pushconstants = GetPresentPushConstants();

		bool ActiveHdr = false; // (Commands->SwapChain->Format().colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) ? 1 : 0;

		// Select present shader based on what the user is actually using
		int presentShader = 0;
		if (ActiveHdr) presentShader |= 1;
		if (GammaMode == 1) presentShader |= 2;
		if (pushconstants.Contrast != 1.0f || pushconstants.Saturation != 1.0f) presentShader |= 4;

		VkViewport viewport = {};
		viewport.width = (float)Textures->Scene->Width;
		viewport.height = (float)Textures->Scene->Height;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.extent.width = Textures->Scene->Width;
		scissor.extent.height = Textures->Scene->Height;

		auto cmdbuffer = Commands->GetDrawCommands();

		VkAccessFlags srcColorAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		VkAccessFlags dstColorAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		PipelineBarrier()
			.AddImage(Textures->Scene->PPImage[1].get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, srcColorAccess, dstColorAccess)
			.Execute(cmdbuffer, srcStages, dstStages);

		RenderPassBegin()
			.RenderPass(RenderPasses->Postprocess.RenderPass.get())
			.Framebuffer(Framebuffers->PPImageFB[1].get())
			.RenderArea(0, 0, Textures->Scene->Width, Textures->Scene->Height)
			.AddClearColor(0.0f, 0.0f, 0.0f, 1.0f)
			.Execute(cmdbuffer);

		cmdbuffer->setViewport(0, 1, &viewport);
		cmdbuffer->setScissor(0, 1, &scissor);
		cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->Present.ScreenshotPipeline[presentShader].get());
		cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->Present.PipelineLayout.get(), 0, DescriptorSets->GetPresentSet());
		cmdbuffer->pushConstants(RenderPasses->Present.PipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PresentPushConstants), &pushconstants);
		cmdbuffer->draw(6, 1, 0, 0);

		cmdbuffer->endRenderPass();

		PipelineBarrier()
			.AddImage(Textures->Scene->PPImage[1].get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_SHADER_READ_BIT)
			.Execute(cmdbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}

	// The frame's ten bits a channel copied as they are and made bytes on the
	// CPU, as the GL device's are (TenBitRowToBytes): a blit to BGRA8 rounds
	// to the nearest byte, which reads the darks under the buffer's levels
	auto srcimage = Textures->Scene->PPImage[GammaCorrectScreenshots ? 1 : 0].get();

	int w, h;
	LastFrameSize(w, h);

	// Staging buffer for download
	auto staging = BufferBuilder()
		.Size(w * h * 4)
		.Usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU)
		.DebugName("ReadPixelsStaging")
		.Create(Device.get());

	PipelineBarrier()
		.AddImage(srcimage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	// Copy from image to buffer
	VkBufferImageCopy region = {};
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	region.imageExtent.depth = 1;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cmdbuffer->copyImageToBuffer(srcimage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging->buffer, 1, &region);

	PipelineBarrier()
		.AddImage(srcimage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	// The copy made visible to the host: the fence's wait covers the device's
	// own accesses alone, and VMA may give the staging buffer cached memory
	// that is not coherent (some PowerVR and Adreno drivers), which the read
	// then invalidates
	PipelineBarrier()
		.AddMemory(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

	// Submit command buffers and wait for device to finish the work
	SubmitAndWait(false, 0, 0);

	const uint32_t* pixels = (const uint32_t*)staging->Map(0, w * h * 4);
	staging->Invalidate(0, (size_t)w * h * 4);
	for (int y = 0; y < h; y++)
		TenBitRowToBytes(pixels + (size_t)y * w, Pixels + (size_t)y * w, w, y, GammaCorrectScreenshots);
	staging->Unmap();
}

void VulkanRenderDevice::LastFrameSize(int& width, int& height) const
{
	if (Textures->Scene)
	{
		width = Textures->Scene->Width;
		height = Textures->Scene->Height;
	}
	else
	{
		RenderDevice::LastFrameSize(width, height);
	}
}

void VulkanRenderDevice::EndFlash()
{
	if (FlashScale != vec4(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != vec4(0.0f, 0.0f, 0.0f, 0.0f))
	{
		vec4 color(FlashFog.x, FlashFog.y, FlashFog.z, 1.0f - std::min(FlashScale.x * 2.0f, 1.0f));
		vec2 zero2(0.0f);

		DrawBatch(Commands->GetDrawCommands());
		pushconstants.objectToProjection = mat4::identity();
		pushconstants.nearClip = vec4(0.0f, 0.0f, 0.0f, 0.0f);

		SetPipeline(RenderPasses->GetEndFlashPipeline());

		// Nothing is sampled here, but a set still has to be bound for the batch.
		ivec4 zero4 = GetTextureIndexes(PF_Highlighted, nullptr);

		auto alloc = ReserveVertices(4, 6);
		if (alloc.vptr)
		{
			SceneVertex* vptr = alloc.vptr;
			uint32_t* iptr = alloc.iptr;
			uint32_t vpos = alloc.vpos;

			vptr[0] = { 0, vec3(-1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };
			vptr[1] = { 0, vec3(1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };
			vptr[2] = { 0, vec3(1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };
			vptr[3] = { 0, vec3(-1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };

			iptr[0] = vpos;
			iptr[1] = vpos + 1;
			iptr[2] = vpos + 2;
			iptr[3] = vpos;
			iptr[4] = vpos + 2;
			iptr[5] = vpos + 3;

			UseVertices(4, 6);
		}

		DrawBatch(Commands->GetDrawCommands());
		if (CurrentFrame)
			SetSceneNode(CurrentFrame);
	}
}

void VulkanRenderDevice::SetSceneNode(SceneNode* Frame)
{
	auto commands = Commands->GetDrawCommands();
	DrawBatch(commands);

	CurrentFrame = Frame;
	Aspect = Frame->FY / Frame->FX;
	RProjZ = (float)std::tan(radians(Frame->FovAngle) * 0.5);
	RFX2 = 2.0f * RProjZ / Frame->FX;
	RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;

	viewportdesc = {};
	viewportdesc.x = (float)Frame->XB;
	viewportdesc.y = (float)Frame->YB;
	viewportdesc.width = (float)Frame->X;
	viewportdesc.height = (float)Frame->Y;
	viewportdesc.minDepth = 0.1f;
	viewportdesc.maxDepth = 1.0f;
	commands->setViewport(0, 1, &viewportdesc);

	pushconstants.objectToProjection = PolygonShift(Frame) * mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);

	// TBD; do this or do like UE1 does and do the transform on the CPU?
	// maybe optionally do one or the other? transform on CPU can be super slow --Xaleros
	pushconstants.objectToProjection = pushconstants.objectToProjection * Frame->WorldToView * Frame->ObjectToWorld;

	// A mirror's frame is clipped at the mirror's plane, its NearClip, in the
	// object's space, the view's turned by the transpose: dot(plane, M p) =
	// dot(transpose(M) plane, p). So the vertex needs no view matrix, and the
	// push constants fit 128 bytes. Any other frame's plane is (0, 0, 0, 1),
	// which clips nothing: the projection's near plane, view depth 1, clips
	// it, as every device clips there (RenderPassManager).
	bool mirror = Frame->NearClip != vec4(0.0f, 0.0f, 1.0f, -1.0f);
	pushconstants.nearClip = mirror ? mat4::transpose(Frame->WorldToView * Frame->ObjectToWorld) * Frame->NearClip : vec4(0.0f, 0.0f, 0.0f, 1.0f);
	RenderPasses->SetMirrorFrame(mirror);
}

void VulkanRenderDevice::PrecacheTexture(TextureInfo& Info, uint32_t PolyFlags)
{
	PolyFlags = ApplyPrecedenceRules(PolyFlags);
	Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
}

void VulkanRenderDevice::ClearTextureCache()
{
	Commands->WaitForFrame();
	Batch.DescriptorSet = nullptr;
	DescriptorSets->ClearCache();
	Textures->ClearCache();
	Uploads->ClearCache();
}

void VulkanRenderDevice::BlitSceneToPostprocess()
{
	auto buffers = Textures->Scene.get();
	auto cmdbuffer = Commands->GetDrawCommands();

	PipelineBarrier barrer0;
	VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrer0.AddImage(
		buffers->ColorBuffer.get(),
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_TRANSFER_READ_BIT);
	barrer0.AddImage(
		buffers->PPImage[0].get(),
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT);
	barrer0.Execute(
		Commands->GetDrawCommands(),
		srcStageMask,
		VK_PIPELINE_STAGE_TRANSFER_BIT);

	if (buffers->SceneSamples != VK_SAMPLE_COUNT_1_BIT)
	{
		VkImageResolve resolve = {};
		resolve.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		resolve.srcSubresource.layerCount = 1;
		resolve.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		resolve.dstSubresource.layerCount = 1;
		resolve.extent = { (uint32_t)buffers->ColorBuffer->width, (uint32_t)buffers->ColorBuffer->height, 1 };
		cmdbuffer->resolveImage(
			buffers->ColorBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			buffers->PPImage[0]->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &resolve);
	}
	else
	{
		auto colorBuffer = buffers->ColorBuffer.get();
		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { colorBuffer->width, colorBuffer->height, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { colorBuffer->width, colorBuffer->height, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.layerCount = 1;
		cmdbuffer->blitImage(
			colorBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			buffers->PPImage[0]->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit, VK_FILTER_NEAREST);
	}

	PipelineBarrier barrier1;
	barrier1.AddImage(
		buffers->PPImage[0].get(),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT);
	barrier1.Execute(
		Commands->GetDrawCommands(),
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

// The world's frame copied out and its bloom taken into the levels, then the
// scene's pass again, its colour and depth as the world left them, the bloom
// added into its colour at its samples, for the windows drawn over it:
// the windows drawn after it stay sharp, and the screen flash, the loading
// screen's dimming and the menus' masks, drawn over the world after it,
// cover its bloom as they cover the world (D3DDrv's EndFlash blends over all
// the frame, D3DDrv.dll 0x10007c90), where added at Unlock it lay over them
// whole
void VulkanRenderDevice::TakeBloom()
{
	if (!Bloom || !IsLocked)
		return;

	auto cmdbuffer = Commands->GetDrawCommands();
	DrawBatch(cmdbuffer);
	cmdbuffer->endRenderPass();
	BlitSceneToPostprocess();
	RunBloomPass();

	VkAccessFlags colorAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	VkAccessFlags depthAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	VkPipelineStageFlags depthStages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	PipelineBarrier()
		.AddImage(Textures->Scene->ColorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, colorAccess)
		.AddImage(Textures->Scene->DepthBuffer.get(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depthAccess, depthAccess, VK_IMAGE_ASPECT_DEPTH_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT | depthStages, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | depthStages);

	RenderPassBegin()
		.RenderPass(RenderPasses->Scene.RenderPassContinue.get())
		.Framebuffer(Framebuffers->SceneFramebuffer.get())
		.RenderArea(0, 0, Textures->Scene->Width, Textures->Scene->Height)
		.Execute(cmdbuffer);

	// The first level over the whole scene, added, no depth tested or written
	VkViewport viewport = {};
	viewport.width = (float)Textures->Scene->Width;
	viewport.height = (float)Textures->Scene->Height;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor = {};
	scissor.extent.width = Textures->Scene->Width;
	scissor.extent.height = Textures->Scene->Height;
	BloomPushConstants bloomconstants = {};
	cmdbuffer->setViewport(0, 1, &viewport);
	cmdbuffer->setScissor(0, 1, &scissor);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->Bloom.CombineScene.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->Bloom.PipelineLayout.get(), 0, DescriptorSets->GetBloomVTextureSet(0));
	cmdbuffer->pushConstants(RenderPasses->Bloom.PipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BloomPushConstants), &bloomconstants);
	cmdbuffer->draw(6, 1, 0, 0);

	VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
	VkDeviceSize offsets[] = { 0 };
	cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
	cmdbuffer->bindIndexBuffer(Buffers->SceneIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
	cmdbuffer->setViewport(0, 1, &viewportdesc);
	// The bloom's pipelines fix their own depth bias, which Vulkan forgets
	// the dynamic one for
	DepthBiasSet = DepthBiasUnknown;
}

void VulkanRenderDevice::RunBloomPass()
{
	float blurAmount = 0.6f + BloomAmount * (1.9f / 255.0f);
	BloomPushConstants pushconstants;
	ComputeBlurSamples(7, blurAmount, pushconstants.SampleWeights);

	auto cmdbuffer = Commands->GetDrawCommands();

	PipelineBarrier()
		.AddImage(Textures->Scene->BloomBlurLevels[0].VTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	// Extract overbright pixels that we want to bloom:
	BloomStep(cmdbuffer,
		RenderPasses->Bloom.Extract.get(),
		DescriptorSets->GetBloomPPImageSet(),
		Framebuffers->BloomBlurLevels[0].VTextureFB.get(),
		Textures->Scene->BloomBlurLevels[0].Width,
		Textures->Scene->BloomBlurLevels[0].Height,
		pushconstants);

	// Blur and downscale:
	for (int i = 0; i < NumBloomLevels - 1; i++)
	{
		PipelineBarrier()
			.AddImage(Textures->Scene->BloomBlurLevels[i].HTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
			.AddImage(Textures->Scene->BloomBlurLevels[i].VTexture.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_SHADER_READ_BIT)
			.Execute(cmdbuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		// Gaussian blur
		BloomStep(cmdbuffer,
			RenderPasses->Bloom.BlurVertical.get(),
			DescriptorSets->GetBloomVTextureSet(i),
			Framebuffers->BloomBlurLevels[i].HTextureFB.get(),
			Textures->Scene->BloomBlurLevels[i].Width,
			Textures->Scene->BloomBlurLevels[i].Height,
			pushconstants);

		PipelineBarrier()
			.AddImage(Textures->Scene->BloomBlurLevels[i].VTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
			.AddImage(Textures->Scene->BloomBlurLevels[i].HTexture.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_SHADER_READ_BIT)
			.Execute(cmdbuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		BloomStep(cmdbuffer,
			RenderPasses->Bloom.BlurHorizontal.get(),
			DescriptorSets->GetBloomHTextureSet(i),
			Framebuffers->BloomBlurLevels[i].VTextureFB.get(),
			Textures->Scene->BloomBlurLevels[i].Width,
			Textures->Scene->BloomBlurLevels[i].Height,
			pushconstants);

		PipelineBarrier()
			.AddImage(Textures->Scene->BloomBlurLevels[i + 1].VTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
			.AddImage(Textures->Scene->BloomBlurLevels[i].VTexture.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_SHADER_READ_BIT)
			.Execute(cmdbuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		// Linear downscale
		BloomStep(cmdbuffer,
			RenderPasses->Bloom.Scale.get(),
			DescriptorSets->GetBloomVTextureSet(i),
			Framebuffers->BloomBlurLevels[i + 1].VTextureFB.get(),
			Textures->Scene->BloomBlurLevels[i + 1].Width,
			Textures->Scene->BloomBlurLevels[i + 1].Height,
			pushconstants);
	}

	// Blur and upscale:
	for (int i = NumBloomLevels - 1; i >= 0; i--)
	{
		PipelineBarrier()
			.AddImage(Textures->Scene->BloomBlurLevels[i].HTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
			.AddImage(Textures->Scene->BloomBlurLevels[i].VTexture.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_SHADER_READ_BIT)
			.Execute(cmdbuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		// Gaussian blur
		BloomStep(cmdbuffer,
			RenderPasses->Bloom.BlurVertical.get(),
			DescriptorSets->GetBloomVTextureSet(i),
			Framebuffers->BloomBlurLevels[i].HTextureFB.get(),
			Textures->Scene->BloomBlurLevels[i].Width,
			Textures->Scene->BloomBlurLevels[i].Height,
			pushconstants);

		PipelineBarrier()
			.AddImage(Textures->Scene->BloomBlurLevels[i].VTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
			.AddImage(Textures->Scene->BloomBlurLevels[i].HTexture.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_SHADER_READ_BIT)
			.Execute(cmdbuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		BloomStep(cmdbuffer,
			RenderPasses->Bloom.BlurHorizontal.get(),
			DescriptorSets->GetBloomHTextureSet(i),
			Framebuffers->BloomBlurLevels[i].VTextureFB.get(),
			Textures->Scene->BloomBlurLevels[i].Width,
			Textures->Scene->BloomBlurLevels[i].Height,
			pushconstants);

		// Linear upscale
		if (i > 0)
		{
			PipelineBarrier()
				.AddImage(Textures->Scene->BloomBlurLevels[i - 1].VTexture.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
				.AddImage(Textures->Scene->BloomBlurLevels[i].VTexture.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_SHADER_READ_BIT)
				.Execute(cmdbuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

			BloomStep(cmdbuffer,
				RenderPasses->Bloom.Scale.get(),
				DescriptorSets->GetBloomVTextureSet(i),
				Framebuffers->BloomBlurLevels[i - 1].VTextureFB.get(),
				Textures->Scene->BloomBlurLevels[i - 1].Width,
				Textures->Scene->BloomBlurLevels[i - 1].Height,
				pushconstants);
		}
	}

	PipelineBarrier()
		.AddImage(Textures->Scene->BloomBlurLevels[0].VTexture.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void VulkanRenderDevice::BloomStep(VulkanCommandBuffer* cmdbuffer, VulkanPipeline* pipeline, VulkanDescriptorSet* input, VulkanFramebuffer* output, int width, int height, const BloomPushConstants& pushconstants)
{
	RenderPassBegin()
		.RenderPass(pipeline != RenderPasses->Bloom.Combine.get() ? RenderPasses->Postprocess.RenderPass.get() : RenderPasses->Postprocess.RenderPassCombine.get())
		.Framebuffer(output)
		.RenderArea(0, 0, width, height)
		.AddClearColor(0.0f, 0.0f, 0.0f, 1.0f)
		.Execute(cmdbuffer);

	VkViewport viewport = {};
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.extent.width = width;
	scissor.extent.height = height;

	cmdbuffer->setViewport(0, 1, &viewport);
	cmdbuffer->setScissor(0, 1, &scissor);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->Bloom.PipelineLayout.get(), 0, input);
	cmdbuffer->pushConstants(RenderPasses->Bloom.PipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BloomPushConstants), &pushconstants);
	cmdbuffer->draw(6, 1, 0, 0);

	cmdbuffer->endRenderPass();
}

float VulkanRenderDevice::ComputeBlurGaussian(float n, float theta) // theta = Blur Amount
{
	return (float)((1.0f / sqrt(2 * 3.14159265359f * theta)) * expf(-(n * n) / (2.0f * theta * theta)));
}

void VulkanRenderDevice::ComputeBlurSamples(int sampleCount, float blurAmount, float* sampleWeights)
{
	sampleWeights[0] = ComputeBlurGaussian(0, blurAmount);

	float totalWeights = sampleWeights[0];

	for (int i = 0; i < sampleCount / 2; i++)
	{
		float weight = ComputeBlurGaussian(i + 1.0f, blurAmount);

		sampleWeights[i * 2 + 1] = weight;
		sampleWeights[i * 2 + 2] = weight;

		totalWeights += weight * 2;
	}

	for (int i = 0; i < sampleCount; i++)
	{
		sampleWeights[i] /= totalWeights;
	}
}

PresentPushConstants VulkanRenderDevice::GetPresentPushConstants()
{
	PresentPushConstants pushconstants = {};
	pushconstants.HdrScale = 0.8f + HdrScale * (3.0f / 255.0f);
	float brightness = clamp(Brightness * GammaScale, 0.05f, 2.99f);

	if (GammaMode == 0)
	{
		float invGammaRed = 1.0f / std::max(brightness + GammaOffset + GammaOffsetRed, 0.001f);
		float invGammaGreen = 1.0f / std::max(brightness + GammaOffset + GammaOffsetGreen, 0.001f);
		float invGammaBlue = 1.0f / std::max(brightness + GammaOffset + GammaOffsetBlue, 0.001f);
		pushconstants.GammaCorrection = vec4(invGammaRed, invGammaGreen, invGammaBlue, 0.0f);
	}
	else
	{
		float invGammaRed = 1.0f / std::max(GammaOffset + GammaOffsetRed + 1.0f, 0.001f);
		float invGammaGreen = 1.0f / std::max(GammaOffset + GammaOffsetGreen + 1.0f, 0.001f);
		float invGammaBlue = 1.0f / std::max(GammaOffset + GammaOffsetBlue + 1.0f, 0.001f);
		pushconstants.GammaCorrection = vec4(invGammaRed, invGammaGreen, invGammaBlue, brightness);
	}

	pushconstants.Contrast = Contrast;
	pushconstants.Saturation = Saturation;

	return pushconstants;
}

void VulkanRenderDevice::DrawPresentTexture(int width, int height)
{
	// The scene's size: the image scales from it to the window's
	int vpWidth = GetRenderWidth();
	int vpHeight = GetRenderHeight();

	PresentPushConstants pushconstants = GetPresentPushConstants();

	bool ActiveHdr = (Commands->SwapChain->Format().colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) ? 1 : 0;

	// Select present shader based on what the user is actually using
	int presentShader = 0;
	if (ActiveHdr) presentShader |= 1;
	if (GammaMode == 1) presentShader |= 2;
	if (pushconstants.Contrast != 1.0f || pushconstants.Saturation != 1.0f) presentShader |= 4;

	// Laid out for the swapchain's extent, which the present render pass
	// covers, not the size asked for: the surface's range holds the extent,
	// and a SUBOPTIMAL swapchain is not remade, so the two differed for a
	// frame of a resize (the picture cut off, or in the corner)
	width = Commands->SwapChain->Width();
	height = Commands->SwapChain->Height();
	float scale = std::min(width / (float)vpWidth, height / (float)vpHeight);
	int letterboxWidth = (int)std::round(vpWidth * scale);
	int letterboxHeight = (int)std::round(vpHeight * scale);
	int letterboxX = (width - letterboxWidth) / 2;
	int letterboxY = (height - letterboxHeight) / 2;

	VkViewport viewport = {};
	viewport.x = (float)letterboxX;
	viewport.y = (float)letterboxY;
	viewport.width = (float)letterboxWidth;
	viewport.height = (float)letterboxHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset.x = letterboxX;
	scissor.offset.y = letterboxY;
	scissor.extent.width = letterboxWidth;
	scissor.extent.height = letterboxHeight;

	auto cmdbuffer = Commands->GetDrawCommands();

	PipelineBarrier()
		.AddImage(Commands->SwapChain->GetImage(Commands->PresentImageIndex), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	RenderPassBegin()
		.RenderPass(RenderPasses->Present.RenderPass.get())
		.Framebuffer(Framebuffers->GetSwapChainFramebuffer())
		.RenderArea(0, 0, Commands->SwapChain->Width(), Commands->SwapChain->Height())
		.AddClearColor(0.0f, 0.0f, 0.0f, 1.0f)
		.Execute(cmdbuffer);
	cmdbuffer->setViewport(0, 1, &viewport);
	cmdbuffer->setScissor(0, 1, &scissor);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->Present.Pipeline[presentShader].get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->Present.PipelineLayout.get(), 0, DescriptorSets->GetPresentSet());
	cmdbuffer->pushConstants(RenderPasses->Present.PipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PresentPushConstants), &pushconstants);
	cmdbuffer->draw(6, 1, 0, 0);
	cmdbuffer->endRenderPass();

	PipelineBarrier()
		.AddImage(Commands->SwapChain->GetImage(Commands->PresentImageIndex), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

// Held within the GPU's maxSamplerLodBias, which a sampler may not pass
void VulkanRenderDevice::SetLODBias(float bias)
{
	float most = Device->PhysicalDevice.Properties.Properties.limits.maxSamplerLodBias;
	bias = std::clamp(bias, -most, most);
	if (bias != LODBias)
	{
		LODBias = bias;
		LODBiasChanged = true;
	}
}

RenderDevice::FrameDrawStats VulkanRenderDevice::GetFrameStats() const
{
	FrameDrawStats stats;
	stats.Surfaces = Stats.ComplexSurfaces;
	stats.Polygons = Stats.GouraudPolygons;
	stats.Tiles = Stats.Tiles;
	stats.Flushes = Stats.DrawCalls;
	stats.SurfaceTime = DrawTimes.Surfaces;
	stats.PolygonTime = DrawTimes.Polygons;
	stats.TileTime = DrawTimes.Tiles;
	Textures->AddPoolStats(stats.Pools);
	return stats;
}
