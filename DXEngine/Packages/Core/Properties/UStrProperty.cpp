
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

// The original's UStrProperty::Identical (Core.dll 0x101676f0): appStricmp
// (0x10123560), the C library's _wcsicmp, which in the C locale, the one the
// game runs in, takes A to Z as a to z and every other character as it is.
// What compares a property's values by it -- a save's and the travel's
// against the class's defaults, replication's against what the other side
// last got, a struct's == and != member by member -- takes two strings apart
// in case alone as the same; the script's own == takes them by each
// character (EqualEqual_StrStr).
bool UStrProperty::CompareElement(const void* v1, const void* v2)
{
	return ScriptEqualsIgnoreCase(*static_cast<const ScriptString*>(v1), *static_cast<const ScriptString*>(v2));
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

// Delimited, the original's ReadToken from pos (Core.dll 0x10167980,
// 0x10164cb0): a quoted string's content, its escapes undone, the position
// past its closing quote; a run of letters, digits, _ and -; else the one
// character there, the position left on it. A quoted string left open is
// refused.
size_t UStrProperty::ImportDelimitedText(const std::string& text, size_t pos, void* data)
{
	std::string token;
	if (!ReadToken(text, pos, token))
		return ImportFailed;
	*(ScriptString*)data = UTF8ToScript(token);
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
