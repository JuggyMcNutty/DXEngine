#include "Precomp.h"
#include "OriginalCommandLine.h"
#include "Utils/StrTools.h"
#include <cctype>
#include <cstring>

namespace
{
	bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

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
	for (size_t at = StrTools::ue1_strfind(line, name, 1); at != std::string::npos; at = StrTools::ue1_strfind(line, name, at + 1))
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
	return StrTools::ue1_parse(line, name + "=", value, maxLen);
}

bool OriginalCommandLine::Number(const std::string& name, int& value) const
{
	size_t at = StrTools::ue1_strfind(line, name + "=");
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
	if (StrTools::ue1_strfind(token, "SERVER") == 0 && token.size() == 6 && !NextToken(line, pos, token))
		return {};
	if (token.empty() || token[0] == '-')
		return {};
	return token;
}
