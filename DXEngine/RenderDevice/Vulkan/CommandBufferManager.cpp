
#include "Precomp.h"
#include "CommandBufferManager.h"
#include "VulkanRenderDevice.h"
#include "Utils/Logger.h"
#include <dxgpu/vulkanswapchain.h>
#include <dxgpu/vulkansurface.h>
#include <dxwidgets/core/widget.h>

CommandBufferManager::CommandBufferManager(VulkanRenderDevice* renderer) : renderer(renderer)
{
	SwapChain = VulkanSwapChainBuilder()
		.Create(renderer->Device.get());

	ImageAvailableSemaphore = SemaphoreBuilder()
		.DebugName("ImageAvailableSemaphore")
		.Create(renderer->Device.get());

	RenderFinishedFence = FenceBuilder()
		.DebugName("RenderFinishedFence")
		.Create(renderer->Device.get());

	TransferSemaphore.reset(new VulkanSemaphore(renderer->Device.get()));

	CommandPool = CommandPoolBuilder()
		.QueueFamily(renderer->Device.get()->GraphicsFamily)
		.DebugName("CommandPool")
		.Create(renderer->Device.get());

	FrameDeleteList = std::make_unique<DeleteList>();
}

CommandBufferManager::~CommandBufferManager()
{
	DeleteFrameObjects();
}

// The frame's fence waited for and reset, each result through the device's
// check: a lost device (a GPU reset, a driver update, an external GPU
// unplugged) ends the game with its name and the device's fault report, as
// the original's critical error ends it (bugs.md D.93)
void CommandBufferManager::WaitAndResetFence()
{
	VulkanDevice* device = renderer->Device.get();
	device->CheckVulkanError(vkWaitForFences(device->device, 1, &RenderFinishedFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max()), "vkWaitForFences failed");
	device->CheckVulkanError(vkResetFences(device->device, 1, &RenderFinishedFence->fence), "vkResetFences failed");
}

void CommandBufferManager::WaitForFrame()
{
	if (!FramePending)
		return;

	WaitAndResetFence();

	FramePending = false;
	PendingDrawCommands.reset();
	PendingTransferCommands.reset();
	DeleteFrameObjects();
}

void CommandBufferManager::WaitForTransfer()
{
	WaitForFrame();
	renderer->Uploads->SubmitUploads();

	if (TransferCommands)
	{
		TransferCommands->end();

		QueueSubmit()
			.AddCommandBuffer(TransferCommands.get())
			.Execute(renderer->Device.get(), renderer->Device.get()->GraphicsQueue, RenderFinishedFence.get());

		WaitAndResetFence();

		TransferCommands.reset();
	}
}

void CommandBufferManager::SubmitCommands(bool present, int presentWidth, int presentHeight, bool wait)
{
	WaitForFrame();
	renderer->Uploads->SubmitUploads();

	// A window of no size (minimized) has nothing to present to
	if (present && (presentWidth <= 0 || presentHeight <= 0))
		present = false;

	if (present)
	{
		if (SwapChain->Lost() || RequestedWidth != presentWidth || RequestedHeight != presentHeight || UsingVsync != renderer->UseVSync || UsingHdr != renderer->Hdr)
			CreateSwapChain(presentWidth, presentHeight);

		PresentImageIndex = SwapChain->AcquireImage(ImageAvailableSemaphore.get());
		if (PresentImageIndex != -1)
		{
			renderer->DrawPresentTexture(presentWidth, presentHeight);
		}
	}

	if (TransferCommands)
	{
		TransferCommands->end();

		QueueSubmit()
			.AddCommandBuffer(TransferCommands.get())
			.AddSignal(TransferSemaphore.get())
			.Execute(renderer->Device.get(), renderer->Device.get()->GraphicsQueue);
	}

	if (DrawCommands)
		DrawCommands->end();

	QueueSubmit submit;
	if (DrawCommands)
	{
		submit.AddCommandBuffer(DrawCommands.get());
	}
	if (TransferCommands)
	{
		submit.AddWait(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TransferSemaphore.get());
	}
	if (present && PresentImageIndex != -1)
	{
		submit.AddWait(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, ImageAvailableSemaphore.get());
		submit.AddSignal(RenderFinishedSemaphores[PresentImageIndex].get());
	}
	submit.Execute(renderer->Device.get(), renderer->Device.get()->GraphicsQueue, RenderFinishedFence.get());

	if (present && PresentImageIndex != -1)
	{
		SwapChain->QueuePresent(PresentImageIndex, RenderFinishedSemaphores[PresentImageIndex].get());
	}

	if (!wait)
	{
		// Leave the frame in flight: the CPU goes on to the next game tick
		// while the GPU draws this one. WaitForFrame collects it before
		// anything touches its buffers, images or descriptors again.
		FramePending = true;
		PendingDrawCommands = std::move(DrawCommands);
		PendingTransferCommands = std::move(TransferCommands);
		return;
	}

	WaitAndResetFence();

	DrawCommands.reset();
	TransferCommands.reset();
	DeleteFrameObjects();
}

void CommandBufferManager::CreateSwapChain(int width, int height, bool logFailure)
{
	// A fence covers a submit, not the present queued after it: the
	// old swapchain and its semaphores may still be in use until the
	// device is idle. A device lost here is said here (bugs.md REN.91)
	renderer->Device->CheckVulkanError(vkDeviceWaitIdle(renderer->Device.get()->device), "vkDeviceWaitIdle failed");

	UsingVsync = renderer->UseVSync;
	UsingHdr = renderer->Hdr;
	RequestedWidth = width;
	RequestedHeight = height;
	renderer->Framebuffers->DestroySwapChainFramebuffers();
	// The surface itself lost (a compositor gone): the swapchain let
	// go, a new surface from the window, the dead one freed; the
	// swapchain was once remade on the dead surface every frame
	if (SwapChain->SurfaceLost())
	{
		SwapChain.reset();
		renderer->Device->Surface = std::make_shared<VulkanSurface>(renderer->Device->Instance, renderer->Viewport->CreateVulkanSurface(renderer->Device->Instance->Instance));
		SwapChain = std::make_shared<VulkanSwapChain>(renderer->Device.get());
	}
	SwapChain->Create(width, height, renderer->UseVSync ? 2 : 3, renderer->UseVSync, renderer->Hdr);
	// The present pass and its pipelines for a swapchain made alone: none
	// made (a surface of no size, a create refused), which the next present
	// tries again, has no images for them, and building them at every
	// frame of it cost 8 pipelines a frame (bugs.md REN.77)
	if (SwapChain->ImageCount() > 0)
		renderer->Framebuffers->CreateSwapChainFramebuffers();

	RenderFinishedSemaphores.clear();
	for (int i = 0; i < SwapChain->ImageCount(); i++)
	{
		RenderFinishedSemaphores.push_back(SemaphoreBuilder()
			.DebugName("RenderFinishedSemaphore")
			.Create(renderer->Device.get()));
	}

	// A create refused is logged once with its result, the window then
	// showing nothing until one is made, where it was silent
	VkResult result = SwapChain->CreateResult();
	if (logFailure && result != VK_SUCCESS && result != LoggedSwapChainResult)
		LogMessage("VulkanDrv: could not make the swapchain (" + VkResultToString(result) + "): nothing presented, tried again at each frame");
	if (result != VK_SUCCESS || SwapChain->ImageCount() > 0)
		LoggedSwapChainResult = result;
}

VulkanCommandBuffer* CommandBufferManager::GetTransferCommands()
{
	if (!TransferCommands)
	{
		WaitForFrame();
		TransferCommands = CommandPool->createBuffer();
		TransferCommands->begin();
	}
	return TransferCommands.get();
}

VulkanCommandBuffer* CommandBufferManager::GetDrawCommands()
{
	if (!DrawCommands)
	{
		WaitForFrame();
		DrawCommands = CommandPool->createBuffer();
		DrawCommands->begin();
	}
	return DrawCommands.get();
}

void CommandBufferManager::DeleteFrameObjects()
{
	FrameDeleteList = std::make_unique<DeleteList>();
}
