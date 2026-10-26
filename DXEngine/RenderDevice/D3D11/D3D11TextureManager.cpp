
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

D3D11CachedTexture* D3D11TextureManager::GetFromCache(int masked, uint64_t cacheID)
{
	if (LastTextureResult[masked].first == cacheID && LastTextureResult[masked].second)
		return LastTextureResult[masked].second;

	LastTextureResult[masked].first = cacheID;
	LastTextureResult[masked].second = TextureCache[masked][cacheID].get();

	return LastTextureResult[masked].second;
}

void D3D11TextureManager::UpdateTextureRect(TextureInfo* info, int x, int y, int w, int h)
{
	// Both of the texture's cache entries, the masked one's rectangle masked
	for (int masked = 0; masked < 2; masked++)
	{
		D3D11CachedTexture* tex = GetFromCache(masked, info->CacheID);
		if (tex)
		{
			renderer->Uploads->UploadTextureRect(tex, *info, x, y, w, h, masked != 0);
			info->bRealtimeChanged = 0;
		}
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

	// A realtime change goes to every copy there is, masked and not, as
	// D3DDrv keeps one copy made again at a change: a P8 texture drawn
	// masked and unmasked in one frame animates in both
	if (info->bRealtimeChanged)
	{
		info->bRealtimeChanged = 0;
		for (int m = 0; m < 2; m++)
		{
			D3D11CachedTexture* copy = GetFromCache(m, info->CacheID);
			if (!copy)
				continue;
			if (info->UpdateWidth > 0)
				renderer->Uploads->UploadTextureRect(copy, *info, info->UpdateX, info->UpdateY, info->UpdateWidth, info->UpdateHeight, m != 0);
			else
				renderer->Uploads->UploadTexture(copy, *info, m != 0);
		}
	}

	D3D11CachedTexture* tex = GetFromCache((int)masked, info->CacheID);
	if (!tex)
	{
		std::unique_ptr<D3D11CachedTexture>& tex2 = TextureCache[(int)masked][info->CacheID];
		tex2.reset(new D3D11CachedTexture());
		tex = tex2.get();

		renderer->Uploads->UploadTexture(tex, *info, masked);
	}

	float uscale = info->UScale;
	float vscale = info->VScale;
	tex->UScale = uscale;
	tex->VScale = vscale;
	tex->PanX = info->Pan.x;
	tex->PanY = info->Pan.y;
	tex->UMult = 1.0f / (uscale * info->USize);
	tex->VMult = 1.0f / (vscale * info->VSize);

	return tex;
}

void D3D11TextureManager::ClearCache()
{
	for (auto& cache : TextureCache)
	{
		cache.clear();
	}
	for (auto& texture : LastTextureResult)
		texture = {};
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
