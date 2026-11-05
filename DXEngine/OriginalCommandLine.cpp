#include "Precomp.h"
#include "OriginalCommandLine.h"
#include "Utils/StrTools.h"
#include <cctype>
#include <cstring>

namespace
{
	bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

	bool IsAsciiAlnum(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }

	// A character's length in the original's wide line, at its UTF-8 lead
	// byte: a continuation byte none, a four-byte character two (its UTF-16
	// pair).
	size_t Utf16Units(char c)
	{
		unsigned char u = (unsigned char)c;
		return (u & 0xc0) == 0x80 ? 0 : u >= 0xf0 ? 2 : 1;
	}

	// appStrfind: a substring in any case, from pos, but none straight after a
	// letter or digit -- INI= is not found within USERINI= (dx-reverse-info
	// cli-flags.md); npos when there is none.
	size_t FindNoCase(const std::string& hay, const std::string& needle, size_t pos)
	{
		if (needle.empty())
			return std::string::npos;
		for (size_t i = pos; i + needle.size() <= hay.size(); i++)
		{
			if (i > 0 && IsAsciiAlnum(hay[i - 1]))
				continue;
			size_t k = 0;
			while (k < needle.size() && std::tolower((unsigned char)hay[i + k]) == std::tolower((unsigned char)needle[k]))
				k++;
			if (k == needle.size())
				return i;
		}
		return std::string::npos;
	}

	// ParseToken: the next word, or a quoted run; false at the end.
	bool NextToken(const std::string& s, size_t& pos, std::string& token)
	{
		while (pos < s.size() && IsSpace(s[pos]))
			pos++;
		if (pos >= s.size())
			return false;
		token.clear();
		if (s[pos] == '"')
		{
			pos++;
			while (pos < s.size() && s[pos] != '"')
				token += s[pos++];
			if (pos < s.size())
				pos++;
		}
		else
		{
			while (pos < s.size() && !IsSpace(s[pos]))
				token += s[pos++];
		}
		return true;
	}
}

OriginalCommandLine& OriginalCommandLine::Get()
{
	static OriginalCommandLine instance;
	return instance;
}

void OriginalCommandLine::Set(const std::string& value)
{
	line = value;
	given = true;
}

bool OriginalCommandLine::Param(const std::string& name) const
{
	// Past the line's first character, as ParseParam starts its search there.
	// The name ending at a space, a '?' or the end is DXEngine's own test, by
	// choice (OriginalCommandLine.h): the original's checks nothing after it.
	for (size_t at = FindNoCase(line, name, 1); at != std::string::npos; at = FindNoCase(line, name, at + 1))
	{
		char before = line[at - 1];
		size_t end = at + name.size();
		if ((before == '-' || before == '/') && (end == line.size() || IsSpace(line[end]) || line[end] == '?'))
			return true;
	}
	return false;
}

bool OriginalCommandLine::Value(const std::string& name, std::string& value, size_t maxLen) const
{
	size_t at = FindNoCase(line, name + "=", 0);
	if (at == std::string::npos)
		return false;
	// Parse copies the value's first maxLen - 1 characters (appStrncpy,
	// Core.dll 0x101233c0), past the quote of a quoted one, then cuts it at
	// its first quote, or one not quoted at its first space, CR, LF, tab or
	// comma (0x10146c4e to 0x10146c96).
	size_t pos = at + name.size() + 1;
	bool quoted = pos < line.size() && line[pos] == '"';
	if (quoted)
		pos++;
	const char* stops = quoted ? "\"" : " \r\n\t,";
	value.clear();
	for (size_t units = 0; pos < line.size() && !std::strchr(stops, line[pos]); pos++)
	{
		units += Utf16Units(line[pos]);
		if (units >= maxLen)
			break;
		value += line[pos];
	}
	return true;
}

bool OriginalCommandLine::Number(const std::string& name, int& value) const
{
	size_t at = FindNoCase(line, name + "=", 0);
	if (at == std::string::npos)
		return false;
	value = StrTools::ue1_atoi(std::string_view(line).substr(at + name.size() + 1));
	return true;
}

std::string OriginalCommandLine::StartURL() const
{
	if (!Param("hax0r") && !Param("server"))
		return {};
	size_t pos = 0;
	std::string token;
	if (!NextToken(line, pos, token))
		return {};
	if (FindNoCase(token, "SERVER", 0) == 0 && token.size() == 6 && !NextToken(line, pos, token))
		return {};
	if (token.empty() || token[0] == '-')
		return {};
	return token;
}
