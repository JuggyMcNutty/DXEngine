#pragma once

#include <cstdint>
#include <string>
#include <string_view>

class StrTools
{
public:
	static bool equals_ignore_case(const std::string_view& str1, const std::string_view& str2);
	static bool startswith(const std::string_view& str, const std::string_view& cmp, bool ignoreCase = false);
	// Finds first occurrence of ANY character given in characters in str. Returns std::string::npos if none of the characters can be found.
	static size_t find_first_of_any(const std::string& str, const std::string& characters);
	// The original's hash of a name, case aside (Core's appStrihash): its
	// CRC table (GCRCTable, polynomial 0x04C11DB7 from the top bit) run over
	// each character's two bytes, a-z in upper case. The flag base's 64
	// buckets and the AI event manager's 256 are chosen by it.
	static uint32_t ue1_strihash(const std::string_view& str);
	// An int as the original's appAtoi reads one (Core.dll 0x101235b0,
	// MSVCRT's _wtoi): blanks skipped, a sign, then the digits up to the
	// first that is not one, 0 without any; past an int's range held to its
	// ends (SweepVmConsole: 4294967297 is 2147483647).
	static int32_t ue1_atoi(const std::string_view& str);
	// A float as the original's "%f" writes it, the MSVC runtime's it links:
	// its digits exact to the sixth decimal, a 5 after the sixth rounding up
	// -- a tie away from zero, where the C library's rounds it to even --,
	// and infinity and NaN as that runtime spells them.
	static std::string float_text(double value);
};
