
#include "Precomp.h"
#include "SHA1Sum.h"
#include "TinySHA1/TinySHA1.hpp"

std::string SHA1Sum::of_file(const fs::path& filepath)
{
	// A file that does not open, or fails as it is read (a folder of its
	// name, an I/O error), has none (bugs.md ENG.38)
	try
	{
		auto bytes = File::read_all_bytes(filepath.string());

		sha1::SHA1 s;
		s.processBytes(bytes.data(), bytes.size());
		uint32_t digest[5];
		s.getDigest(digest);

		char temp[41];
		snprintf(temp, 41, "%08x%08x%08x%08x%08x", digest[0], digest[1], digest[2], digest[3], digest[4]);

		return std::string{temp};
	}
	catch (const std::exception&)
	{
		return "";
	}
}
