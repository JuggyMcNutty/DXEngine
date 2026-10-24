
#include "Precomp.h"
#include "UploadManager.h"
#include "VulkanRenderDevice.h"
#include "CachedTexture.h"
#include <dxgpu/vulkanbuilders.h>

UploadManager::UploadManager(VulkanRenderDevice* renderer) : renderer(renderer)
{
}

UploadManager::~UploadManager()
{
}

// The textures go, and their uploads not yet copied with them: the staging
// buffer is free again.
void UploadManager::ClearCache()
{
	PendingUploads.clear();
	UploadBufferPos = 0;
}

bool UploadManager::SupportsTextureFormat(TextureFormat Format) const
{
	return TextureUploader::GetUploader(Format, renderer->Device->PhysicalDevice.Device);
}

void UploadManager::UploadTexture(CachedTexture* tex, const TextureInfo& Info, bool masked)
{
	// The upload buffer is reused from offset 0 after every submit
	renderer->Commands->WaitForFrame();

	// The image the size of its top mip's data, whatever the texture's
	// USize and VSize say; none, or too big, a white texel.
	bool hasMips = Info.NumMips >= 1 && Info.Mips;
	int width = hasMips ? Info.Mips[0].Width : 0;
	int height = hasMips ? Info.Mips[0].Height : 0;
	int mipcount = Info.NumMips;

	TextureUploader* uploader = TextureUploader::GetUploader(Info.Format, renderer->Device->PhysicalDevice.Device);

	uint32_t maxDimension = renderer->Device.get()->PhysicalDevice.Properties.Properties.limits.maxImageDimension2D;
	if (!uploader || width < 1 || height < 1 || (uint32_t)width > maxDimension || (uint32_t)height > maxDimension)
	{
		width = 1;
		height = 1;
		mipcount = 1;
		uploader = nullptr;
	}

	VkFormat format = uploader ? uploader->GetVkFormat() : VK_FORMAT_R8G8B8A8_UNORM;

	if (!tex->image)
	{
		tex->image = ImageBuilder()
			.Format(format)
			.Size(width, height, mipcount)
			.Usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
			.DebugName("CachedTexture.Image")
			.Create(renderer->Device.get());

		tex->imageView = ImageViewBuilder()
			.Image(tex->image.get(), format)
			.DebugName("CachedTexture.ImageView")
			.Create(renderer->Device.get());
	}

	tex->pendingUploads[0].clear();
	tex->pendingUploads[1].clear();

	if (uploader)
		UploadData(tex, Info, masked, uploader);
	else
		UploadWhite(tex);
}

void UploadManager::UploadTextureRect(CachedTexture* tex, const TextureInfo& Info, int x, int y, int w, int h)
{
	renderer->Commands->WaitForFrame();

	TextureUploader* uploader = TextureUploader::GetUploader(Info.Format, renderer->Device->PhysicalDevice.Device);
	if (!uploader || Info.NumMips < 1 || x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > Info.Mips[0].Width || y + h > Info.Mips[0].Height || Info.Mips[0].Data.empty())
		return;

	size_t pixelsSize = uploader->GetUploadSize(x, y, w, h);
	pixelsSize = (pixelsSize + 15) / 16 * 16; // memory alignment
	if (pixelsSize > (size_t)BufferManager::UploadBufferSize)
		return;

	WaitIfUploadBufferIsFull((int)pixelsSize);

	uint8_t* data = renderer->Buffers->UploadData;
	uint8_t* Ptr = data + UploadBufferPos;
	uploader->UploadRect(Ptr, &Info.Mips[0], x, y, w, h, Info.Palette, false);

	VkBufferImageCopy region = {};
	region.bufferOffset = (VkDeviceSize)(Ptr - data);
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { (int32_t)x, (int32_t)y, 0 };
	region.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };

	AddPendingUpload(tex, region, true);

	UploadBufferPos += (int)pixelsSize;
}

// Each mip into the staging buffer and copied to its level: whole while it
// fits, else in bands of rows -- whole blocks of a block format -- the buffer
// waited on whenever it is full.
void UploadManager::UploadData(CachedTexture* tex, const TextureInfo& Info, bool masked, TextureUploader* uploader)
{
	int blockRows = uploader->GetBlockRows();
	for (int level = 0; level < Info.NumMips; level++)
	{
		UnrealMipmap* Mip = &Info.Mips[level];
		if (Mip->Data.empty() || Mip->Width < 1 || Mip->Height < 1)
			continue;

		int bandSize = std::max(uploader->GetUploadSize(0, 0, Mip->Width, blockRows), 1);
		int bandRows = std::max(BufferManager::UploadBufferSize / bandSize, 1) * blockRows;
		for (int y = 0; y < Mip->Height; y += bandRows)
		{
			int rows = std::min(bandRows, Mip->Height - y);
			int size = uploader->GetUploadSize(0, y, Mip->Width, rows);
			size = (size + 15) / 16 * 16; // memory alignment
			WaitIfUploadBufferIsFull(size);

			VkBufferImageCopy region = {};
			region.bufferOffset = UploadBufferPos;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = level;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { 0, y, 0 };
			region.imageExtent = { (uint32_t)Mip->Width, (uint32_t)rows, 1 };
			AddPendingUpload(tex, region, false);

			uploader->UploadRect(renderer->Buffers->UploadData + UploadBufferPos, Mip, 0, y, Mip->Width, rows, Info.Palette, masked);
			UploadBufferPos += size;
		}
	}
}

void UploadManager::UploadWhite(CachedTexture* tex)
{
	WaitIfUploadBufferIsFull(16);

	auto data = (uint32_t*)(renderer->Buffers->UploadData + UploadBufferPos);
	data[0] = 0xffffffff;

	VkBufferImageCopy region = {};
	region.bufferOffset = UploadBufferPos;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = { 1, 1, 1 };
	AddPendingUpload(tex, region, false);

	UploadBufferPos += 16; // 16-byte aligned
}

// The copies made and waited for when the bytes would not fit: the buffer
// free again from its start.
void UploadManager::WaitIfUploadBufferIsFull(int bytes)
{
	if (UploadBufferPos + bytes > BufferManager::UploadBufferSize)
	{
		renderer->Commands->WaitForTransfer();
		UploadBufferPos = 0;
	}
}

void UploadManager::AddPendingUpload(CachedTexture* tex, const VkBufferImageCopy& region, bool isPartial)
{
	if (!tex->inPendingUploads)
	{
		PendingUploads.push_back(tex);
		tex->inPendingUploads = true;
	}

	tex->pendingUploads[isPartial].push_back(region);
}

void UploadManager::SubmitUploads()
{
	if (PendingUploads.empty())
		return;

	auto cmdbuffer = renderer->Commands->GetTransferCommands();

	// Transition images to transfer
	PipelineBarrier beforeBarrier;
	for (CachedTexture* tex : PendingUploads)
	{
		beforeBarrier.AddImage(
			tex->image->image,
			tex->imageLayout,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, tex->image->mipLevels);

		tex->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}
	beforeBarrier.Execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	// Do full texture uploads, then partial
	for (int i = 0; i < 2; i++)
	{
		// Copy from buffer to images
		VkBuffer buffer = renderer->Buffers->UploadBuffer->buffer;
		for (CachedTexture* tex : PendingUploads)
		{
			if (!tex->pendingUploads[i].empty())
			{
				if (i == 0)
					renderer->Stats.Uploads++;
				else
					renderer->Stats.RectUploads++;

				cmdbuffer->copyBufferToImage(buffer, tex->image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)tex->pendingUploads[i].size(), tex->pendingUploads[i].data());
			}
		}
	}

	// Transition images to texture sampling
	PipelineBarrier afterBarrier;
	for (CachedTexture* tex : PendingUploads)
	{
		afterBarrier.AddImage(
			tex->image->image,
			tex->imageLayout,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, tex->image->mipLevels);

		tex->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	afterBarrier.Execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	// Remove textures from pending uploads
	for (CachedTexture* tex : PendingUploads)
	{
		tex->pendingUploads[0].clear();
		tex->pendingUploads[1].clear();
		tex->inPendingUploads = false;
	}
	PendingUploads.clear();
	UploadBufferPos = 0;
}
