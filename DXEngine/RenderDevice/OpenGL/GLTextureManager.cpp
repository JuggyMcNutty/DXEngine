
#include "Precomp.h"
#include "GLTextureManager.h"
#include "GLRenderDevice.h"
#include "GLCachedTexture.h"

GLTextureManager::GLTextureManager(GLRenderDevice* renderer) : renderer(renderer)
{
}

GLTextureManager::~GLTextureManager()
{
	ClearCache();
}

GLCachedTexture* GLTextureManager::GetFromCache(uint64_t cacheID)
{
	if (LastTextureResult.first == cacheID && LastTextureResult.second)
		return LastTextureResult.second;

	LastTextureResult.first = cacheID;
	LastTextureResult.second = TextureCache[cacheID].get();

	return LastTextureResult.second;
}

void GLTextureManager::UpdateTextureRect(TextureInfo* info, int x, int y, int w, int h)
{
	GLCachedTexture* tex = GetFromCache(info->CacheID);
	if (tex)
	{
		CacheStats.Upload(tex->Stat, *info, false, [&] { renderer->Uploads->UploadTextureRect(tex, *info, x, y, w, h, tex->Masked); });
		info->bRealtimeChanged = 0;
	}
}

GLCachedTexture* GLTextureManager::GetTexture(TextureInfo* info, bool masked)
{
	if (!info)
		return GetNullTexture();

	if (info->Texture && (info->Texture->PolyFlags() & PF_Masked))
		masked = true;

	if (info->Format != TextureFormat::P8)
		masked = false;

	// One copy a texture, as D3DDrv's SetTexture finds its copy by the cache
	// ID alone (dx-reverse-info d3ddrv-dll.md, textures): the draw that first
	// sets a texture makes the copy, colour 0 cleared or not as that draw is
	// masked or not, and every later draw takes it so, until a realtime
	// change makes it again, as the draw then is; a rectangle of it changed
	// keeps the copy's own way
	GLCachedTexture* tex = GetFromCache(info->CacheID);
	if (info->bRealtimeChanged)
	{
		info->bRealtimeChanged = 0;
		if (tex)
		{
			if (info->UpdateWidth > 0)
			{
				CacheStats.Upload(tex->Stat, *info, false, [&] { renderer->Uploads->UploadTextureRect(tex, *info, info->UpdateX, info->UpdateY, info->UpdateWidth, info->UpdateHeight, tex->Masked); });
			}
			else
			{
				tex->Masked = masked;
				CacheStats.Upload(tex->Stat, *info, true, [&] { renderer->Uploads->UploadTexture(tex, *info, masked); });
			}
		}
	}

	if (!tex)
	{
		std::unique_ptr<GLCachedTexture>& tex2 = TextureCache[info->CacheID];
		tex2.reset(new GLCachedTexture());
		tex = tex2.get();
		LastTextureResult = { info->CacheID, tex };

		tex->Masked = masked;
		CacheStats.Upload(tex->Stat, *info, true, [&] { renderer->Uploads->UploadTexture(tex, *info, masked); });
	}

	float uscale = info->UScale;
	float vscale = info->VScale;
	tex->UScale = uscale;
	tex->VScale = vscale;
	tex->PanX = info->Pan.x;
	tex->PanY = info->Pan.y;
	tex->UMult = 1.0f / (uscale * info->USize);
	tex->VMult = 1.0f / (vscale * info->VSize);
	CacheStats.Set(tex->Stat, *info);

	return tex;
}

void GLTextureManager::ClearCache()
{
	TextureCache.clear();
	LastTextureResult = {};
}

GLCachedTexture* GLTextureManager::CreateNullTexture()
{
	NullTexture.reset(new GLCachedTexture());
	// GL_UNSIGNED_INT_8_8_8_8 (the desktop type this used) is not a valid
	// upload type for GL_RGBA8 under OpenGL ES; four bytes are the same image.
	uint8_t white[4] = { 255, 255, 255, 255 };
	NullTexture->Texture = std::make_shared<GLTexture2D>();
	glBindTexture(GL_TEXTURE_2D, NullTexture->Texture->Handle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	renderer->ForgetBoundTexture(); // bound on the active unit, 0
	return NullTexture.get();
}

void GLTextureManager::AddPoolStats(Array<RenderDevice::TexturePoolStats>& pools) const
{
	CacheStats.AddPools(TextureCache, pools);
}
