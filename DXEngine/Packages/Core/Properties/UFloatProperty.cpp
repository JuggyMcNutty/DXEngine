
#include "Precomp.h"
#include "Utils/StrTools.h"
#include "UFloatProperty.h"
#include "Utils/Convert.h"

UFloatProperty::UFloatProperty(NameString name, UClass* base, ObjectFlags flags) : UPropertyT(std::move(name), base, flags)
{
	ValueType = ExpressionValueType::ValueFloat;
}

void UFloatProperty::LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header)
{
	ThrowIfTypeMismatch(header, UPT_Float);
	*static_cast<float*>(data) = stream->ReadFloat();
}

void UFloatProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	*static_cast<float*>(data) = stream->ReadFloat();
}

void UFloatProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_Float;
}

void UFloatProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
	stream->WriteFloat(*static_cast<float*>(data));
}

std::string UFloatProperty::PrintValue(const void* data)
{
	return StrTools::float_text(*(float*)data);
}

bool UFloatProperty::IsDefaultValue(void* val)
{
	return *(float*)val == 0.0f;
}

void UFloatProperty::SetValueFromString(void* data, const std::string& valueString)
{
	ImportText(valueString, 0, data);
}

// The original's UFloatProperty::ImportText (Core.dll 0x10166940): appAtof
// (atof, 0x101235c0), the number the text starts with, 0 for any other text,
// then on to the next ',', ')', CR or LF, or the text's end; it refuses
// nothing
size_t UFloatProperty::ImportText(const std::string& text, size_t pos, void* data)
{
	*(float*)data = (float)strtod(text.c_str() + std::min(pos, text.size()), nullptr);
	size_t end = text.find_first_of(",)\r\n", pos);
	return end == std::string::npos ? text.size() : end;
}
