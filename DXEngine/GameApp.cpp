
#include "Precomp.h"
#include "Utils/Exception.h"
#include "Utils/Logger.h"
#include "Utils/CommandLine.h"
#include "GameApp.h"
#include "GameFolder.h"
#include "LauncherLine.h"
#include "OriginalCommandLine.h"
#include "Engine.h"
#include "UI/WidgetResourceData.h"
#include "UI/ErrorWindow/ErrorWindow.h"
#include "Utils/File.h"
#include "Utils/PEIcon.h"
#include "dxe_version.h"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <vector>
#ifndef _WIN32
#include <cerrno>
#include <unistd.h>
#else
#include <algorithm>
#include "Utils/UTF16.h"
#endif
#include "Utils/Logger.h"

// What a launcher can rely on this build to do, beyond the original's
// command line: --userdir; a start URL of ?loadgame=N, a save loaded; the
// .int files read from the paths' folders, a mod's from its own.
static const char* const Features = "userdir loadgame-url int-paths";

int GameApp::main(Array<std::string> args)
{
	// --version: which engine this is and what it can do, for a launcher
	// to check before it starts one (and before anything is loaded).
	for (const std::string& arg : args)
	{
		if (arg == "--version")
		{
			std::cout << "DXEngine " << DXE_VERSION << "\nfeatures: " << Features << std::endl;
			return 0;
		}
	}

	int exitcode = 0;
	std::string relaunchLine;
	GameLaunchInfo game; // once known, the error window shows its icon
	bool widgets = false; // the display backend, DXEngine.pk3 and the theme are up
	bool windowSystem = false; // a window can open: the error window's too
	// Read while the command line lives: the try block's end destroys it
	// before its handler runs
	bool noLauncher = false;

	try
	{
		CommandLine cmd(args);
		commandline = &cmd;
		noLauncher = commandline->HasArg("-n", "--no-launcher");

		// --userdir: the engine's own folder, in place of
		// ~/.config/DXEngine (Directory::engineUserDir).
		if (commandline->HasArg("", "--userdir"))
		{
			std::string userDir = commandline->GetArg("", "--userdir");
			if (userDir.empty())
				throw std::runtime_error("--userdir needs a folder: --userdir=<dir>");
			Directory::create(userDir);
			Directory::setEngineUserDir(userDir);
		}

		// The original's command line, from the launcher that started us, and
		// the line back to it (OriginalCommandLine, LauncherLine).
		if (commandline->HasArg("", "--cmdline"))
			OriginalCommandLine::Get().Set(commandline->GetArg("", "--cmdline"));
		LauncherLine::Get().Hello();

		// Stream the engine log to stderr in headless mode. Without this the
		// log only surfaces inside the modal error window, which is no use on
		// a test rig or an embedded target -- and LogUnimplemented() is exactly
		// how we find out which natives a game still needs.
		if (commandline->HasArg("-n", "--no-launcher") || commandline->HasArg("-v", "--verbose"))
		{
			Logger::Get()->SetCallback([](const LogMessageLine& line)
			{
				std::cerr << "[" << line.Source << "] " << line.Text << std::endl;
			});
		}

		// The display backend, DXEngine.pk3 and the theme, inside the try: a
		// failure here ("Could not open DXEngine.pk3", SDL's) is reported as
		// every other, with the prefix the launcher reads, after the hello
		// and with the log; before, it left through main's own catch.
		windowSystem = InitWidgetResources("dark");
		widgets = true;

		if (ErrorWindow::CheckCrashReporter())
			return 0;

		// Every option the engine takes but --showcrashreport, its own crash
		// report's relaunch (ENGINE.md, running it)
		if (commandline->HasArg("-h", "--help"))
		{
			std::cout <<
				"DXEngine [<options>] [<game folder>]\n"
				"  -u, --url=<map>       the map to start, or a server's address\n"
				"  --server              a dedicated server of the --url map\n"
				"  --lanplay             with --server, the LAN tick rate\n"
				"  --ini=<file>          the ini read and written back, as the original's INI=\n"
				"  --userini=<file>      the user ini, as the original's USERINI=\n"
				"  --cmdline=<line>      the original's command line, as a launcher passes it\n"
				"  --userdir=<dir>       the engine's own folder: its settings, log and shots\n"
				"  --timeline=<file>     keys, text, a pad, the pointer and commands at set times\n"
				"  -n, --no-launcher     the log on stderr, an error there alone, no error window\n"
				"  -v, --verbose         the log on stderr\n"
				"  --version             the engine's version and features\n"
				"  -h, --help            these options\n"
				"An option that takes a value takes the next word when it has no '='.\n"
				"With no game folder, the game whose System folder it starts in (on Windows,\n"
				"else the registry's).\n";
			return 0;
		}

		GameFolderSelection::UpdateList();
		if (GameFolderSelection::Games.empty())
		{
			// A failure, reported as every other (the error window, the
			// launcher's prefix, a failing exit code), not a clean quit
			// after one line on stdout
			throw std::runtime_error("No UE1 game found in the given folder");
		}
		else
		{
			std::cout << "Launching " << GameFolderSelection::Games[0].gameName
			          << " " << GameFolderSelection::Games[0].gameVersionString
			          << std::endl;
			GameLaunchInfo info = GameFolderSelection::GetLaunchInfo(0);
			game = info;
			Engine engine(info);
			engine.Run();
			relaunchLine = engine.RelaunchLine;
		}
		commandline = nullptr;
	}
	catch (const std::exception& e)
	{
		commandline = nullptr;

		// Always report to stderr as well as the GUI. On a headless test rig or
		// an embedded target there may be no window to read the message in, and
		// a modal error window with nobody to close it just spins.
		exitcode = 1;
		std::cerr << "DXEngine error: " << e.what() << std::endl;
		for (const LogMessageLine& line : Logger::Get()->GetLog())
			std::cerr << "  log: " << line.Text << std::endl;
		if (widgets && windowSystem && !noLauncher)
		{
			std::vector<std::shared_ptr<Image>> icon;
			if (!game.gameRootFolder.empty())
				icon = PEIcon::OfGame(game.gameRootFolder, game.gameExecutableName);
			ErrorWindow::ExecModal(e.what(), Logger::Get()->GetLog(), {}, icon);
		}
	}

	if (widgets)
		DeinitWidgetResources();
	if (exitcode == 0 && !relaunchLine.empty())
		Relaunch(args, relaunchLine);
	return exitcode;
}

void GameApp::Relaunch(const Array<std::string>& args, const std::string& line)
{
	// A RELAUNCH with no launcher to start (Engine::Relaunch): this engine
	// again, with the same options but the original's command line the
	// RELAUNCH named in place of its start (--cmdline, --url, --server,
	// --lanplay, --timeline).
	auto option = [](const std::string& arg, const char* name)
	{
		size_t n = std::strlen(name);
		return arg.compare(0, n, name) == 0 && (arg.size() == n || arg[n] == '=');
	};
	Array<std::string> next;
	for (size_t i = 0; i < args.size(); i++)
	{
		const std::string& arg = args[i];
		if (option(arg, "--cmdline") || option(arg, "--url") || option(arg, "-u") || option(arg, "--server") ||
			option(arg, "--lanplay") || option(arg, "--timeline"))
		{
			// its value as the next word (CommandLine) goes with it
			if (CommandLine::TakesValue(arg) && i + 1 < args.size() && !args[i + 1].empty() && args[i + 1][0] != '-')
				i++;
			continue;
		}
		next.push_back(arg);
	}
	next.push_back("--cmdline=" + line);

#ifndef _WIN32
	char path[4096];
	ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (length <= 0)
	{
		std::cerr << "Relaunch: the engine's own path is unknown" << std::endl;
		return;
	}
	path[length] = 0;
	std::vector<char*> argv;
	argv.push_back(path);
	for (std::string& arg : next)
		argv.push_back(arg.data());
	argv.push_back(nullptr);
	std::cerr << "Relaunch: " << path << " --cmdline=" << line << std::endl;
	execv(path, argv.data());
	std::cerr << "Relaunch: " << std::strerror(errno) << std::endl;
#else
	// Windows has no exec: the engine is started again, its output where
	// this one's goes -- its standard handles, and no others, handed on --
	// and this one exits. Each word is quoted as CommandLineToArgvW splits
	// the line.
	std::vector<wchar_t> path(32768);
	DWORD length = GetModuleFileNameW(nullptr, path.data(), (DWORD)path.size());
	if (length == 0 || length >= path.size())
	{
		std::cerr << "Relaunch: the engine's own path is unknown" << std::endl;
		return;
	}
	auto quote = [](const std::wstring& word)
	{
		if (!word.empty() && word.find_first_of(L" \t\n\v\"") == std::wstring::npos)
			return word;
		std::wstring quoted = L"\"";
		size_t backslashes = 0;
		for (wchar_t c : word)
		{
			if (c == L'\\')
			{
				backslashes++;
				continue;
			}
			// backslashes before a quote are doubled, and the quote escaped
			quoted.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
			quoted.push_back(c);
			backslashes = 0;
		}
		quoted.append(backslashes * 2, L'\\');
		return quoted + L"\"";
	};
	std::wstring commandLine = L"\"" + std::wstring(path.data()) + L"\"";
	for (const std::string& arg : next)
		commandLine += L" " + quote(to_utf16(arg));

	HANDLE standard[3] = { GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE) };
	std::vector<HANDLE> inherit;
	for (HANDLE h : standard)
	{
		if (h && h != INVALID_HANDLE_VALUE && std::find(inherit.begin(), inherit.end(), h) == inherit.end() &&
			SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
			inherit.push_back(h);
	}
	SIZE_T size = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
	std::vector<char> attributes(size);
	auto list = (LPPROC_THREAD_ATTRIBUTE_LIST)attributes.data();
	STARTUPINFOEXW startup = {};
	startup.StartupInfo.cb = sizeof(startup);
	bool listed = !inherit.empty() && InitializeProcThreadAttributeList(list, 1, 0, &size);
	if (listed && UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit.data(), inherit.size() * sizeof(HANDLE), nullptr, nullptr))
	{
		startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
		startup.StartupInfo.hStdInput = standard[0];
		startup.StartupInfo.hStdOutput = standard[1];
		startup.StartupInfo.hStdError = standard[2];
		startup.lpAttributeList = list;
	}
	std::cerr << "Relaunch: " << from_utf16(path.data()) << " --cmdline=" << line << std::endl;
	PROCESS_INFORMATION process = {};
	bool inherits = startup.lpAttributeList != nullptr;
	if (CreateProcessW(path.data(), commandLine.data(), nullptr, nullptr, inherits, inherits ? EXTENDED_STARTUPINFO_PRESENT : 0,
		nullptr, nullptr, &startup.StartupInfo, &process))
	{
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
	}
	else
	{
		std::cerr << "Relaunch: error " << GetLastError() << std::endl;
	}
	if (listed)
		DeleteProcThreadAttributeList(list);
#endif
}
