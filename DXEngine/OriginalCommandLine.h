#pragma once

#include <string>

// The original's command line, as DeusEx.exe hands it to its engine: one
// string, which DXEngine takes from --cmdline= -- the recreated launcher's
// run-game.sh passes the player's there. Its flags are found as the
// original's code finds them (dx-reverse-info's cli-flags.md) -- Parse's "X="
// anywhere but straight after a letter or digit, in any case, its value in
// double quotes or up to a space, tab, CR, LF or comma, as much of it as the
// original's buffer holds -- but for ParseParam, stricter by choice: "-X" or
// "/X" anywhere, ending at a space, a '?' (a URL's next option: the Host
// screen's relaunch puts -server there) or the end, where the original's
// checks nothing after the name.
// Both take '/' as a switch, and a Linux path is full of them: the original's
// rule would find -server in INI=/srv/server/x.ini. What DXEngine does with
// each flag is NATIVES.md's "The command line".
class OriginalCommandLine
{
public:
	static OriginalCommandLine& Get();

	OriginalCommandLine() = default;
	explicit OriginalCommandLine(const std::string& value) { Set(value); }

	void Set(const std::string& line);
	bool Given() const { return given; }
	const std::string& Line() const { return line; }

	bool Param(const std::string& name) const;

	// Parse's "X=" into the original's buffer of maxLen characters (Core.dll
	// 0x10146bc0): a quoted value to its closing quote, else to the first
	// space, tab, CR, LF or comma, of either its first maxLen - 1 characters
	// at most, one past U+FFFF two, as the original's UTF-16 counts it. True
	// whenever the name is found, the value empty or not.
	bool Value(const std::string& name, std::string& value, size_t maxLen) const;

	// Parse's int (Core.dll 0x10147730): appAtoi of the text straight after
	// "X=", nothing cut, no quote taken off; true whenever the name is found.
	bool Number(const std::string& name, int& value) const;

	// UGameEngine::Init's start URL: the first token -- the second when the
	// first is SERVER -- but only with -hax0r or -server, and not a token
	// starting with '-'. Empty for the game's own start (DX.dx).
	std::string StartURL() const;

private:
	std::string line;
	bool given = false;
};
