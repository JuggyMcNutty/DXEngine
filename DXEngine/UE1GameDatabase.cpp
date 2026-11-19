
#include "Precomp.h"
#include "Utils/File.h"
#include "UE1GameDatabase.h"
#include "Utils/SHA1Sum.h"
#include "Utils/StrTools.h"
#include <iostream>

// The GUID in a package's header, as Package::Open reads it (from version
// 68 after the tables' counts, before it the last of its heritage), its
// header alone read; "" for a file that is no package or is cut short
static std::string PackageGuid(const fs::path& path)
{
	try
	{
		auto file = File::try_open_existing(path.string());
		if (!file)
			return {};
		if (file->read_uint32() != 0x9E2A83C1)
			return {};
		uint16_t version = file->read_uint16();
		file->seek(36);
		uint8_t guid[16];
		if (version < 68)
		{
			uint32_t heritageCount = file->read_uint32();
			uint32_t heritageOffset = file->read_uint32();
			if (heritageCount == 0)
				return {};
			file->seek((int64_t)heritageOffset + ((int64_t)heritageCount - 1) * 16);
		}
		file->read(guid, 16);
		char text[33];
		uint32_t d[4];
		for (int i = 0; i < 4; i++)
			d[i] = guid[i * 4] | (guid[i * 4 + 1] << 8) | (guid[i * 4 + 2] << 16) | ((uint32_t)guid[i * 4 + 3] << 24);
		snprintf(text, sizeof(text), "%08X%08X%08X%08X", d[0], d[1], d[2], d[3]);
		return text;
	}
	catch (const std::exception&)
	{
		return {};
	}
}

std::string WhyNot1112fmPackages(const fs::path& systemFolder)
{
	for (const char* name : { "Core.u", "Engine.u", "DeusEx.u" })
	{
		fs::path path = Directory::resolveCase(systemFolder / name);
		std::error_code ec;
		if (!fs::is_regular_file(path, ec))
			return std::string("System/") + name + " is not there";
		std::string guid = PackageGuid(path);
		if (guid.empty())
			return std::string("System/") + name + " is no package";
		bool known = false;
		for (const UE1PackageGuid& package : Packages1112fm)
			known = known || (StrTools::equals_ignore_case(package.file, name) && StrTools::equals_ignore_case(package.guid, guid));
		if (!known)
			return std::string("System/") + name + " is not 1112fm's";
	}
	return {};
}

UE1GameFound FindUE1GameInPath(const std::string& ue1_game_root_folder_path)
{
	if (ue1_game_root_folder_path.empty())
		return {};

	if (!fs::exists(ue1_game_root_folder_path) || !fs::is_directory(ue1_game_root_folder_path))
		return {};

	// The System folder in any case: a copy recased on a case-sensitive file
	// system is the game still, as Windows finds it
	const auto UE1GameSystemPath = Directory::resolveCase(fs::path(ue1_game_root_folder_path) / "System");

	for (auto& executable_name : knownUE1ExecutableNames)
	{
		// The executable in any case too, its name kept as the database's
		auto executablePath = Directory::resolveCase(UE1GameSystemPath / executable_name);

		std::error_code ec;
		if (!fs::is_regular_file(executablePath, ec))
			continue;

		// Empty for a file that cannot be read: no hash of the database's
		std::string sha1sum = SHA1Sum::of_file(executablePath);

		// Now check whether there is a match within the database or not
		const auto it = SHA1Database.find(sha1sum);
		
		if (it != SHA1Database.end())
			return { it->second, executable_name, false };

		// An executable of no hash here beside 1112fm's packages: 1112fm,
		// the executable never run (bugs.md D.89)
		if (WhyNot1112fmPackages(UE1GameSystemPath).empty())
			return { KnownUE1Games::DEUS_EX_1112fm, executable_name, true };

		return {};
	}

	// We got nothing here
	return {};
}