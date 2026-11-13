#pragma once

#include "RenderDevice/RenderDevice.h"

#include "SceneTextures.h"

struct TextureInfo;
class VulkanRenderDevice;
class CachedTexture;

class TextureManager
{
public:
	TextureManager(VulkanRenderDevice* renderer);
	~TextureManager();

	void UpdateTextureRect(TextureInfo* info, int x, int y, int w, int h);
	CachedTexture* GetTexture(TextureInfo* info, bool masked);

	void ClearCache();

	// STAT HARDWARE's: the cache's counts, and its pools (RenderDevice::
	// FrameDrawStats)
	TextureCacheStats CacheStats;
	void AddPoolStats(Array<RenderDevice::TexturePoolStats>& pools) const;
	void ClearAllBindlessIndexes();

	std::unique_ptr<VulkanImage> NullTexture;
	std::unique_ptr<VulkanImageView> NullTextureView;

	std::unique_ptr<VulkanImage> DitherImage;
	std::unique_ptr<VulkanImageView> DitherImageView;

	std::unique_ptr<SceneTextures> Scene;

	int GetTexturesInCache() { return (int)TextureCache.size(); }

private:
	void CreateNullTexture();
	void CreateDitherTexture();

	VulkanRenderDevice* renderer = nullptr;
	// One copy a texture, as D3DDrv keeps (dx-reverse-info d3ddrv-dll.md, textures)
	std::unordered_map<uint64_t, std::unique_ptr<CachedTexture>> TextureCache;
};
