
#include "Precomp.h"
#include "UFireTexture.h"

void UFireTexture::Load(ObjectStream* stream)
{
	UFractalTexture::Load(stream);

	int size = stream->ReadIndex();
	Sparks.resize(size);
	for (int i = 0; i < size; i++)
	{
		FireEngine::Spark& spark = Sparks[i];
		spark.Type = stream->ReadUInt8();
		spark.Heat = stream->ReadUInt8();
		spark.X = stream->ReadUInt8();
		spark.Y = stream->ReadUInt8();
		spark.A = stream->ReadUInt8();
		spark.B = stream->ReadUInt8();
		spark.C = stream->ReadUInt8();
		spark.D = stream->ReadUInt8();
	}
}

void UFireTexture::Save(PackageStreamWriter* stream)
{
	UFractalTexture::Save(stream);

	// The sparks placed in the editor, as the original's Serialize saves
	// them after the properties, NumSparks among them as it stood (Fire.dll
	// 0x10508300): from the last down, each one the sparks let go (31 on)
	// takes the last one's place and the count drops, the fire going on
	// without them; the placed ones written in the order that leaves, and
	// the slots past them emptied, as a load leaves them
	int count = std::clamp(NumSparks(), 0, (int)Sparks.size());
	for (int i = count - 1; i >= 0; i--)
	{
		if (Sparks[i].Type >= 31)
			Sparks[i] = Sparks[--count];
	}
	NumSparks() = count;
	for (size_t i = count; i < Sparks.size(); i++)
		Sparks[i] = {};
	stream->WriteIndex(count);
	for (int i = 0; i < count; i++)
	{
		const FireEngine::Spark& spark = Sparks[i];
		stream->WriteUInt8(spark.Type);
		stream->WriteUInt8(spark.Heat);
		stream->WriteUInt8(spark.X);
		stream->WriteUInt8(spark.Y);
		stream->WriteUInt8(spark.A);
		stream->WriteUInt8(spark.B);
		stream->WriteUInt8(spark.C);
		stream->WriteUInt8(spark.D);
	}
}

// The original's constructor: no old heat (-1), so its PostLoad builds the
// colour table whatever the package's RenderHeat, one of 0 too, and the
// stars on (Fire.dll 0x10507840)
void UFireTexture::InitNativeDefaults()
{
	UFractalTexture::InitNativeDefaults();
	OldRenderHeat() = -1;
	StarStatus() = 1;
}

// The original's PostLoad: masks, never masked itself, the colours for
// RenderHeat, and room for SparksLimit sparks.
void UFireTexture::Prepare()
{
	FireEngine::InitTables();
	UMask() = USize() - 1;
	VMask() = VSize() - 1;
	bMasked() = false;

	if (OldRenderHeat() != RenderHeat())
	{
		FireEngine::BuildFireRenderTable(RenderTable().data(), RenderHeat());
		OldRenderHeat() = RenderHeat();
	}

	// The sparks grown to SparksLimit, as the original's Serialize grows
	// them as it loads (0x10508300); PostLoad holds SparksLimit to 4..8,192
	// only when the array is then not that size, more sparks saved than it
	// allows (0x10507e07), so a limit outside that range stands, one of 0
	// leaving the fire no sparks at all. A limit past 1,048,576 is held
	// there, where the original would take any limit's memory; NumSparks
	// held to the sparks saved.
	NumSparks() = std::clamp(NumSparks(), 0, (int)Sparks.size());
	SparksLimit() = std::min(SparksLimit(), 0x100000);
	int limit = SparksLimit();
	if ((int)Sparks.size() < limit)
		Grow(limit);
	if ((int)Sparks.size() != limit)
	{
		SparksLimit() = clamp(SparksLimit(), 4, 0x2000);
		limit = SparksLimit();
		if (NumSparks() > limit)
		{
			// Over the limit, what the sparks let go goes first.
			for (int i = NumSparks() - 1; i >= 0 && NumSparks() > limit; i--)
			{
				if (Sparks[i].Type >= 31)
					Sparks[i] = Sparks[--NumSparks()];
			}
			NumSparks() = std::min(NumSparks(), limit);
		}
		if ((int)Sparks.size() < limit)
			Grow(limit);
		else
			Sparks.resize(limit);
	}
}

// Empty slots to the given number of sparks
void UFireTexture::Grow(int size)
{
	size_t oldSize = Sparks.size();
	Sparks.resize(size);
	for (size_t i = oldSize; i < Sparks.size(); i++)
		Sparks[i] = {};
}

// The original's ConstantTimeTick: the sparks, the pass, the stars.
void UFireTexture::UpdateFrame()
{
	// With no sparks too, as the original's: the pass goes on, from a
	// SparksLimit of 0 (0x10507ff0)
	UnrealMipmap& mipmap = UsedMipmaps.front();
	if (mipmap.Width < 8 || mipmap.Height < 8)
		return;

	FireEngine::Fire fire;
	fire.Bits = mipmap.Data.data();
	// The row shift from the width itself: the package's UBits is not
	// checked against it, and a wrong one indexed outside the image
	int ubits = 0;
	while ((1u << ubits) < (unsigned)mipmap.Width && ubits < 16)
		ubits++;
	fire.UBits = ubits;
	fire.UMask = mipmap.Width - 1;
	fire.VMask = mipmap.Height - 1;
	fire.Sparks = Sparks.data();
	fire.SparksLimit = (int)Sparks.size();
	fire.NumSparks = NumSparks();
	fire.GlobalPhase = GlobalPhase();
	fire.AuxPhase = AuxPhase();
	fire.FX_Frequency = FX_Frequency();
	fire.StarStatus = StarStatus();

	FireEngine::RedrawSparks(fire);
	FireEngine::FirePass(fire.Bits, RenderTable().data(), mipmap.Width, mipmap.Height, bRising());
	FireEngine::PostDrawSparks(fire);

	NumSparks() = fire.NumSparks;
	GlobalPhase() = fire.GlobalPhase;
	AuxPhase() = fire.AuxPhase;
	StarStatus() = fire.StarStatus;
	TextureModified = true;
}
