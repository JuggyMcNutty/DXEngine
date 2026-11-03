#pragma once

#include <string>
#include <string_view>

// A script's string: 16-bit characters, as the original's are (its build is
// Unicode, each TCHAR a UTF-16 code unit: core-dll.md, strings). Names, paths,
// logs, ini text and the engine's own text stay 8-bit, converted at the edges.
using ScriptString = std::u16string;

// Each byte a character, as the original widens 8-bit text.
ScriptString Latin1ToScript(std::string_view text);

// Each character a byte; one past 0xFF becomes bad, as the original's ToAnsi
// narrows (0x7F) where it must, or the engine's own '?'.
std::string ScriptToLatin1(std::u16string_view text, char bad = '?');

// UTF-8 to 16-bit characters and back: a code point past 0xFFFF as a
// surrogate pair, and a lone surrogate kept as itself.
ScriptString UTF8ToScript(std::string_view text);
std::string ScriptToUTF8(std::u16string_view text);
