
#include "Precomp.h"
#include "UPalette.h"

void UPalette::Load(ObjectStream* stream)
{
	UObject::Load(stream);
	// The devices index a palette by a byte: a short one is padded to 256
	// with opaque black (a hand-made package's; every stock palette has 256),
	// and a count that is no palette refused
	int count = stream->ReadIndex();
	if (count < 0 || count > 65536)
		Exception::Throw("Palette " + Name.ToString() + " has " + std::to_string(count) + " colours");
	Colors.resize(count);
	stream->ReadBytes(Colors.data(), count * 4);

	if (stream->GetVersion() < 66)
	{
		for (uint32_t& c : Colors)
			c |= 0xff000000;
	}
	if (Colors.size() < 256)
		Colors.resize(256, 0xff000000);
}

void UPalette::Save(PackageStreamWriter* stream)
{
	UObject::Save(stream);
	stream->WriteIndex((int)Colors.size());
	stream->WriteBytes(Colors.data(), (uint32_t)Colors.size() * 4);
}

uint8_t UPalette::FindBestColor(const Color& color) const
{
	int r = color.R;
	int g = color.G;
	int b = color.B;
	int count = (int)Colors.size();
	const uint32_t* palette = Colors.data();
	int bestcolor = 0;
	int bestdist = 257 * 257 + 257 * 257 + 257 * 257;
	for (int color = 0; color < count; color++)
	{
		int sr = (palette[color] & 0xff);
		int sg = ((palette[color] >> 8) & 0xff);
		int sb = ((palette[color] >> 16) & 0xff);
		int x = r - sr;
		int y = g - sg;
		int z = b - sb;
		int dist = x * x + y * y + z * z;
		if (dist < bestdist)
		{
			if (dist == 0)
				return color;

			bestdist = dist;
			bestcolor = color;
		}
	}
	return bestcolor;
}
