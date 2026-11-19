
#include "Precomp.h"
#include "GameFolder.h"
#include "Utils/File.h"
#include "Utils/UTF16.h"
#include "Utils/StrTools.h"
#include "UE1GameDatabase.h"
#include "Utils/CommandLine.h"
#include "OriginalCommandLine.h"
#include <filesystem>

Array<GameLaunchInfo> GameFolderSelection::Games;
std::string GameFolderSelection::NotFound;

// Why a folder given is not taken for the game, as FindUE1GameInPath looks
static std::string WhyNotTheGame(const std::string& folder)
{
	std::error_code ec;
	if (!fs::is_directory(fs::path(folder), ec))
		return "\"" + folder + "\" is no folder";
	fs::path system = Directory::resolveCase(fs::path(folder) / "System");
	for (const std::string& name : knownUE1ExecutableNames)
	{
		fs::path exe = Directory::resolveCase(system / name);
		if (fs::is_regular_file(exe, ec))
			return exe.string() + " is a build the engine does not know";
	}
	return "\"" + folder + "\" has no System folder with " + knownUE1ExecutableNames[0];
}

void GameFolderSelection::UpdateList()
{
	Games.clear();
	NotFound.clear();

	for (const std::string& folder : commandline->GetItems())
	{
		GameLaunchInfo game = ExamineFolder(folder);
		if (!game.gameName.empty())
			Games.push_back(game);
		else if (NotFound.empty())
			NotFound = "No UE1 game found in the given folder: " + WhyNotTheGame(folder);
	}

	// A folder given and not the game: no other is looked for, which would
	// start another install (bugs.md ENG.47)
	if (!commandline->GetItems().empty())
		return;

#ifdef WIN32
	NotFound = "No UE1 game found: no game folder given, the engine started in no game's System "
		"folder, and none named in the registry";
#else
	NotFound = "No UE1 game found: no game folder given, and the engine started in no game's System folder";
#endif

	if (Games.empty())
	{
		// Check if we're within an UE1-game System folder.
		auto p = std::filesystem::current_path();
		if (StrTools::equals_ignore_case(p.filename().string(), "System"))
		{
			GameLaunchInfo game = ExamineFolder(p.parent_path().string());
			if (!game.gameName.empty())
				Games.push_back(game);
		}
	}

	if (Games.empty())
	{
		for (const std::string& folder : FindGameFolders())
		{
			GameLaunchInfo game = ExamineFolder(folder);
			if (!game.gameName.empty())
				Games.push_back(game);
		}
	}
}

// The original's System: the folder its Core.dll is in, which its relative
// paths are taken from (dx-reverse-info core-dll.md, appBaseDir) -- the
// game's, or a mod's own that it runs from (--system), as the mod's launcher
// starts in it.
static fs::path SystemFolder(const GameLaunchInfo& info)
{
	if (!info.systemFolder.empty())
		return Directory::resolveCase(fs::path(info.systemFolder));
	return Directory::resolveCase(fs::path(info.gameRootFolder) / "System");
}

// The original's INI= and USERINI=: a path from System, the original's
// working folder, its folders and name found in any case, as Windows' file
// system finds them (dx-reverse-info cli-flags.md; the launchers find them
// so too). A part not there at all is kept as given: a new ini is made there.
static std::string OriginalIniPath(const GameLaunchInfo& info, const std::string& value)
{
	fs::path path = convert_path_separators(value);
	if (path.is_relative())
		path = SystemFolder(info) / path;
	return Directory::resolveCase(path.lexically_normal()).string();
}

GameLaunchInfo GameFolderSelection::GetLaunchInfo(int selectedGame)
{
	if (selectedGame < 0 || (size_t)selectedGame >= Games.size())
		return {};

	GameLaunchInfo info = Games[selectedGame];
	info.url = commandline->GetArg("-u", "--url", info.url);
	info.systemIniPath = commandline->GetArg("", "--ini", info.systemIniPath);
	info.userIniPath = commandline->GetArg("", "--userini", info.userIniPath);
	std::string system = commandline->GetArg("", "--system", "");
	if (!system.empty())
	{
		std::error_code ec;
		fs::path folder = fs::absolute(fs::path(convert_path_separators(system)), ec);
		if (!ec)
			info.systemFolder = Directory::resolveCase(folder.lexically_normal()).string();
	}
	info.dedicatedServer = commandline->HasArg("", "--server") || info.dedicatedServer;
	info.lanPlay = commandline->HasArg("", "--lanplay") || info.lanPlay;
	info.timelinePath = commandline->GetArg("", "--timeline", info.timelinePath);

	// The original's command line, when the launcher gave one: its start
	// URL, -server, -lanplay (a dedicated server's LanServerMaxTickRate,
	// network.md), and the inis its INI= and USERINI= name, each read into
	// appInit's 256 characters (Core.dll 0x10149a68, 0x10149b96), over
	// DXEngine's own options.
	const OriginalCommandLine& original = OriginalCommandLine::Get();
	if (original.Given())
	{
		std::string url = original.StartURL();
		if (!url.empty())
			info.url = url;
		if (original.Param("server"))
			info.dedicatedServer = true;
		if (original.Param("lanplay"))
			info.lanPlay = true;
		std::string ini;
		if (original.Value("INI", ini, 256) && !ini.empty())
			info.systemIniPath = OriginalIniPath(info, ini);
		if (original.Value("USERINI", ini, 256) && !ini.empty())
			info.userIniPath = OriginalIniPath(info, ini);
		// HX.exe's own words, its default inis (dx-reverse-info hx-dll.md,
		// HX.exe; bugs.md HX.12), which its name makes HXDefault.ini and
		// HXDefUser.ini: DXLauncher gives them for HX
		if (original.Value("DEFAULTINI", ini, 256) && !ini.empty())
			info.defaultIniPath = OriginalIniPath(info, ini);
		if (original.Value("DEFUSERINI", ini, 256) && !ini.empty())
			info.defUserIniPath = OriginalIniPath(info, ini);
	}
	return info;
}

GameLaunchInfo GameFolderSelection::ExamineFolder(const std::string& path)
{
	GameLaunchInfo info;

	if (path.empty())
		return info;

	auto ue1_game = FindUE1GameInPath(path);

	if (ue1_game.first != KnownUE1Games::UE1_GAME_NOT_FOUND)
	{
		info.gameRootFolder = path;
		info.gameExecutableName = fs::path(ue1_game.second).stem().string();

		switch (ue1_game.first)
		{
			case KnownUE1Games::DEUS_EX_1002f:
			{
				info.gameName = "Deus Ex";
				info.gameVersionString = "1002";
			}
			break;
			case KnownUE1Games::DEUS_EX_1112fm:
			{
				info.gameName = "Deus Ex";
				info.gameVersionString = "1112fm";
			}
			break;
			case KnownUE1Games::UE1_GAME_NOT_FOUND:
				// Do nothing, we want the fields to be empty.
			break;
		}
	}
	
	// If no game is found, this will be an empty GameLaunchInfo
	return info;
}

#ifdef WIN32

static std::string FindEpicRegisteredGame(const std::string& keyname)
{
	Array<wchar_t> buffer(1024);
	HKEY regkey = 0;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, to_utf16("SOFTWARE\\Unreal Technology\\Installed Apps\\" + keyname).c_str(), 0, KEY_READ, &regkey) == ERROR_SUCCESS)
	{
		DWORD type = 0;
		DWORD size = (DWORD)(buffer.size() * sizeof(wchar_t));
		LSTATUS result = RegQueryValueEx(regkey, L"Folder", 0, &type, (LPBYTE)buffer.data(), &size);
		RegCloseKey(regkey);
		if (result == ERROR_SUCCESS && type == REG_SZ)
		{
			buffer.back() = 0;
			return from_utf16(buffer.data());
		}
	}
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, to_utf16("SOFTWARE\\WOW6432Node\\Unreal Technology\\Installed Apps\\" + keyname).c_str(), 0, KEY_READ, &regkey) == ERROR_SUCCESS)
	{
		DWORD type = 0;
		DWORD size = (DWORD)(buffer.size() * sizeof(wchar_t));
		LSTATUS result = RegQueryValueEx(regkey, L"Folder", 0, &type, (LPBYTE)buffer.data(), &size);
		RegCloseKey(regkey);
		if (result == ERROR_SUCCESS && type == REG_SZ)
		{
			buffer.back() = 0;
			return from_utf16(buffer.data());
		}
	}
	return {};
}

Array<std::string> GameFolderSelection::FindGameFolders()
{
	return { FindEpicRegisteredGame("Deus Ex") };
}

#else

Array<std::string> GameFolderSelection::FindGameFolders()
{
	return {};
}

#endif
