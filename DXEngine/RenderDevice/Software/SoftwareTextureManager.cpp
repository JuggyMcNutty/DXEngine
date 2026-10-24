
#include "Precomp.h"
#include "SoftwareTextureManager.h"
#include "SoftwareTextureUploader.h"
#include "RenderDevice/RenderDevice.h"
#include "Packages/Engine/Resources/Level/UModel.h"

SoftwareTextureManager::SoftwareTextureManager()
{
	SoftwareTextureUploader::MakeWhite(&White);
}

SoftwareTexture* SoftwareTextureManager::Find(int masked, uint64_t cacheID)
{
	if (LastHit[masked].second && LastHit[masked].first == cacheID)
		return LastHit[masked].second;

	auto it = TextureCache[masked].find(cacheID);
	if (it == TextureCache[masked].end())
		return nullptr;
	LastHit[masked] = { cacheID, it->second.get() };
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

	SoftwareTexture* tex = Find((int)masked, info->CacheID);
	if (!tex)
	{
		std::unique_ptr<SoftwareTexture>& slot = TextureCache[(int)masked][info->CacheID];
		slot = std::make_unique<SoftwareTexture>();
		tex = slot.get();
		LastHit[(int)masked] = { info->CacheID, tex };
		SoftwareTextureUploader::Upload(tex, *info, masked);
		info->bRealtimeChanged = false;
	}
	else if (info->bRealtimeChanged)
	{
		info->bRealtimeChanged = false;
		if (info->UpdateWidth > 0)
		{
			if (BeforeWrite)
				BeforeWrite(tex, info->UpdateX, info->UpdateY, info->UpdateWidth, info->UpdateHeight);
			SoftwareTextureUploader::UploadRect(tex, *info, masked, info->UpdateX, info->UpdateY, info->UpdateWidth, info->UpdateHeight);
		}
		else
		{
			if (BeforeWrite)
				BeforeWrite(tex, 0, 0, -1, -1);
			SoftwareTextureUploader::Upload(tex, *info, masked);
		}
	}
	return tex;
}

void SoftwareTextureManager::UpdateTextureRect(TextureInfo* info, int x, int y, int w, int h)
{
	SoftwareTexture* tex = Find(0, info->CacheID);
	if (tex)
	{
		if (BeforeWrite)
			BeforeWrite(tex, x, y, w, h);
		SoftwareTextureUploader::UploadRect(tex, *info, false, x, y, w, h);
		info->bRealtimeChanged = false;
	}
}

void SoftwareTextureManager::ClearCache()
{
	for (auto& cache : TextureCache)
		cache.clear();
	for (auto& hit : LastHit)
		hit = {};
}
