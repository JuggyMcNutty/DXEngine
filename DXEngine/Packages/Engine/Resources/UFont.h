#pragma once

#include "Textures/UTexture.h"

class UTexture;

class FontCharacter
{
public:
	int StartU = 0;
	int StartV = 0;
	int USize = 0;
	int VSize = 0;
};

class FontPage
{
public:
	UTexture* Texture = nullptr;
	Array<FontCharacter> Characters;
};

class FontGlyph
{
public:
	FontGlyph() = default;
	FontGlyph(UTexture* texture, const FontCharacter& c) : Texture(texture), StartU(c.StartU), StartV(c.StartV), USize(c.USize), VSize(c.VSize) {}

	UTexture* Texture = nullptr;
	int StartU = 0;
	int StartV = 0;
	int USize = 0;
	int VSize = 0;
};

// The original's Font is an Object: its pages and their characters, each
// page's texture an object of its own (Engine.dll UFont::Serialize
// 0x10385400)
class UFont : public UObject
{
public:
	using UObject::UObject;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;
	void Mark(GCMarker& marker) override;

	FontGlyph GetGlyph(char16_t c) const;
	FontGlyph GetPageGlyph(char16_t c) const;

private:
	FontGlyph FindGlyph(char16_t c) const;

	Array<FontPage> pages;
	int charactersPerPage = 0;
};
