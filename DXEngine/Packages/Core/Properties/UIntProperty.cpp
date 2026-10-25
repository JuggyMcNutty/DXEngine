
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
	// As the original's import (core-dll.md, configuration): text that
	// starts with a digit or a sign is read as far as it is a number, and
	// any other text, an empty one too, leaves the value as it was
	if (!valueString.empty() && (valueString.front() == '-' || valueString.front() == '+' || (valueString.front() >= '0' && valueString.front() <= '9')))
		*(int32_t*)data = (int32_t)strtol(valueString.c_str(), nullptr, 10);
}
