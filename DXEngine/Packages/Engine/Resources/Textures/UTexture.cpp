
#include "Precomp.h"
#include "UTexture.h"
#include "Engine.h"
#include "Packages/Engine/UDXEngineClient.h"
#include <algorithm>

void UTexture::Load(ObjectStream* stream)
{
	UBitmap::Load(stream);

	int mipsCount = stream->ReadUInt8();
	UncompressedMipmaps.resize(mipsCount);
	for (UnrealMipmap& mipmap : UncompressedMipmaps)
	{
		uint32_t widthoffset = 0;
		if (stream->GetVersion() >= 63)
			widthoffset = stream->ReadInt32();
		int bytes = stream->ReadIndex();
		mipmap.Data.resize(bytes);
		stream->ReadBytes(mipmap.Data.data(), bytes);
		mipmap.Width = stream->ReadUInt32();
		mipmap.Height = stream->ReadUInt32();
		mipmap.UBits = stream->ReadUInt8();
		mipmap.VBits = stream->ReadUInt8();
	}

	bool compressed = false;
	if (HasProperty("bHasComp") && GetBool("bHasComp"))
	{
		mipsCount = stream->ReadUInt8();
		CompressedMipmaps.resize(mipsCount);
		for (UnrealMipmap& mipmap : CompressedMipmaps)
		{
			uint32_t widthoffset = 0;
			if (stream->GetVersion() >= 68)
				widthoffset = stream->ReadInt32();
			int bytes = stream->ReadIndex();
			mipmap.Data.resize(bytes);
			stream->ReadBytes(mipmap.Data.data(), bytes);
			mipmap.Width = stream->ReadUInt32();
			mipmap.Height = stream->ReadUInt32();
			mipmap.UBits = stream->ReadUInt8();
			mipmap.VBits = stream->ReadUInt8();
		}

		// The original's UTexture::Lock (Engine.dll 0x103ecc90) draws the
		// compressed mips only when they are DXT1, which every device here
		// draws; else the others.
		compressed = GetByte("CompFormat") == (uint8_t)TextureFormat::BC1 && !CompressedMipmaps.empty();
	}

	int format = compressed ? GetByte("CompFormat") : GetByte("Format");
	UsedMipmaps = compressed ? CompressedMipmaps : UncompressedMipmaps;
	if (format <= (int)TextureFormat::BGRA8)
	{
		UsedFormat = (TextureFormat)format;
	}
	else
	{
		UsedFormat = TextureFormat::Unknown;
		LogMessage("Texture " + Name.ToString() + ": format " + std::to_string(format) + " is none Deus Ex has; drawn white");
	}
}

// Engine.dll UTexture::Lock (0x103ecc90), a frame's texture locked with no
// LOD of its own: the client's TextureLODSet for the texture's LODSet,
// raised to the render device's RecommendedLOD (D3DDrv leaves it 0), held to
// 3 and to the last mip.
int UTexture::DetailLOD()
{
	if (!engine || !engine->client || UsedMipmaps.empty())
		return 0;
	int lod = engine->client->DetailLevel(LODSet());
	return std::max(std::min({ lod, 3, (int)UsedMipmaps.size() - 1 }), 0);
}

const UnrealMipmap* UTexture::P8Mip() const
{
	if (UsedFormat == TextureFormat::P8)
		return UsedMipmaps.empty() ? nullptr : &UsedMipmaps.front();
	if (const_cast<UTexture*>(this)->Format() == (uint8_t)TextureFormat::P8 && !UncompressedMipmaps.empty())
		return &UncompressedMipmaps.front();
	return nullptr;
}

void UTexture::Save(PackageStreamWriter* stream)
{
	UBitmap::Save(stream);

	stream->WriteUInt8((uint8_t)UncompressedMipmaps.size());
	for (const UnrealMipmap& mipmap : UncompressedMipmaps)
	{
		if (stream->GetVersion() >= 63)
			stream->BeginSkipOffset();
		stream->WriteIndex((int)mipmap.Data.size());
		stream->WriteBytes(mipmap.Data.data(), (int)mipmap.Data.size());
		if (stream->GetVersion() >= 63)
			stream->EndSkipOffset();
		stream->WriteUInt32(mipmap.Width);
		stream->WriteUInt32(mipmap.Height);
		stream->WriteUInt8(mipmap.UBits);
		stream->WriteUInt8(mipmap.VBits);
	}

	if (HasProperty("bHasComp") && GetBool("bHasComp"))
	{
		stream->WriteUInt8((uint8_t)CompressedMipmaps.size());
		for (UnrealMipmap& mipmap : CompressedMipmaps)
		{
			if (stream->GetVersion() >= 68)
				stream->BeginSkipOffset();
			stream->WriteIndex((int)mipmap.Data.size());
			stream->WriteBytes(mipmap.Data.data(), (int)mipmap.Data.size());
			if (stream->GetVersion() >= 68)
				stream->EndSkipOffset();
			stream->WriteUInt32(mipmap.Width);
			stream->WriteUInt32(mipmap.Height);
			stream->WriteUInt8(mipmap.UBits);
			stream->WriteUInt8(mipmap.VBits);
		}
	}
}

// The original steps a texture from its Update, at most once a frame that
// draws it (UTexture::Tick): PrimeCount steps the first time, then a step a
// frame with no MaxFrameRate, else one once 1/MaxFrameRate has built up, the
// excess kept up to 1/MinFrameRate. With no MaxFrameRate a step comes at
// most 60 times a second here, as at the original's 60 frames.
void UTexture::Update(float elapsed)
{
	if (!Prepared)
	{
		Prepared = true;
		Prepare();
	}

	while (PrimeCurrent() < PrimeCount())
	{
		PrimeCurrent()++;
		UpdateFrame();
	}

	if (MaxFrameRate() == 0.0f)
	{
		const float step = 1.0f / 60.0f;
		FrameTime += elapsed;
		if (FrameTime >= step)
		{
			FrameTime = std::min(FrameTime - step, step);
			UpdateFrame();
		}
		return;
	}

	float minInterval = 1.0f / clamp(MaxFrameRate(), 0.01f, 100.0f);
	float maxInterval = 1.0f / clamp(MinFrameRate(), 0.01f, 100.0f);
	Accumulator() += elapsed;
	if (Accumulator() < minInterval)
		return;
	UpdateFrame();
	if (Accumulator() >= maxInterval)
		Accumulator() = std::min(Accumulator() - maxInterval, maxInterval);
	else
		Accumulator() = 0.0f;
}

void UTexture::UpdateFrame()
{
	// Loop textures
	UTexture* cur = AnimCurrent();
	if (!cur) cur = this;
	cur = cur->AnimNext();
	if (!cur) cur = this;
	AnimCurrent() = cur;
}

void UTexture::OnGCDestroy()
{
	if (engine)
		engine->GarbageTextureDied = true;
	UBitmap::OnGCDestroy();
}
