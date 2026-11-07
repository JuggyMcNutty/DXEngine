
#include "Precomp.h"
#include "GLUploadManager.h"
#include "GLRenderDevice.h"
#include "GLCachedTexture.h"

GLUploadManager::GLUploadManager(GLRenderDevice* renderer) : renderer(renderer)
{
	// At least 16,384 on desktop GL 4, 2,048 on ES 3.0: a texture over it is
	// the white texel, where its glTexImage2D failed and threw out of the draw
	GLint maxSize = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
	if (maxSize > 0)
		MaxTextureSize = maxSize;
}

GLUploadManager::~GLUploadManager()
{
}

bool GLUploadManager::SupportsTextureFormat(TextureFormat Format) const
{
	return GLTextureUploader::GetUploader(Format);
}

// The texture's levels made once and its mips sent, each level its own size,
// the image the size of its top mip's data; none, too big for the driver or
// short of its bytes (a mod's DXT1 mip, which the package's load does not
// pad), a white texel, as the Vulkan and D3D11 devices have it. A compressed
// format's levels are made by their uploads (glCompressedTexImage2D), as ES
// takes no compressed format in glTexImage2D.
void GLUploadManager::UploadTexture(GLCachedTexture* tex, const TextureInfo& Info, bool masked)
{
	bool hasMips = Info.NumMips >= 1 && Info.Mips;
	int width = hasMips ? Info.Mips[0].Width : 0;
	int height = hasMips ? Info.Mips[0].Height : 0;
	int mipcount = Info.NumMips;

	GLTextureUploader* uploader = GLTextureUploader::GetUploader(Info.Format);

	if (!uploader || width < 1 || height < 1 || width > MaxTextureSize || height > MaxTextureSize ||
		Info.Mips[0].Data.size() < RenderDevice::MipBytes(Info.Format, width, height))
	{
		width = 1;
		height = 1;
		mipcount = 1;
		uploader = nullptr;
	}

	GLint internalFormat = GL_RGBA8;
	GLenum format = GL_RGBA;
	GLenum type = GL_UNSIGNED_BYTE;
	bool compressed = false;
	if (uploader)
	{
		internalFormat = uploader->GetInternalformat();
		if (uploader->GetFormat() != 0)
		{
			format = uploader->GetFormat();
			type = uploader->GetType();
		}
		else
		{
			compressed = true;
		}
	}

	if (!tex->Texture)
	{
		tex->Texture = std::make_shared<GLTexture2D>();
		glBindTexture(GL_TEXTURE_2D, tex->Texture->Handle);
		if (!compressed)
		{
			int mipwidth = width;
			int mipheight = height;
			for (int miplevel = 0; miplevel < mipcount; miplevel++)
			{
				glTexImage2D(GL_TEXTURE_2D, miplevel, internalFormat, mipwidth, mipheight, 0, format, type, nullptr);
				mipwidth = std::max(mipwidth >> 1, 1);
				mipheight = std::max(mipheight >> 1, 1);
			}
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, std::max(mipcount - 1, 0));
		// Once a texture made, not once a level or an upload: each ask makes a
		// threaded driver finish its queue (GLRenderDevice.h, ThrowIfGLError)
		ThrowIfGLError("UploadTexture failed");
	}

	if (uploader)
		UploadData(tex->Texture.get(), Info, masked, uploader, width, height);
	else
		UploadWhite(tex->Texture.get());
	renderer->ForgetBoundTexture(); // bound on the active unit, 0

	renderer->Stats.Uploads++;
}

void GLUploadManager::UploadTextureRect(GLCachedTexture* tex, const TextureInfo& Info, int x, int y, int w, int h, bool masked)
{
	GLTextureUploader* uploader = GLTextureUploader::GetUploader(Info.Format);
	if (!uploader || uploader->GetFormat() == 0 || Info.NumMips < 1 || !Info.Mips || x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > Info.Mips[0].Width || y + h > Info.Mips[0].Height || Info.Mips[0].Data.empty())
		return;

	size_t pixelsSize = uploader->GetUploadSize(x, y, w, h);
	pixelsSize = (pixelsSize + 15) / 16 * 16; // memory alignment

	uint8_t* data = GetUploadBuffer(pixelsSize);
	uploader->UploadRect(data, &Info.Mips[0], x, y, w, h, Info.Palette, masked);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // the rows packed tight
	glBindTexture(GL_TEXTURE_2D, tex->Texture->Handle);
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, uploader->GetFormat(), uploader->GetType(), data);
	renderer->ForgetBoundTexture(); // bound on the active unit, 0

	renderer->Stats.RectUploads++;
}

// Each mip into its level, the rows packed tight (a 1-wide 16-bit mip's are 2
// bytes). A compressed format defines every level, by halves of the top's
// size down to 1x1 -- a missing or misfit mip's, or one short of its bytes,
// from zero blocks -- so the chain is whole, as GL samples no incomplete one.
void GLUploadManager::UploadData(GLTexture2D* image, const TextureInfo& Info, bool masked, GLTextureUploader* uploader, int width, int height)
{
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glBindTexture(GL_TEXTURE_2D, image->Handle);
	bool compressed = uploader->GetFormat() == 0;
	int levelwidth = width;
	int levelheight = height;
	for (int level = 0; level < Info.NumMips; level++)
	{
		UnrealMipmap* Mip = &Info.Mips[level];
		bool fits = !Mip->Data.empty() && Mip->Width == levelwidth && Mip->Height == levelheight &&
			Mip->Data.size() >= RenderDevice::MipBytes(Info.Format, Mip->Width, Mip->Height);
		if (compressed)
		{
			size_t mipsize = uploader->GetUploadSize(0, 0, levelwidth, levelheight);
			uint8_t* data = GetUploadBuffer(mipsize);
			if (fits)
				uploader->UploadRect(data, Mip, 0, 0, levelwidth, levelheight, Info.Palette, masked);
			else
				memset(data, 0, mipsize);
			glCompressedTexImage2D(GL_TEXTURE_2D, level, uploader->GetInternalformat(), levelwidth, levelheight, 0, (GLsizei)mipsize, data);
		}
		else
		{
			// A level with no mip of its own, or a misfit one, is zeros, as a
			// compressed one's is: made without data it was undefined, read
			// as whatever the driver had (the BC1 decode's, among others)
			size_t size = uploader->GetUploadSize(0, 0, levelwidth, levelheight);
			uint8_t* data = GetUploadBuffer(size);
			if (fits)
				uploader->UploadRect(data, Mip, 0, 0, levelwidth, levelheight, Info.Palette, masked);
			else
				memset(data, 0, size);
			glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, levelwidth, levelheight, uploader->GetFormat(), uploader->GetType(), data);
		}
		levelwidth = std::max(levelwidth >> 1, 1);
		levelheight = std::max(levelheight >> 1, 1);
	}
}

void GLUploadManager::UploadWhite(GLTexture2D* image)
{
	auto data = (uint32_t*)GetUploadBuffer(sizeof(uint32_t));
	data[0] = 0xffffffff;
	glBindTexture(GL_TEXTURE_2D, image->Handle);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

uint8_t* GLUploadManager::GetUploadBuffer(size_t size)
{
	// Using uint32_t vector for dword alignment
	size = (size + 3) / 4;
	if (UploadBuffer.size() < size)
		UploadBuffer.resize(size);
	return (uint8_t*)UploadBuffer.data();
}
