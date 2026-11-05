
#include "Precomp.h"
#include "UFont.h"
#include "Textures/UTexture.h"

// The original's (Engine.dll UFont::Serialize 0x10385400): the object's
// properties, its pages (each a texture and its characters), then
// CharactersPerPage
void UFont::Load(ObjectStream* stream)
{
	UObject::Load(stream);

	pages.resize(stream->ReadIndex());
	for (FontPage& page : pages)
	{
		page.Texture = stream->ReadObject<UTexture>();
		page.Characters.resize(stream->ReadIndex());
		for (FontCharacter& character : page.Characters)
		{
			character.StartU = stream->ReadInt32();
			character.StartV = stream->ReadInt32();
			character.USize = stream->ReadInt32();
			character.VSize = stream->ReadInt32();
		}
	}

	charactersPerPage = stream->ReadUInt32();
}

void UFont::Save(PackageStreamWriter* stream)
{
	UObject::Save(stream);

	stream->WriteIndex((int)pages.size());
	for (FontPage& page : pages)
	{
		stream->WriteObject(page.Texture);
		stream->WriteIndex((int)page.Characters.size());
		for (FontCharacter& character : page.Characters)
		{
			stream->WriteInt32(character.StartU);
			stream->WriteInt32(character.StartV);
			stream->WriteInt32(character.USize);
			stream->WriteInt32(character.VSize);
		}
	}

	stream->WriteUInt32(charactersPerPage);
}

// A 16-bit character through the pages in turn; an 8-bit one in none, a to z
// as A to Z, and else a space. One past 8 bits in none is nothing, no room
// taken, as the original's canvas measures it (TileConsole: U+0100 and
// U+03A9 of FontMenuHeaders 0 wide).
FontGlyph UFont::GetGlyph(char16_t c) const
{
	FontGlyph glyph = FindGlyph(c);
	if (glyph.USize == 0 && c > 0xFF)
		return {};
	if (glyph.USize == 0)
	{
		if (c >= 'a' && c <= 'z')
		{
			c += 'A' - 'a';
			glyph = FindGlyph(c);
		}

		if (glyph.USize == 0)
			glyph = FindGlyph(32);
	}
	return glyph;
}

// A character as Extension's GC finds it (XGC's glyph lookup, Extension.dll
// 0x10026880): on page c / CharactersPerPage at c % CharactersPerPage, and
// one not there has no size -- it draws nothing and takes no room.
FontGlyph UFont::GetPageGlyph(char16_t c) const
{
	if (charactersPerPage <= 0)
		return {};
	size_t page = c / charactersPerPage;
	size_t index = c % charactersPerPage;
	if (page >= pages.size() || index >= pages[page].Characters.size())
		return {};
	return { pages[page].Texture, pages[page].Characters[index] };
}

FontGlyph UFont::FindGlyph(char16_t c) const
{
	size_t index = c;
	for (auto& page : pages)
	{
		if (index < page.Characters.size())
		{
			return { page.Texture, page.Characters[index] };
		}
		index -= page.Characters.size();
	}
	return {};
}

void UFont::Mark(GCMarker& marker)
{
	UObject::Mark(marker);
	marker.SetField("pages");
	for (FontPage& page : pages)
		marker.Mark(page.Texture);
}
