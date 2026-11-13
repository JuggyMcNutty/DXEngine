#pragma once

#include "RenderDevice/RenderDevice.h"

class CachedTexture
{
public:
	// STAT HARDWARE's: its format, bytes and the frame it was last set in
	TextureCacheStats::Entry Stat;
	std::unique_ptr<VulkanImage> image;
	std::unique_ptr<VulkanImageView> imageView;
	VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	int BindlessIndex[4] = { -1, -1, -1, -1 };
	int RealtimeChangeCount = 0;
	// Colour 0 cleared in it, as the draw that made it was masked
	bool Masked = false;

	std::vector<VkBufferImageCopy> pendingUploads[2];
	bool inPendingUploads = false;
};
