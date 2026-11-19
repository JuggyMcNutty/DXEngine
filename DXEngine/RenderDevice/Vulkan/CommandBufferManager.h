#pragma once

#include <dxgpu/vulkanobjects.h>

class VulkanRenderDevice;

class CommandBufferManager
{
public:
	CommandBufferManager(VulkanRenderDevice* renderer);
	~CommandBufferManager();

	void WaitForTransfer();
	void SubmitCommands(bool present, int presentWidth, int presentHeight, bool wait = true);
	// The swapchain made for a size, with the present pass for its images:
	// at the device's start and when a present finds it lost or the size,
	// vsync or HDR changed; a create refused is logged unless the caller
	// reports it (the device's start, which then fails)
	void CreateSwapChain(int width, int height, bool logFailure = true);

	// Waits for a frame SubmitCommands left in flight (wait = false), if there
	// is one. Anything that writes a buffer, image or descriptor the GPU may
	// still be reading calls this first.
	void WaitForFrame();
	VulkanCommandBuffer* GetTransferCommands();
	VulkanCommandBuffer* GetDrawCommands();
	void DeleteFrameObjects();

	struct DeleteList
	{
		std::vector<std::unique_ptr<VulkanImage>> images;
		std::vector<std::unique_ptr<VulkanImageView>> imageViews;
		std::vector<std::unique_ptr<VulkanBuffer>> buffers;
		std::vector<std::unique_ptr<VulkanDescriptorSet>> descriptors;
	};
	std::unique_ptr<DeleteList> FrameDeleteList;

	std::shared_ptr<VulkanSwapChain> SwapChain;
	int PresentImageIndex = -1;
	bool UsingVsync = false;
	bool UsingHdr = false;
	// The size the swapchain was last made for: the surface may clamp its
	// extent to another, which is no reason to make it again
	int RequestedWidth = -1;
	int RequestedHeight = -1;

private:
	void WaitAndResetFence();

	VulkanRenderDevice* renderer = nullptr;

	std::unique_ptr<VulkanSemaphore> ImageAvailableSemaphore;
	std::vector<std::unique_ptr<VulkanSemaphore>> RenderFinishedSemaphores;
	std::unique_ptr<VulkanSemaphore> TransferSemaphore;
	std::unique_ptr<VulkanFence> RenderFinishedFence;
	std::unique_ptr<VulkanCommandPool> CommandPool;
	std::unique_ptr<VulkanCommandBuffer> DrawCommands;
	std::unique_ptr<VulkanCommandBuffer> TransferCommands;

	// The frame in flight: its command buffers live until its fence signals.
	bool FramePending = false;
	std::unique_ptr<VulkanCommandBuffer> PendingDrawCommands;
	std::unique_ptr<VulkanCommandBuffer> PendingTransferCommands;

	// The create's failure last logged, logged again only when it changes or
	// after a swapchain was made
	VkResult LoggedSwapChainResult = VK_SUCCESS;
};
