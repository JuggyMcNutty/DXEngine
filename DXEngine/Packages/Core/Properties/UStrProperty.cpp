
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
		*static_cast<ScriptString*>(data) = stream->ReadScriptString();
	}
	else if (header.type == UPT_String)
	{
		int len = header.size;
		// The tag's size, a 32-bit int from the file, held before the allocation
		if (len < 0 || len > 16 * 1024 * 1024)
			Exception::Throw("A string of " + std::to_string(len) + " bytes in " + Name.ToString());
		Array<char> s;
		s.resize(len);
		stream->ReadBytes(s.data(), (int)s.size());
		s.push_back(0);
		*static_cast<ScriptString*>(data) = Latin1ToScript(s.data());
	}
	else
	{
		Exception::Throw("Property value does not match property type!");
	}
}

void UStrProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	*static_cast<ScriptString*>(data) = stream->ReadScriptString();
}

void UStrProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_Str;
}

void UStrProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
	stream->WriteScriptString(*static_cast<ScriptString*>(data));
}

void UStrProperty::SetValueFromString(void* data, const std::string& valueString)
{
	*(ScriptString*)data = UTF8ToScript(valueString);
}

// The original's UStrProperty::ImportText (Core.dll 0x10167980), undelimited:
// the text from pos to its end, the position left where it was, so that an
// exec function's next parameter reads the same text again
size_t UStrProperty::ImportText(const std::string& text, size_t pos, void* data)
{
	*(ScriptString*)data = UTF8ToScript(pos < text.size() ? text.substr(pos) : std::string());
	return pos;
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
		*(ScriptString*)data = ScriptString();
		return;
	}
	if (valueString[p] == '"')
	{
		*(ScriptString*)data = UTF8ToScript(Unquote(valueString));
		return;
	}
	size_t q = p;
	while (q < valueString.size() && (std::isalnum((unsigned char)valueString[q]) || valueString[q] == '_' || valueString[q] == '-'))
		q++;
	if (q == p)
		q = p + 1;
	*(ScriptString*)data = UTF8ToScript(valueString.substr(p, q - p));
}

std::string UStrProperty::PrintValue(const void* data)
{
	return '"' + ScriptToUTF8(*(const ScriptString*)data) + '"';
}

bool UStrProperty::IsDefaultValue(void* val)
{
	return ((ScriptString*)val)->length() == 0;
}
