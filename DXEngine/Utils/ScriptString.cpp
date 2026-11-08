#include "Precomp.h"
#include "ScriptString.h"

ScriptString Latin1ToScript(std::string_view text)
{
	ScriptString result;
	result.reserve(text.size());
	for (char c : text)
		result.push_back((char16_t)(uint8_t)c);
	return result;
}

std::string ScriptToLatin1(std::u16string_view text, char bad)
{
	std::string result;
	result.reserve(text.size());
	for (char16_t c : text)
		result.push_back(c <= 0xFF ? (char)(uint8_t)c : bad);
	return result;
}

// UTF-8's characters, each in its shortest form and no further than
// U+10FFFF; any other byte, a longer form's lead among them (C0 A9 for ')',
// E0 80 80 for a NUL) or one past U+10FFFF's (F4 90 on, F5 to F7), is a
// character of its own, Latin-1 (bugs.md ENG.35). A surrogate's three bytes
// are that 16-bit unit, as ScriptToUTF8 writes a lone one.
ScriptString UTF8ToScript(std::string_view text)
{
	static const uint32_t least[4] = { 0, 0x80, 0x800, 0x10000 };
	ScriptString result;
	result.reserve(text.size());
	size_t i = 0;
	while (i < text.size())
	{
		uint8_t c = (uint8_t)text[i];
		int extra = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
		uint32_t cp = extra > 0 ? (uint32_t)(c & (0x3F >> extra)) : c;
		bool valid = extra >= 0 && i + extra < text.size();
		for (int k = 1; valid && k <= extra; k++)
		{
			uint8_t next = (uint8_t)text[i + k];
			if ((next & 0xC0) != 0x80)
				valid = false;
			cp = (cp << 6) | (next & 0x3F);
		}
		if (valid && (cp < least[extra] || cp > 0x10FFFF))
			valid = false;
		if (!valid)
		{
			// Not UTF-8: the byte as itself
			result.push_back((char16_t)c);
			i++;
			continue;
		}
		if (cp > 0xFFFF)
		{
			cp -= 0x10000;
			result.push_back((char16_t)(0xD800 + (cp >> 10)));
			result.push_back((char16_t)(0xDC00 + (cp & 0x3FF)));
		}
		else
		{
			result.push_back((char16_t)cp);
		}
		i += extra + 1;
	}
	return result;
}

std::string ScriptToUTF8(std::u16string_view text)
{
	std::string result;
	result.reserve(text.size());
	for (size_t i = 0; i < text.size(); i++)
	{
		uint32_t cp = text[i];
		if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] < 0xE000)
		{
			cp = 0x10000 + ((cp - 0xD800) << 10) + (text[i + 1] - 0xDC00);
			i++;
		}
		if (cp < 0x80)
		{
			result.push_back((char)cp);
		}
		else if (cp < 0x800)
		{
			result.push_back((char)(0xC0 | (cp >> 6)));
			result.push_back((char)(0x80 | (cp & 0x3F)));
		}
		else if (cp < 0x10000)
		{
			result.push_back((char)(0xE0 | (cp >> 12)));
			result.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
			result.push_back((char)(0x80 | (cp & 0x3F)));
		}
		else
		{
			result.push_back((char)(0xF0 | (cp >> 18)));
			result.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
			result.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
			result.push_back((char)(0x80 | (cp & 0x3F)));
		}
	}
	return result;
}

bool ScriptEqualsIgnoreCase(std::u16string_view a, std::u16string_view b)
{
	if (a.size() != b.size())
		return false;
	auto fold = [](char16_t c) { return c >= 'a' && c <= 'z' ? (char16_t)(c - 'a' + 'A') : c; };
	for (size_t i = 0; i < a.size(); i++)
	{
		if (fold(a[i]) != fold(b[i]))
			return false;
	}
	return true;
}
