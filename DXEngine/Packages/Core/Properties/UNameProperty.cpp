
#include "Precomp.h"
#include "UNameProperty.h"

UNameProperty::UNameProperty(NameString name, UClass* base, ObjectFlags flags) : UPropertyT(std::move(name), base, flags)
{
	ValueType = ExpressionValueType::ValueName;
}

void UNameProperty::LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header)
{
	ThrowIfTypeMismatch(header, UPT_Name);
	*static_cast<NameString*>(data) = stream->ReadName();
}

void UNameProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	*static_cast<NameString*>(data) = stream->ReadName();
}

void UNameProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_Name;
}

void UNameProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
	stream->WriteName(*static_cast<NameString*>(data));
}

void UNameProperty::SetValueFromString(void* data, const std::string& valueString)
{
	ImportText(valueString, 0, data);
}

// The original's UNameProperty::ImportText (Core.dll 0x101674d0): the text's
// first token, without dots (ReadToken), is the name, an empty one None: "-7"
// names -, "a b" a, "\"a b\"" a b; a quoted string left open is refused and
// sets nothing
size_t UNameProperty::ImportText(const std::string& text, size_t pos, void* data)
{
	std::string token;
	if (!ReadToken(text, pos, token))
		return ImportFailed;
	*(NameString*)data = NameString(token);
	return pos;
}

std::string UNameProperty::PrintValue(const void* data)
{
	return ((NameString*)data)->ToString();
}

bool UNameProperty::IsDefaultValue(void* val)
{
	return static_cast<NameString*>(val)->IsNone();
}
