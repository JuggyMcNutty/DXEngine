#pragma once

#include <unordered_map>

struct TextureInfo;
class GLRenderDevice;
class GLCachedTexture;

class GLTextureManager
{
public:
	GLTextureManager(GLRenderDevice* renderer);
	~GLTextureManager();

	void UpdateTextureRect(TextureInfo* info, int x, int y, int w, int h);
	GLCachedTexture* GetTexture(TextureInfo* info, bool masked);

	void ClearCache();
	int GetTexturesInCache() { return (int)TextureCache.size(); }

	GLCachedTexture* GetNullTexture()
	{
		if (NullTexture)
			return NullTexture.get();
		return CreateNullTexture();
	}

private:
	GLCachedTexture* CreateNullTexture();

	GLCachedTexture* GetFromCache(uint64_t cacheID);

	GLRenderDevice* renderer = nullptr;
	// One copy a texture, as D3DDrv keeps (dx-reverse-info d3ddrv-dll.md, textures)
	std::unordered_map<uint64_t, std::unique_ptr<GLCachedTexture>> TextureCache;
	std::unique_ptr<GLCachedTexture> NullTexture;

	std::pair<uint64_t, GLCachedTexture*> LastTextureResult;
};
