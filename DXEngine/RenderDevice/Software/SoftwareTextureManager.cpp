
#include "Precomp.h"
#include "SoftwareTextureManager.h"
#include "SoftwareTextureUploader.h"
#include "RenderDevice/RenderDevice.h"
#include "Packages/Engine/Resources/Level/UModel.h"

SoftwareTextureManager::SoftwareTextureManager()
{
	SoftwareTextureUploader::MakeWhite(&White);
}

SoftwareTexture* SoftwareTextureManager::Find(uint64_t cacheID)
{
	if (LastHit.second && LastHit.first == cacheID)
		return LastHit.second;

	auto it = TextureCache.find(cacheID);
	if (it == TextureCache.end())
		return nullptr;
	LastHit = { cacheID, it->second.get() };
	return it->second.get();
}

SoftwareTexture* SoftwareTextureManager::GetTexture(TextureInfo* info, bool masked)
{
	if (!info)
		return nullptr;

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
	SoftwareTexture* tex = Find(info->CacheID);
	if (info->bRealtimeChanged)
	{
		info->bRealtimeChanged = false;
		if (tex)
		{
			if (info->UpdateWidth > 0)
			{
				if (BeforeWrite)
					BeforeWrite(tex, info->UpdateX, info->UpdateY, info->UpdateWidth, info->UpdateHeight);
				CacheStats.Upload(tex->Stat, *info, false, [&] { SoftwareTextureUploader::UploadRect(tex, *info, tex->Masked, info->UpdateX, info->UpdateY, info->UpdateWidth, info->UpdateHeight); });
			}
			else
			{
				if (BeforeWrite)
					BeforeWrite(tex, 0, 0, -1, -1);
				tex->Masked = masked;
				CacheStats.Upload(tex->Stat, *info, true, [&] { SoftwareTextureUploader::Upload(tex, *info, masked); });
			}
		}
	}

	if (!tex)
	{
		std::unique_ptr<SoftwareTexture>& slot = TextureCache[info->CacheID];
		slot = std::make_unique<SoftwareTexture>();
		tex = slot.get();
		LastHit = { info->CacheID, tex };
		tex->Masked = masked;
		CacheStats.Upload(tex->Stat, *info, true, [&] { SoftwareTextureUploader::Upload(tex, *info, masked); });
	}
	CacheStats.Set(tex->Stat, *info);
	return tex;
}

void SoftwareTextureManager::UpdateTextureRect(TextureInfo* info, int x, int y, int w, int h)
{
	SoftwareTexture* tex = Find(info->CacheID);
	if (tex)
	{
		if (BeforeWrite)
			BeforeWrite(tex, x, y, w, h);
		CacheStats.Upload(tex->Stat, *info, false, [&] { SoftwareTextureUploader::UploadRect(tex, *info, tex->Masked, x, y, w, h); });
		info->bRealtimeChanged = false;
	}
}

void SoftwareTextureManager::ClearCache()
{
	TextureCache.clear();
	LastHit = {};
}

void SoftwareTextureManager::AddPoolStats(Array<RenderDevice::TexturePoolStats>& pools) const
{
	CacheStats.AddPools(TextureCache, pools);
}
