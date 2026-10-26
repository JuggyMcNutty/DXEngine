
#include "Precomp.h"
#include <cctype>
#include "UStrProperty.h"

UStrProperty::UStrProperty(NameString name, UClass* base, ObjectFlags flags) : UPropertyT(std::move(name), base, flags)
{
	ValueType = ExpressionValueType::ValueString;
}

void UStrProperty::LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header)
{
	if (header.type == UPT_Str)
	{
		*static_cast<std::string*>(data) = stream->ReadString();
	}
	else if (header.type == UPT_String)
	{
		int len = header.size;
		Array<char> s;
		s.resize(len);
		stream->ReadBytes(s.data(), (int)s.size());
		s.push_back(0);
		*static_cast<std::string*>(data) = s.data();
	}
	else
	{
		Exception::Throw("Property value does not match property type!");
	}
}

void UStrProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	*static_cast<std::string*>(data) = stream->ReadString();
}

void UStrProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_Str;
}

void UStrProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
	stream->WriteString(*static_cast<std::string*>(data));
}

void UStrProperty::SetValueFromString(void* data, const std::string& valueString)
{
	*(std::string*)data = valueString;
}

// A struct member's or an array element's text, as the original's delimited
// import reads it (UStrProperty::ImportText, Core.dll 0x10167980, by
// ParseToken): a quoted string's content, its escapes undone, else one bare
// token -- a run of letters, digits, _ and -, or a single other character
void UStrProperty::SetDelimitedValueFromString(void* data, const std::string& valueString)
{
	size_t p = valueString.find_first_not_of(' ');
	if (p == std::string::npos)
	{
		*(std::string*)data = std::string();
		return;
	}
	if (valueString[p] == '"')
	{
		*(std::string*)data = Unquote(valueString);
		return;
	}
	size_t q = p;
	while (q < valueString.size() && (std::isalnum((unsigned char)valueString[q]) || valueString[q] == '_' || valueString[q] == '-'))
		q++;
	if (q == p)
		q = p + 1;
	*(std::string*)data = valueString.substr(p, q - p);
}

std::string UStrProperty::PrintValue(const void* data)
{
	return '"' + *(std::string*)data + '"';
}

bool UStrProperty::IsDefaultValue(void* val)
{
	return ((std::string*)val)->length() == 0;
}
