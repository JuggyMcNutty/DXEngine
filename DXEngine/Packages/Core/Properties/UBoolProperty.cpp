
#include "Precomp.h"
#include "Utils/StrTools.h"
#include "UBoolProperty.h"

UBoolProperty::UBoolProperty(NameString name, UClass* base, ObjectFlags flags) : UProperty(std::move(name), base, flags)
{
	ValueType = ExpressionValueType::ValueBool;
}

void UBoolProperty::LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header)
{
	ThrowIfTypeMismatch(header, UPT_Bool);
	SetBool(data, header.boolValue);
}

void UBoolProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	SetBool(data, stream->ReadUInt8() == 1); // Is this always a byte? Is it aligned? Bitfield stuff?
}

void UBoolProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_Bool;
	header.boolValue = GetBool(data);
}

void UBoolProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
}

void UBoolProperty::SaveStructMemberValue(void* data, PackageStreamWriter* stream)
{
	stream->WriteUInt8(GetBool(data) ? 1 : 0);
}

size_t UBoolProperty::ElementAlignment()
{
	return alignof(uint32_t);
}

size_t UBoolProperty::ElementSize()
{
	return sizeof(uint32_t);
}

void UBoolProperty::ConstructElement(void* data)
{
	SetBool(data, false);
}

void UBoolProperty::CopyConstructElement(void* data, const void* src)
{
	SetBool(data, GetBool(src));
}

void UBoolProperty::DestructElement(void* /*data*/)
{
	// Nothing to do here
}

void UBoolProperty::CopyElement(void* data, const void* src)
{
	SetBool(data, GetBool(src));
}

bool UBoolProperty::CompareElement(const void* v1, const void* v2)
{
	bool b1 = GetBool(v1);
	bool b2 = GetBool(v2);
	return b1 == b2;
}

bool UBoolProperty::CompareLessElement(const void* v1, const void* v2)
{
	bool b1 = GetBool(v1);
	bool b2 = GetBool(v2);
	return b1 < b2;
}

bool UBoolProperty::GetBool(const void* data) const
{
	uint32_t v = *static_cast<const uint32_t*>(data);
	return (v & DataOffset.BitfieldMask) != 0;
}

void UBoolProperty::SetBool(void* data, bool value)
{
	uint32_t& v = *static_cast<uint32_t*>(data);
	if (value)
		v = v | DataOffset.BitfieldMask;
	else
		v = v & ~DataOffset.BitfieldMask;
}

void UBoolProperty::SetValueFromString(void* data, const std::string& valueString)
{
	ImportText(valueString, 0, data);
}

// The original's UBoolProperty::ImportText (Core.dll 0x10166380): the text's
// first token (ReadToken), so "True,", "1.0" and a quoted "True" are true: 1
// or True in any case sets it, 0 or False clears it, the position past the
// token; any other token, an empty one too, is refused, the value kept
// (bugs.md VM.21). It takes Core.int's localized words too ([General] True
// and False), which in the game's int are True and False.
size_t UBoolProperty::ImportText(const std::string& text, size_t pos, void* data)
{
	std::string token;
	if (!ReadToken(text, pos, token))
		return ImportFailed;
	if (token == "1" || StrTools::equals_ignore_case(token, "True"))
		SetBool(data, true);
	else if (token == "0" || StrTools::equals_ignore_case(token, "False"))
		SetBool(data, false);
	else
		return ImportFailed;
	return pos;
}

bool UBoolProperty::IsDefaultValue(void* val)
{
	return GetBool(val) == false;
}

std::string UBoolProperty::PrintValue(const void* data)
{
	return GetBool(data) ? "True" : "False";
}
