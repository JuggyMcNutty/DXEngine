
#include "Precomp.h"
#include "Utils/StrTools.h"
#include "UByteProperty.h"
#include "Packages/Core/UEnum.h"
#include "Utils/Convert.h"

UByteProperty::UByteProperty(NameString name, UClass* base, ObjectFlags flags) : UPropertyT(std::move(name), base, flags)
{
	ValueType = ExpressionValueType::ValueByte;
}

void UByteProperty::Load(ObjectStream* stream)
{
	UProperty::Load(stream);
	EnumType = stream->ReadObject<UEnum>();
}

void UByteProperty::Save(PackageStreamWriter* stream)
{
	UProperty::Save(stream);
	stream->WriteObject(EnumType);
}

void UByteProperty::LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header)
{
	ThrowIfTypeMismatch(header, UPT_Byte);
	*static_cast<uint8_t*>(data) = stream->ReadUInt8();
}

void UByteProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	*static_cast<uint8_t*>(data) = stream->ReadUInt8();
}

void UByteProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_Byte;
}

void UByteProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
	stream->WriteUInt8(*static_cast<uint8_t*>(data));
}

std::string UByteProperty::PrintValue(const void* data)
{
	uint8_t val = *(uint8_t*)data;
	if (EnumType && val < EnumType->ElementNames.size())
		return EnumType->ElementNames[val].ToString();

	return std::to_string(*(uint8_t*)data);
}

bool UByteProperty::IsDefaultValue(void* val)
{
	return *(uint8_t*)val == 0;
}

void UByteProperty::SetValueFromString(void* data, const std::string& valueString)
{
	ImportText(valueString, 0, data);
}

// The original's UByteProperty::ImportText (Core.dll 0x10165aa0): an enum's
// byte by the text's first token (ReadToken), one of the enum's names in any
// case, the position past it: "SK_Two junk" is SK_Two. A byte's number from
// digits at pos alone, no blank or sign before them, truncated to a byte
// (appAtoi), the position past them. For an enum the digits are looked for
// past the token, where a number's token leaves none: an enum is never set
// from a number (bugs.md VM.21). Anything else is refused, the value kept.
size_t UByteProperty::ImportText(const std::string& text, size_t pos, void* data)
{
	if (EnumType)
	{
		std::string token;
		if (!ReadToken(text, pos, token))
			return ImportFailed;
		// FName(token, FNAME_Find): no name for an empty token or None
		if (!token.empty() && !StrTools::equals_ignore_case(token, "None"))
		{
			for (size_t i = 0; i < EnumType->ElementNames.size(); i++)
			{
				if (StrTools::equals_ignore_case(EnumType->ElementNames[i].ToString(), token))
				{
					*(uint8_t*)data = (uint8_t)i;
					return pos;
				}
			}
		}
	}
	if (pos >= text.size() || text[pos] < '0' || text[pos] > '9')
		return ImportFailed;
	uint8_t value = 0;
	while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9')
		value = (uint8_t)(value * 10 + (text[pos++] - '0'));
	*(uint8_t*)data = value;
	return pos;
}

void UByteProperty::Mark(GCMarker& marker)
{
	UPropertyT<uint8_t>::Mark(marker);
	marker.MarkCode(EnumType);
}
