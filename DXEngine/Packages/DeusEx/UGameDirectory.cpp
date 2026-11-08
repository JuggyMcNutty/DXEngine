
#include "Precomp.h"
#include <limits>
#include <algorithm>
#include <cctype>
#include <set>
#include "UGameDirectory.h"
#include "Utils/Logger.h"
#include "Utils/StrTools.h"
#include "Utils/File.h"
#include "Engine.h"
#include "Package/PackageManager.h"

void UDXGameDirectory::GetGameDirectory()
{
	if (GameDirectoryType() == EGameDirectoryTypes::GD_Maps)
	{
		currentDirectory = Directory::resolveCase(fs::path(engine->LaunchInfo.gameRootFolder) / "Maps");
		PopulateDirectoryList();
	}
	else
	{
		currentDirectory = engine->packages->GetSaveFolderPath();
		PopulateDirectoryList();
	}
}

int UDXGameDirectory::GetNewSaveFileIndex()
{
	// The highest existing SaveNNNN plus one, never a gap (the original's).
	return engine->NextSaveSlot();
}

// The original's (DeusEx.dll 0x10017d60): "Save%04d.<the save extension>"
// for any index -- DXEngine's own saves are folders named by the slot, the
// quick save's and Current apart -- and a new name takes a new index for -1
// or none (0x10017e30)
std::string UDXGameDirectory::GenerateSaveFilename(int saveIndex)
{
	char name[64];
	snprintf(name, sizeof(name), "Save%04d.%s", saveIndex, engine->packages->GetSaveExtension().c_str());
	return name;
}

std::string UDXGameDirectory::GenerateNewSaveFileName(std::optional<int> newIndex)
{
	return GenerateSaveFilename(newIndex && *newIndex != -1 ? *newIndex : GetNewSaveFileIndex());
}

int UDXGameDirectory::GetDirCount()
{
	return (int)DirectoryList().size();
}

std::string UDXGameDirectory::GetDirFilename(int fileIndex)
{
	auto list = DirectoryList();
	if (fileIndex < 0 || (size_t)fileIndex >= list.size())
		return {};
	return ScriptToUTF8(list[fileIndex]);
}

void UDXGameDirectory::SetDirType(EGameDirectoryTypes newDirType)
{
	GameDirectoryType() = newDirType;
}

void UDXGameDirectory::SetDirFilter(const std::string& strFilter)
{
	CurrentFilter() = UTF8ToScript(strFilter);
}

// A slot's save info: -1 the quick save, any other number Save%04d
// (GetSaveIndexFolderName).
UDXSaveInfo* UDXGameDirectory::GetSaveInfo(int fileIndex)
{
	if (GameDirectoryType() != EGameDirectoryTypes::GD_SaveGames)
		return nullptr;
	return LoadSaveInfo(GetSaveIndexFolderName(fileIndex));
}

// The save info of the listing's i-th directory -- its place in the list,
// not its slot number (dx-reverse-info/deusex-dll.md, the save directory).
UDXSaveInfo* UDXGameDirectory::GetSaveInfoFromDirectoryIndex(int DirectoryIndex)
{
	auto list = DirectoryList();
	if (GameDirectoryType() != EGameDirectoryTypes::GD_SaveGames || DirectoryIndex < 0 || (size_t)DirectoryIndex >= list.size())
		return nullptr;
	return LoadSaveInfo(ScriptToUTF8(list[DirectoryIndex]));
}

// Loads a save directory's MyDeusExSaveInfo and keeps it, as the original
// keeps it, so DeleteSaveInfo can let it go.
UDXSaveInfo* UDXGameDirectory::LoadSaveInfo(const std::string& folderName)
{
	auto pkg = engine->packages->GetSaveInfoPackage(folderName);
	if (!pkg)
	{
		// A save made this session is not scanned yet.
		engine->packages->ScanSaveInfos();
		pkg = engine->packages->GetSaveInfoPackage(folderName);
	}
	if (!pkg)
		return nullptr;

	auto info = Cast<UDXSaveInfo>(pkg->GetUObject("DeusExSaveInfo", "MyDeusExSaveInfo"));
	if (info)
	{
		auto list = LoadedSaveInfoPointers();
		bool kept = false;
		for (auto& it : list)
			kept |= (it == info);
		if (!kept)
			list.push_back(info);
	}
	return info;
}

// One transient save info, made the first time and kept: the Save Game
// screen stamps the new save's row with its time (dx-reverse-info/deusex-dll.md,
// the save directory).
UDXSaveInfo* UDXGameDirectory::GetTempSaveInfo()
{
	if (!TempSaveInfo())
	{
		UClass* cls = engine->packages->FindClass("DeusEx.DeusExSaveInfo");
		TempSaveInfo() = Cast<UDXSaveInfo>(engine->packages->GetTransientPackage()->NewObject("DeusExSaveInfo", cls, ObjectFlags::Transient));
	}
	return TempSaveInfo();
}

// Lets go of a kept save info: its file and the object. Nothing is deleted
// on disk; the screens delete a save with the DELETEGAME console command.
void UDXGameDirectory::DeleteSaveInfo(UDXSaveInfo& saveInfo)
{
	auto list = LoadedSaveInfoPointers();
	for (size_t i = 0; i < list.size(); i++)
	{
		if (list[i] == &saveInfo)
		{
			for (size_t j = i + 1; j < list.size(); j++)
				list[j - 1] = list[j];
			list.Array->Resize(list.size() - 1);
			engine->packages->RemoveSaveInfoPackage(saveInfo.package->GetPackageName());
			return;
		}
	}
}

// Lets go of every kept save info. Nothing is deleted on disk.
void UDXGameDirectory::PurgeAllSaveInfo()
{
	auto list = LoadedSaveInfoPointers();
	for (size_t i = 0; i < list.size(); i++)
	{
		if (list[i])
			engine->packages->RemoveSaveInfoPackage(list[i]->package->GetPackageName());
	}
	list.Array->Resize(0);
}

// The free space of the save path's drive and a slot's size, both in KB, as
// the original's (dx-reverse-info/deusex-dll.md, the save directory). The screens
// ask a directory object for the free space before it has read any
// directory, so both go by the save path itself. The original cuts the KB
// to an int, which wraps past 2 TB free and would refuse every save;
// DXEngine holds it at the largest int, and space that cannot be read counts
// as that much, where the original's 0 would refuse every save too.
int UDXGameDirectory::GetSaveFreeSpace()
{
	const int capKB = std::numeric_limits<int>::max();
	std::error_code ec;
	fs::path path = engine->packages->GetSaveFolderPath();
	fs::space_info info = fs::space(path, ec);
	if (ec)
		info = fs::space(path.parent_path(), ec);
	if (ec)
		return capKB;
	return (int)std::min<uintmax_t>(info.available / 1024, (uintmax_t)capKB);
}

// Each of the slot's files cut to a whole KB and summed, as the original's
// (DeusEx.dll 0x100179d0); its folder found in any case.
int UDXGameDirectory::GetSaveDirectorySize(int saveIndex)
{
	std::error_code ec;
	int size = 0;
	for (auto& p : fs::directory_iterator(Directory::resolveCase(engine->packages->GetSaveFolderPath() / GetSaveIndexFolderName(saveIndex)), ec))
	{
		uintmax_t fileSize = p.is_regular_file(ec) ? p.file_size(ec) : 0;
		if (!ec)
			size += (int)(fileSize / 1024);
	}
	return size;
}

// A slot's folder as the original's GetSaveInfo and GetSaveDirectorySize
// name it (DeusEx.dll 0x10017eb0, 0x100179d0): -1 the quick save, any other
// number Save%04d -- -2 too, Save-002, not Current, which the Load and Save
// Game screens' row for a folder whose SaveInfo is missing asks for
// (SaveInfoMissing;;;;-2), and which no save makes but one to slot -2.
std::string UDXGameDirectory::GetSaveIndexFolderName(int saveIndex)
{
	return saveIndex == -1 ? engine->SaveSlotFolderName(-1) : engine->SaveNumberedFolderName(saveIndex);
}

// A folder's names in the order the original's FindFiles gives them on NTFS,
// by name without regard to case, a to z taken as upper case, as NTFS orders
// a folder: so the five of [\]^_ come after the letters, where lower case
// would put them before. A directory's own order on Linux is none.
static void SortAsFound(std::vector<std::string>& names)
{
	std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
		return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
			[](char x, char y) { return std::toupper((unsigned char)x) < std::toupper((unsigned char)y); });
	});
}

void UDXGameDirectory::PopulateDirectoryList()
{
	auto list = DirectoryList();
	list.Array->Resize(0);

	if (GameDirectoryType() == EGameDirectoryTypes::GD_Maps)
	{
		// The map files ("*.<MapExt>") of every map folder of the paths, as
		// the original's GetMapsDirectory (DeusEx.dll 0x10017200) finds
		// them: a mod's maps in its own folder too, and nothing else; a name
		// listed already, in any case, passed over, as the original's
		// passes it over, so a map in two folders, or a folder the paths
		// name twice, is listed once
		const std::string mapExt = "." + engine->packages->GetMapExtension();
		std::set<std::string> listed;
		for (const std::string& folder : engine->packages->GetMapFolders())
		{
			if (!fs::exists(folder) || !fs::is_directory(folder))
				continue;
			std::vector<std::string> names;
			for (auto& p : fs::directory_iterator(folder))
				if (p.is_regular_file() && StrTools::equals_ignore_case(p.path().extension().string(), mapExt))
					names.push_back(p.path().filename().string());
			SortAsFound(names);
			for (const std::string& name : names)
			{
				std::string folded = name;
				for (char& c : folded)
					c = (char)std::toupper((unsigned char)c);
				if (listed.insert(folded).second)
					list.push_back(UTF8ToScript(name));
			}
		}
		return;
	}

	if (!fs::exists(currentDirectory) || !fs::is_directory(currentDirectory))
		return;

	{
		// The save listing is the Save* directories, in any case, as the
		// original's finds them (GetSaveGamesDirectory, DeusEx.dll 0x10017520):
		// Save-003 of a SaveGame(-3) among them; the quick save is asked for
		// by slot (-1), and neither it nor Current is listed.
		std::vector<std::string> names;
		for (auto& p : fs::directory_iterator(currentDirectory))
		{
			if (!p.is_directory())
				continue;
			const std::string name = p.path().filename().string();
			if (name.size() >= 4 && StrTools::equals_ignore_case(name.substr(0, 4), "Save"))
				names.push_back(name);
		}
		SortAsFound(names);
		for (const std::string& name : names)
			list.push_back(UTF8ToScript(name));
	}
}
