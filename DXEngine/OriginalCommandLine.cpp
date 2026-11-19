#include "Precomp.h"
#include "OriginalCommandLine.h"
#include "Utils/StrTools.h"
#include <cctype>
#include <cstring>

namespace
{
	bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
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

// The tokens by the original's ParseToken (StrTools::ue1_parse_token): blanks
// and tabs between them, a quoted run whole
std::string OriginalCommandLine::FirstToken() const
{
	size_t pos = 0;
	std::string token;
	if (!StrTools::ue1_parse_token(line, pos, token, 0))
		return {};
	if (StrTools::ue1_strfind(token, "SERVER") == 0 && token.size() == 6 && !StrTools::ue1_parse_token(line, pos, token, 0))
		return {};
	if (token[0] == '-')
		return {};
	return token;
}

std::string OriginalCommandLine::StartURL() const
{
	if (!Param("hax0r") && !Param("server"))
		return {};
	return FirstToken();
}
