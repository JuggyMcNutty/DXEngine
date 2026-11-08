#pragma once

#include "File.h"

class SHA1Sum
{
public:
	// The SHA1 sum of the file at filepath, in hex, or an empty string where
	// the file does not open or its read fails
	static std::string of_file(const fs::path& filepath);
};