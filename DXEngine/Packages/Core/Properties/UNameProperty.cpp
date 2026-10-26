
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

// The original's reader of a token (Core.dll 0x10164cb0, ReadToken, which
// UNameProperty::ImportText calls without dots): a quoted string, with \\
// and \xx (two hex digits, a digit that is none counting 0) escapes, to its
// closing quote; else, starting with a letter or digit, the run of letters,
// digits, _ and -; else the first character alone. A quoted string left
// open gives no token.
static bool ReadToken(const std::string& text, std::string& token)
{
	auto alnum = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); };
	auto hex = [](char c) { return (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : 0; };
	size_t i = 0, n = text.size();
	token.clear();
	if (n > 0 && text[0] == '"')
	{
		i = 1;
		while (i < n && text[i] != '"' && text[i] != '\r' && text[i] != '\n')
		{
			if (text[i] == '\\')
			{
				if (i + 1 < n && text[i + 1] == '\\')
				{
					token.push_back('\\');
					i += 2;
				}
				else
				{
					int high = i + 1 < n ? hex(text[i + 1]) : 0;
					int low = i + 2 < n ? hex(text[i + 2]) : 0;
					token.push_back((char)(16 * high + low));
					i += 3;
				}
			}
			else
				token.push_back(text[i++]);
		}
		return i < n && text[i] == '"';
	}
	if (n == 0 || !alnum(text[0]))
	{
		token.assign(1, n > 0 ? text[0] : '\0');
		return true;
	}
	while (i < n && (alnum(text[i]) || text[i] == '_' || text[i] == '-'))
		token.push_back(text[i++]);
	return true;
}

// UNameProperty::ImportText (Core.dll): the text's first token is the name;
// no token (a quoted string left open) sets nothing
void UNameProperty::SetValueFromString(void* data, const std::string& valueString)
{
	std::string token;
	if (!ReadToken(valueString, token))
		return;
	if (!token.empty() && token[0] == '\0')
		token.clear();
	*(NameString*)data = NameString(token);
}

std::string UNameProperty::PrintValue(const void* data)
{
	return ((NameString*)data)->ToString();
}

bool UNameProperty::IsDefaultValue(void* val)
{
	return static_cast<NameString*>(val)->IsNone();
}
