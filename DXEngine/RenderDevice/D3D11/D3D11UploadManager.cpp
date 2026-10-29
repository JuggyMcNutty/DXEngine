
#include "Precomp.h"
#include "D3D11UploadManager.h"
#include "D3D11RenderDevice.h"
#include "D3D11CachedTexture.h"

D3D11UploadManager::D3D11UploadManager(D3D11RenderDevice* renderer) : renderer(renderer)
{
}

D3D11UploadManager::~D3D11UploadManager()
{
}

bool D3D11UploadManager::SupportsTextureFormat(TextureFormat Format) const
{
	return D3D11TextureUploader::GetUploader(Format);
}

void D3D11UploadManager::UploadTexture(D3D11CachedTexture* tex, const TextureInfo& Info, bool masked)
{
	// The image the size of its top mip's data, whatever the texture's USize
	// and VSize say; none, too big or short of its bytes, a white texel, as
	// the Vulkan device has it. The levels are at most the chain down to 1x1:
	// more made CreateTexture2D fail, which threw out of the draw.
	bool hasMips = Info.NumMips >= 1 && Info.Mips;
	int width = hasMips ? Info.Mips[0].Width : 0;
	int height = hasMips ? Info.Mips[0].Height : 0;
	int mipcount = Info.NumMips;

	D3D11TextureUploader* uploader = D3D11TextureUploader::GetUploader(Info.Format);

	if (!uploader || width < 1 || height < 1 || (uint32_t)width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || (uint32_t)height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
		Info.Mips[0].Data.size() < RenderDevice::MipBytes(Info.Format, width, height))
	{
		width = 1;
		height = 1;
		mipcount = 1;
		uploader = nullptr;
	}
	else
	{
		int maxLevels = 1;
		while ((std::max(width, height) >> maxLevels) >= 1)
			maxLevels++;
		mipcount = std::min(mipcount, maxLevels);
	}

	DXGI_FORMAT format = uploader ? uploader->GetDxgiFormat() : DXGI_FORMAT_R8G8B8A8_UNORM;

	// Base texture must use complete 4x4 compression blocks in Direct3D 11 or some drivers crash.
	// 
	// We fix this by creating 1 or 2 additional mipmap levels that are empty.

	int minSize = uploader && Info.Format == TextureFormat::BC1 ? 4 : 0;

	if (!tex->Texture)
	{
		if (width < minSize || height < minSize)
		{
			if (width == 1 || height == 1)
			{
				width *= 4;
				height *= 4;
				mipcount += 2;
				tex->DummyMipmapCount = 2;
			}
			else
			{
				width *= 2;
				height *= 2;
				mipcount += 1;
				tex->DummyMipmapCount = 1;
			}
		}

		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = mipcount;
		texDesc.ArraySize = 1;
		texDesc.Format = format;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		//texDesc.MiscFlags = tex->DummyMipmapCount > 0 ? D3D11_RESOURCE_MISC_RESOURCE_CLAMP : 0;
		HRESULT result = renderer->Device->CreateTexture2D(&texDesc, nullptr, tex->Texture.TypedInitPtr());
		ThrowIfFailed(result, "CreateTexture2D(GameTexture) failed");

		//if (tex->DummyMipmapCount > 0)
		//	renderer->Context->SetResourceMinLOD(tex->Texture, (float)tex->DummyMipmapCount);

		//D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		//viewDesc.Format = format;
		//viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		//viewDesc.Texture2D.MostDetailedMip = tex->DummyMipmapCount;
		//viewDesc.Texture2D.MipLevels = -1;
		result = renderer->Device->CreateShaderResourceView(tex->Texture, nullptr/*&viewDesc*/, tex->View.TypedInitPtr());
		ThrowIfFailed(result, "CreateShaderResourceView(GameTexture) failed");
	}

	if (uploader)
		UploadData(tex->Texture, Info, masked, uploader, tex->DummyMipmapCount, minSize);
	else
		UploadWhite(tex->Texture);

	renderer->Stats.Uploads++;
}

void D3D11UploadManager::UploadTextureRect(D3D11CachedTexture* tex, const TextureInfo& Info, int x, int y, int w, int h, bool masked)
{
	D3D11TextureUploader* uploader = D3D11TextureUploader::GetUploader(Info.Format);
	if (!uploader || Info.NumMips < 1 || x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > Info.Mips[0].Width || y + h > Info.Mips[0].Height || Info.Mips[0].Data.empty())
		return;

	// Whole blocks of the format (BC1's 4x4), the box on their edges
	int block = uploader->GetBlockSize();
	int x0 = x / block * block, y0 = y / block * block;
	int x1 = std::min((x + w + block - 1) / block * block, Info.Mips[0].Width);
	int y1 = std::min((y + h + block - 1) / block * block, Info.Mips[0].Height);
	x = x0; y = y0; w = x1 - x0; h = y1 - y0;

	size_t pixelsSize = uploader->GetUploadSize(x, y, w, h);
	pixelsSize = (pixelsSize + 15) / 16 * 16; // memory alignment

	uint8_t* data = GetUploadBuffer(pixelsSize);
	uploader->UploadRect(data, &Info.Mips[0], x, y, w, h, Info.Palette, masked);

	UINT pitch = uploader->GetUploadSize(x, y, w, 1);

	D3D11_BOX box = {};
	box.left = x;
	box.top = y;
	box.right = x + w;
	box.bottom = y + h;
	box.back = 1;
	// The first level of the texture's own, past the padding levels a
	// small block texture is given
	renderer->Context->UpdateSubresource(tex->Texture, D3D11CalcSubresource(tex->DummyMipmapCount, 0, Info.NumMips + tex->DummyMipmapCount), &box, data, pitch, 0);

	renderer->Stats.RectUploads++;
}

// Each level the image has, past its padding ones. A level whose mip is not
// its size, or is short of its bytes (a package's misfit chain), is zeros, as
// the Vulkan device has it: its box lay outside the level and its read ran
// past the mip.
void D3D11UploadManager::UploadData(ID3D11Texture2D* image, const TextureInfo& Info, bool masked, D3D11TextureUploader* uploader, int dummyMipmapCount, INT minSize)
{
	D3D11_TEXTURE2D_DESC desc = {};
	image->GetDesc(&desc);
	int levels = std::min(Info.NumMips, (int)desc.MipLevels - dummyMipmapCount);
	int levelWidth = Info.Mips[0].Width;
	int levelHeight = Info.Mips[0].Height;
	for (INT level = 0; level < levels; level++)
	{
		UnrealMipmap* Mip = &Info.Mips[level];
		bool fits = Mip->Width == levelWidth && Mip->Height == levelHeight && Mip->Data.size() >= RenderDevice::MipBytes(Info.Format, Mip->Width, Mip->Height);
		if (!Mip->Data.empty())
		{
			uint32_t mipwidth = std::max(levelWidth, minSize);
			uint32_t mipheight = std::max(levelHeight, minSize);

			INT mipsize = uploader->GetUploadSize(0, 0, mipwidth, mipheight);
			mipsize = (mipsize + 15) / 16 * 16; // memory alignment

			auto data = (uint32_t*)GetUploadBuffer(mipsize);
			if (fits)
				uploader->UploadRect(data, Mip, 0, 0, mipwidth, mipheight, Info.Palette, masked);
			else
				memset(data, 0, mipsize);

			UINT pitch = uploader->GetUploadSize(0, 0, mipwidth, 1);

			D3D11_BOX box = {};
			box.right = mipwidth;
			box.bottom = mipheight;
			box.back = 1;

			UINT subresource = D3D11CalcSubresource(level + dummyMipmapCount, 0, desc.MipLevels);
			renderer->Context->UpdateSubresource(image, subresource, &box, data, pitch, 0);
		}
		levelWidth = std::max(levelWidth >> 1, 1);
		levelHeight = std::max(levelHeight >> 1, 1);
	}
}

void D3D11UploadManager::UploadWhite(ID3D11Texture2D* image)
{
	auto data = (uint32_t*)GetUploadBuffer(sizeof(uint32_t));
	data[0] = 0xffffffff;

	D3D11_BOX box = {};
	box.right = 1;
	box.bottom = 1;
	box.back = 1;
	renderer->Context->UpdateSubresource(image, D3D11CalcSubresource(0, 0, 1), &box, data, 4, 0);
}

uint8_t* D3D11UploadManager::GetUploadBuffer(size_t size)
{
	// Using uint32_t vector for dword alignment
	size = (size + 3) / 4;
	if (UploadBuffer.size() < size)
		UploadBuffer.resize(size);
	return (uint8_t*)UploadBuffer.data();
}
