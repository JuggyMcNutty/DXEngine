
#include "Precomp.h"
#include "UIntProperty.h"
#include "Utils/Convert.h"

UIntProperty::UIntProperty(NameString name, UClass* base, ObjectFlags flags) : UPropertyT(std::move(name), base, flags)
{
	ValueType = ExpressionValueType::ValueInt;
}

void UIntProperty::LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header)
{
	ThrowIfTypeMismatch(header, UPT_Int);
	*static_cast<int32_t*>(data) = stream->ReadInt32();
}

void UIntProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	*static_cast<int32_t*>(data) = stream->ReadInt32();
}

void UIntProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_Int;
}

void UIntProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
	stream->WriteInt32(*static_cast<int32_t*>(data));
}

std::string UIntProperty::PrintValue(const void* data)
{
	return std::to_string(*(int32_t*)data);
}

bool UIntProperty::IsDefaultValue(void* val)
{
	return *(int*)val == 0;
}

void UIntProperty::SetValueFromString(void* data, const std::string& valueString)
{
	ImportText(valueString, 0, data);
}

// The original's UIntProperty::ImportText (Core.dll 0x10165f00): text whose
// first character is a '-' or a digit read by appAtoi ("-" and "--5" are 0),
// then the run of '-' and digits passed; any other start, a '+', a blank or
// no text, leaves the value as it was (bugs.md VM.21). It refuses nothing.
size_t UIntProperty::ImportText(const std::string& text, size_t pos, void* data)
{
	auto number = [](char c) { return c == '-' || (c >= '0' && c <= '9'); };
	if (pos < text.size() && number(text[pos]))
		*(int32_t*)data = (int32_t)strtol(text.c_str() + pos, nullptr, 10);
	while (pos < text.size() && number(text[pos]))
		pos++;
	return pos;
}
