#pragma once

#include "Packages/Core/UObject.h"

class UPalette;
class USound;
class UActor;
class UFont;

class UnrealMipmap
{
public:
	int Width = 0;
	int Height = 0;
	int UBits = 0;
	int VBits = 0;
	Array<uint8_t> Data;
};

// A texture's format: Deus Ex's, by its packages' byte (UnTex.h's TEXF_P8
// to TEXF_RGBA8), then DXEngine's own, which no package can name.
enum class TextureFormat : uint32_t
{
	P8 = 0x00,
	BGRA8_LM = 0x01,   // TEXF_RGBA7: light and fog maps
	R5G6B5 = 0x02,     // TEXF_RGB16
	BC1 = 0x03,        // TEXF_DXT1
	RGB8 = 0x04,
	BGRA8 = 0x05,      // TEXF_RGBA8
	RGBA32_F = 0x100,  // DXEngine's light and fog maps
	Unknown = 0x101    // a package's byte past TEXF_RGBA8: drawn white
};

class UBitmap : public UObject
{
public:
	using UObject::UObject;

	uint8_t& Format() { return Value<uint8_t>(PropOffsets_Bitmap.Format); }
	FixedArrayView<int, 2> InternalTime() { return FixedArray<int, 2>(PropOffsets_Bitmap.InternalTime); }
	Color& MaxColor() { return Value<Color>(PropOffsets_Bitmap.MaxColor); }
	Color& MipZero() { return Value<Color>(PropOffsets_Bitmap.MipZero); }
	UPalette*& Palette() { return Value<UPalette*>(PropOffsets_Bitmap.Palette); }
	uint8_t& UBits() { return Value<uint8_t>(PropOffsets_Bitmap.UBits); }
	int& UClamp() { return Value<int>(PropOffsets_Bitmap.UClamp); }
	int& USize() { return Value<int>(PropOffsets_Bitmap.USize); }
	uint8_t& VBits() { return Value<uint8_t>(PropOffsets_Bitmap.VBits); }
	int& VClamp() { return Value<int>(PropOffsets_Bitmap.VClamp); }
	int& VSize() { return Value<int>(PropOffsets_Bitmap.VSize); }
};
