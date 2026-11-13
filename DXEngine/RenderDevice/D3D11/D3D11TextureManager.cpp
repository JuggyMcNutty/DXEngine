
#include "Precomp.h"
#include "D3D11TextureManager.h"
#include "D3D11RenderDevice.h"
#include "D3D11CachedTexture.h"
#include <d3d11.h>

D3D11TextureManager::D3D11TextureManager(D3D11RenderDevice* renderer) : renderer(renderer)
{
}

D3D11TextureManager::~D3D11TextureManager()
{
	ClearCache();
}

D3D11CachedTexture* D3D11TextureManager::GetFromCache(uint64_t cacheID)
{
	if (LastTextureResult.first == cacheID && LastTextureResult.second)
		return LastTextureResult.second;

	LastTextureResult.first = cacheID;
	LastTextureResult.second = TextureCache[cacheID].get();

	return LastTextureResult.second;
}

void D3D11TextureManager::UpdateTextureRect(TextureInfo* info, int x, int y, int w, int h)
{
	D3D11CachedTexture* tex = GetFromCache(info->CacheID);
	if (tex)
	{
		CacheStats.Upload(tex->Stat, *info, false, [&] { renderer->Uploads->UploadTextureRect(tex, *info, x, y, w, h, tex->Masked); });
		info->bRealtimeChanged = 0;
	}
}

D3D11CachedTexture* D3D11TextureManager::GetTexture(TextureInfo* info, bool masked)
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
	D3D11CachedTexture* tex = GetFromCache(info->CacheID);
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
		std::unique_ptr<D3D11CachedTexture>& tex2 = TextureCache[info->CacheID];
		tex2.reset(new D3D11CachedTexture());
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

void D3D11TextureManager::ClearCache()
{
	TextureCache.clear();
	LastTextureResult = {};
}

D3D11CachedTexture* D3D11TextureManager::CreateNullTexture()
{
	NullTexture.reset(new D3D11CachedTexture());

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	HRESULT result = renderer->Device->CreateTexture2D(&texDesc, nullptr, NullTexture->Texture.TypedInitPtr());
	ThrowIfFailed(result, "CreateTexture2D(NullTexture) failed");

	result = renderer->Device->CreateShaderResourceView(NullTexture->Texture, nullptr, NullTexture->View.TypedInitPtr());
	ThrowIfFailed(result, "CreateShaderResourceView(NullTexture) failed");

	renderer->Uploads->UploadWhite(NullTexture->Texture);

	return NullTexture.get();
}

void D3D11TextureManager::AddPoolStats(Array<RenderDevice::TexturePoolStats>& pools) const
{
	CacheStats.AddPools(TextureCache, pools);
}
