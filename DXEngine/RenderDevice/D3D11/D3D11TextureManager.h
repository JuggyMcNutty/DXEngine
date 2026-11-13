#pragma once

#include "RenderDevice/RenderDevice.h"

#include <unordered_map>

struct TextureInfo;
class D3D11RenderDevice;
class D3D11CachedTexture;

class D3D11TextureManager
{
public:
	D3D11TextureManager(D3D11RenderDevice* renderer);
	~D3D11TextureManager();

	void UpdateTextureRect(TextureInfo* info, int x, int y, int w, int h);
	D3D11CachedTexture* GetTexture(TextureInfo* info, bool masked);

	void ClearCache();

	// STAT HARDWARE's: the cache's counts, and its pools (RenderDevice::
	// FrameDrawStats)
	TextureCacheStats CacheStats;
	void AddPoolStats(Array<RenderDevice::TexturePoolStats>& pools) const;
	int GetTexturesInCache() { return (int)TextureCache.size(); }

	D3D11CachedTexture* GetNullTexture()
	{
		if (NullTexture)
			return NullTexture.get();
		return CreateNullTexture();
	}

private:
	D3D11CachedTexture* CreateNullTexture();

	D3D11CachedTexture* GetFromCache(uint64_t cacheID);

	D3D11RenderDevice* renderer = nullptr;
	// One copy a texture, as D3DDrv keeps (dx-reverse-info d3ddrv-dll.md, textures)
	std::unordered_map<uint64_t, std::unique_ptr<D3D11CachedTexture>> TextureCache;
	std::unique_ptr<D3D11CachedTexture> NullTexture;

	std::pair<uint64_t, D3D11CachedTexture*> LastTextureResult;
};
