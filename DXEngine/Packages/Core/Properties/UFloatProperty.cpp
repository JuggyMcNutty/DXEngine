
#include "Precomp.h"
#include "Utils/StrTools.h"
#include "UFloatProperty.h"
#include "Utils/Convert.h"
#include "Math/floating.h"

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

// Identical against no default, as against 0 (Core.dll 0x10166780)
bool UFloatProperty::IsDefaultValue(void* val)
{
	return X87::Equal(*(float*)val, 0.0);
}

// The original's UFloatProperty::Identical (Core.dll 0x10166780): an fcomp
// and a test of the x87's C3 flag (X87::Equal), so a NaN is identical to
// anything. What compares a property's values by it -- a save's and the
// travel's against the class's defaults, the script classes' replication
// against what the other side last got (the native lists' by their bits,
// NetChannel's SameBits), a remote call's argument against none, a
// struct's == and != member by member -- takes a NaN for unchanged: a save
// and the travel leave it out, so it comes back as the default, it is not
// sent, and a struct holding one equals another but for it.
bool UFloatProperty::CompareElement(const void* v1, const void* v2)
{
	return X87::Equal(*static_cast<const float*>(v1), *static_cast<const float*>(v2));
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
