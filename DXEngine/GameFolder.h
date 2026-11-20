#pragma once

struct GameLaunchInfo
{
	std::string gameName = "";				// Name of the game ("Deus Ex")
	std::string gameRootFolder = "";		// Path to the folder that contains all the subfolders and files
	std::string gameExecutableName = "";	// Name of the game executable ("DeusEx")
	std::string gameVersionString = "";		// The game's version ("1112fm" or "1002")
	bool executableUnknown = false;			// its executable of no known hash, the game 1112fm by its packages (bugs.md D.89)
	std::string url = "";					// The UnrealURL to launch upon startup
	std::string systemIniPath = "";			// --ini: the system ini to read and write, as the original's INI=
	std::string userIniPath = "";			// --userini: the user ini, as the original's USERINI=
	std::string defaultIniPath = "";		// HX.exe's DEFAULTINI=: the default ini, else Default.ini
	std::string defUserIniPath = "";		// HX.exe's DEFUSERINI=: the default user ini, else DefUser.ini
	std::string systemFolder = "";			// --system: the original's System, a mod's own that it runs from (else the game's)
	bool dedicatedServer = false;			// --server: no client, the map served, as the original's -SERVER
	bool lanPlay = false;					// --lanplay: a dedicated server's LAN tick rate, as the original's -LANPLAY
	std::string timelinePath = "";			// --timeline: keys and commands at set times, the harness's live mode (Timeline.h)
	std::string auditFolder = "";			// --audit-natives: the folder whose packages are found first (NativesAudit.h)
};

class GameFolderSelection
{
public:
	static void UpdateList();
	static GameLaunchInfo GetLaunchInfo(int selectedGame);

	static Array<GameLaunchInfo> Games;
	// Why no game was taken, for the error that says so
	static std::string NotFound;

private:
	static GameLaunchInfo ExamineFolder(const std::string& path);
	static Array<std::string> FindGameFolders();
};
