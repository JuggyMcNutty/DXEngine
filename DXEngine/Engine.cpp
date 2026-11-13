
#include "Precomp.h"
#include "Engine.h"
#include "Timeline.h"
#include "Utils/File.h"
#include "Utils/StrTools.h"
#include "Utils/SHA1Sum.h"
#include "Utils/UTF8Reader.h"
#include "Utils/PEIcon.h"
#include "Render/RenderSubsystem.h"
#include "Package/PackageManager.h"
#include "VM/NativeFunc.h"
#include "Packages/Core/UState.h"
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include "Package/IniProperty.h"
#include "Package/ObjectStream.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/USubsystem.h"
#include "Packages/Core/Properties/UObjectProperty.h"
#include "Packages/Core/Properties/UStrProperty.h"
#include "Packages/Core/Properties/UFloatProperty.h"
#include "Packages/Core/Properties/UIntProperty.h"
#include "Packages/Engine/UClient.h"
#include "Packages/Engine/UConsole.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/UCanvas.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/UHUD.h"
#include "Packages/Engine/Actors/Inventory/UInventory.h"
#include "Packages/Engine/Actors/Decoration/UDecoration.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UGameInfo.h"
#include "Packages/Engine/Actors/Info/UZoneInfo.h"
#include "Packages/Engine/Actors/Info/UPlayerReplicationInfo.h"
#include "Utils/Random.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/UFont.h"
#include "Packages/Engine/Resources/Mesh/ULodMesh.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Packages/Engine/Resources/UMusic.h"
#include "Packages/Engine/Resources/USound.h"
#include "Packages/Engine/UDXEngineClient.h"
#include "Packages/Engine/Subsystems/UGameEngine.h"
#include "Packages/Engine/Subsystems/UDXEngineRenderDevice.h"
#include "Packages/Engine/Subsystems/UDXEngineAudioDevice.h"
#include "LauncherLine.h"
#include "ServerConsole.h"
#include "OriginalCommandLine.h"
#include "Packages/Engine/Subsystems/UDXEngineNetworkDevice.h"
#include "Packages/Extension/UPlayerPawnExt.h"
#include "Packages/Extension/Flags/UFlag.h"
#include "Packages/Extension/Flags/UFlagBase.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"
#include "Packages/DeusEx/UDeusExLevelInfo.h"
#include "Packages/DeusEx/UDeusExPlayer.h"
#include "Packages/DeusEx/UDeusExSaveInfo.h"
#include "ObjectTravelInfo.h"
#include "Math/quaternion.h"
#include "Math/FrustumPlanes.h"
#include "GameWindow.h"
#include "RenderDevice/RenderDevice.h"
#include "VM/Frame.h"
#include "Utils/AlignedAlloc.h"
#include "VM/ScriptCall.h"
#include "Utils/Convert.h"
#include "Network/NetClient.h"
#include "Network/NetChannel.h"
#include "Network/NetServer.h"
#include "Network/NetDemo.h"
#include "Packages/Engine/Actors/NavigationPoint/UPlayerStart.h"
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <functional>
#include <set>
#include <thread>
#ifndef WIN32
#include <unistd.h>
#include <csignal>
#endif

// The input bindings' writes into the pawn (below, with InputCommand)
static UProperty* InputProperty(UObject* pawn, const std::string& name, bool axis);
static void SetInputButton(UObject* pawn, const std::string& name, bool pressed);

// The machine's name, as the original's LevelInfo.ComputerName has it
// (Windows' computer name: the host name in capitals).
static std::string ComputerName()
{
	std::string result;
#ifdef WIN32
	if (const char* name = std::getenv("COMPUTERNAME"))
		result = name;
#else
	char name[256] = {};
	if (gethostname(name, sizeof(name) - 1) == 0)
		result = name;
#endif
	for (char& c : result)
		c = (char)std::toupper((unsigned char)c);
	return result.empty() ? "MyComputer" : result;
}

#ifndef WIN32
// SIGTERM, SIGINT or SIGHUP (a closed terminal, a logout): quit as a closed
// window does, the inis written back. The handler is installed after SDL's,
// in its place (SDL's posts a quit event, which closes the window too) and
// is reset by its first signal, so a second one kills.
static volatile std::sig_atomic_t StopSignalled = 0;

static void OnStopSignal(int)
{
	StopSignalled = 1;
}
#endif

bool Engine::IsStopSignalled()
{
#ifndef WIN32
	return StopSignalled != 0;
#else
	return false;
#endif
}

#ifndef WIN32
static void InstallStopSignal()
{
	struct sigaction action = {};
	action.sa_handler = OnStopSignal;
	action.sa_flags = SA_RESETHAND;
	sigemptyset(&action.sa_mask);
	sigaction(SIGTERM, &action, nullptr);
	sigaction(SIGINT, &action, nullptr);
	sigaction(SIGHUP, &action, nullptr);
}
#endif

// An option with a value, name=..., whatever the value: the original's
// GetOption("name="), which an option of that name alone does not match.
static bool HasValueOption(const UnrealURL& url, const std::string& name)
{
	for (const std::string& option : url.Options)
	{
		if (option.size() > name.size() && option[name.size()] == '=' && StrTools::equals_ignore_case(option.substr(0, name.size()), name))
			return true;
	}
	return false;
}

// An option that is the name alone, in any case: the original's
// FURL::HasOption (Engine.dll 0x103f9df0), which ?loadonly=1 does not match.
static bool HasWholeOption(const UnrealURL& url, const std::string& name)
{
	for (const std::string& option : url.Options)
	{
		if (StrTools::equals_ignore_case(option, name))
			return true;
	}
	return false;
}

// The map the original's Browse of a map tests for its mission and looks for
// in Current: a ?load= option's value, whatever it is, in place of the URL's
// map, the URL's map with none (DeusEx.dll 0x100115ce, 0x10011736-0x10011825).
// The map loaded from Maps is the URL's all the same. No stock path sends one:
// PlayerPawn's QuickLoad sends ?load=9, but DeusExPlayer's replaces it.
static std::string MissionMapName(const UnrealURL& url)
{
	return HasValueOption(url, "load") ? url.GetOption("load") : url.Map;
}

Engine* engine = nullptr;

Engine::Engine(GameLaunchInfo launchinfo) : LaunchInfo(launchinfo)
{
	engine = this;

	packages = std::make_unique<PackageManager>(LaunchInfo);

	// The script's random numbers differ from run to run, as the original's
	std::srand((unsigned int)std::time(nullptr));
	SeedRandom((unsigned int)std::time(nullptr));

	auto transientpkg = packages->GetTransientPackage();
	auto enginepkg = packages->GetPackage("Engine");
	gameengine = UObject::Cast<UGameEngine>(transientpkg->NewObject("gameengine", enginepkg->GetClass("GameEngine"), ObjectFlags::Transient));
	audiodev = UObject::Cast<UDXEngineAudioDevice>(transientpkg->NewObject("audiodev", enginepkg->GetClass("DXEngineAudioDevice"), ObjectFlags::Transient));
	renderdev = UObject::Cast<UDXEngineRenderDevice>(transientpkg->NewObject("renderdev", enginepkg->GetClass("DXEngineRenderDevice"), ObjectFlags::Transient));
	netdev = UObject::Cast<UDXEngineNetworkDevice>(transientpkg->NewObject("netdev", enginepkg->GetClass("DXEngineNetworkDevice"), ObjectFlags::Transient));
	client = UObject::Cast<UDXEngineClient>(transientpkg->NewObject("client", enginepkg->GetClass("DXEngineClient"), ObjectFlags::Transient));
	viewport = UObject::Cast<UViewport>(transientpkg->NewObject("viewport", enginepkg->GetClass("Viewport"), ObjectFlags::Transient));
	canvas = UObject::Cast<UCanvas>(transientpkg->NewObject("canvas", enginepkg->GetClass("Canvas"), ObjectFlags::Transient));
	DefaultTexture = UObject::Cast<UTexture>(packages->GetPackage("Engine")->GetUObject("Texture", "DefaultTexture"));

	floatprop = GC::Alloc<UFloatProperty>(NameString(), nullptr, ObjectFlags::NoFlags);

	Frame::Guards = true;
	Frame::NoRunaway = OriginalCommandLine::Get().Param("norunaway");

	auto extpkg = packages->GetPackage("Extension");
	deusExPackage = packages->GetPackage("DeusEx");
	dxgc = UObject::Cast<UGC>(transientpkg->NewObject("gc", extpkg->GetClass("GC"), ObjectFlags::Transient));
	// The GC class's underline texture (Solid), which every window's GC
	// starts with in the original.
	dxgc->DefaultUnderlineTexture = dxgc->underlineTexture();
	dxgc->Canvas() = canvas;
	// In a little package of its own, as the original's GetSaveInfo(pkg)
	// makes it: Package::Save refuses an object from another package
	// (transient stopped every save), and saving it from package DeusEx
	// exported the DeusExSaveInfo class into SaveInfo.dxs, whose load
	// then re-registered the class's natives, a fatal error.
	// Public, as the original's: its load of a save finds the object by
	// its name in the save's SaveInfo.
	dxSaveInfo = UObject::Cast<UDXSaveInfo>(packages->CreateEmptyPackage("SaveInfo")->NewObject("MyDeusExSaveInfo", deusExPackage->GetClass("DeusExSaveInfo"), ObjectFlags::Public));

	// The AI event manager's class is the original Engine.dll's own,
	// with no script -- Engine.u only declares LevelInfo's property of
	// it -- so the class is made here and added to the Engine package,
	// where a level's manager and a save's import of it resolve.
	{
		UClass* objectClass = UObject::Cast<UClass>(packages->GetPackage("Core")->GetUObject("Class", "Object"));
		UClass* emClass = GC::Alloc<UClass>(NameString("EventManager"), objectClass, ObjectFlags::Native | ObjectFlags::Public | ObjectFlags::Standalone);
		emClass->Class = UObject::Cast<UClass>(packages->GetPackage("Core")->GetUObject("Class", "Class"));
		emClass->Properties = objectClass->Properties;
		emClass->StructSize = objectClass->StructSize;
		emClass->StructAlignment = objectClass->StructAlignment;
		emClass->PropertyData.Init(emClass);
		packages->GetPackage("Engine")->AddRuntimeExport(emClass);
	}

	std::string consolestr = packages->GetIniValue("system", "Engine.Engine", "Console");
	std::string consolepkg = consolestr.substr(0, consolestr.find('.'));
	std::string consolecls = consolestr.substr(consolestr.find('.') + 1);
	// Named after its class and a number, as the original's console object is
	// made with no name (StaticConstructObject: DeusExConsole0)
	UClass* consoleClass = packages->GetPackage(consolepkg)->GetClass(consolecls);
	console = UObject::Cast<UConsole>(transientpkg->NewObject(transientpkg->MakeUniqueObjectName(consoleClass), consoleClass, ObjectFlags::Transient));

	console->Viewport() = viewport;
	canvas->Viewport() = viewport;
	viewport->Console() = console;
}

Engine::~Engine()
{
	if (audiodev)
		audiodev->ShutdownDevice();

	// A log the folder will not take (a full disk, an I/O error) is lost, not
	// the exit: a throw out of a destructor ends the process, before the error
	// window when one from Run is on its way
	try
	{
		Logger::Get()->SaveLogAsPlaintext((Directory::engineUserDir() / "DXE-Log-LastRun.txt").string());
	}
	catch (const std::exception&)
	{
	}

	engine = nullptr;
}

void Engine::Run()
{
	LogMessage("Game: " + LaunchInfo.gameName + " (Version: " + LaunchInfo.gameVersionString + ")");
	LoadEngineSettings();
	LogMessage("Loaded Engine settings");
	LoadKeybindings();
	LogMessage("Loaded key bindings");
	LogGamePackageSHA1Sums();

	// Downloads left unfinished, and those unused too long (the original
	// cleans its cache as the engine starts).
	packages->CleanFileCache();

#ifndef WIN32
	InstallStopSignal();
#endif

	// A dedicated server has no client: no window, sound or picture, no
	// player of its own, no Entry level (the original's -SERVER).
	const bool dedicated = LaunchInfo.dedicatedServer;
	if (!dedicated)
	{
		chosenRenderDevice = ChooseRenderDevice();
		ApplyRunOnlySettings();
		OpenWindow();

		// -nosound, or the game engine's UseSound off: no sound and no music,
		// as the original's engine makes no audio subsystem then
		// (UEngine::InitAudio, Engine.dll 0x1037fe50). DXEngine keeps its
		// device, on OpenAL Soft's null driver, which plays nothing.
		std::string gameEngineClass = packages->GetIniValue("system", "Engine.Engine", "GameEngine", "Engine.GameEngine");
		// UseSound by its first token, as the original's import reads a bool
		// (UBoolProperty::ImportText, Core.dll 0x10166380): 0.0 is false; a
		// text it refuses keeps the class's True (bugs.md ENG.32)
		std::string useSound = packages->GetIniValue("system", gameEngineClass, "UseSound", "True");
		bool soundOff = false;
		try
		{
			soundOff = !IniPropertyConverter<bool>::FromString(useSound);
		}
		catch (const std::exception&)
		{
		}
		if (OriginalCommandLine::Get().Param("nosound") || soundOff)
		{
			LogMessage(soundOff ? "UseSound off: no sound" : "-nosound: no sound");
			NoAudioSubsystem = true;
#ifdef _WIN32
			_putenv_s("ALSOFT_DRIVERS", "null");
#else
			setenv("ALSOFT_DRIVERS", "null", 1);
#endif
		}
		audiodev->InitDevice();
		render = std::make_unique<RenderSubsystem>(window->GetRenderDevice());

		if (!client->StartupFullscreen)
			viewport->bWindowsMouseAvailable() = true;

		window->LockCursor();
	}

	// The Entry level, as the original's Init loads it for a client: a
	// client that loses its server goes back there (ReturnToEntry).
	if (!dedicated)
		LoadEntryMap();

	// The start URL a partial travel's on the player's options, as the
	// original's game engine reads it (UGameEngine::Init, Engine.dll
	// 0x103891c0): one its URL refuses is fatal. Its default, with no URL,
	// is DX.dx whatever the ini's [URL] LocalMap names, as Deus Ex's Init
	// sets FURL::DefaultLocalMap to DX.dx before it reads the command line
	// (0x10389429-0x103894a4)
	UnrealURL defaultURL = GetDefaultURL("DX.dx");
	UnrealURL startURL(&defaultURL, LaunchInfo.url, ETravelType::TRAVEL_Partial);
	if (!LaunchInfo.url.empty() && !startURL.Valid)
		Exception::Throw(LocalizeMessage("Engine", "Errors", "InvalidUrl", "Invalid URL: %s", { LaunchInfo.url }));
	// A server's address, or a URL of another protocol, which the original's
	// first browse hands to the system before it loads the menu's map: both
	// taken as a travel from the menu's map.
	bool launchServer = !LaunchInfo.url.empty() && (!startURL.Host.empty() || !startURL.IsInternal()) && !dedicated;
	// A save to start in, ?loadgame=N, whatever map the URL names: the slot
	// loaded before any map, as the original's first browse loads it
	// (DeusExGameEngine's Browse, dx-reverse-info's deusex-dll.md).
	bool launchSave = !LaunchInfo.url.empty() && startURL.Host.empty() && HasValueOption(startURL, "loadgame") && !dedicated;
	// The start's browse, as the original's Init browses (0x103895db-
	// 0x10389697): one that fails with no error's text is browsed once more
	// at DX.dx on the player's options, unless the URL was DX.dx, so the
	// menu's map plays meanwhile; one that fails with a text, or fails
	// again, is fatal, Engine.int's FailedBrowse. The first browse fails so
	// for a server's address, whose join it begins, a URL of another
	// protocol, which it hands to the system, and a slot with no save or
	// whose level does not load; a dedicated server's of a server's address
	// with Engine.int's ServerOpen (engine-dll.md, starting the game engine;
	// bugs.md ENG.26).
	const std::string startText = LaunchInfo.url.empty() ? std::string("DX.dx") : LaunchInfo.url;
	std::string browseError;
	bool browsed = false;
	if (dedicated && !startURL.Host.empty())
		browseError = LocalizeMessage("Engine", "Errors", "ServerOpen", "Servers can't open network URLs");
	else if (launchSave)
		browsed = LoadSavedGame(startURL);
	else if (!launchServer)
		browsed = BrowseStartMap(LaunchInfo.url.empty() ? defaultURL : startURL, browseError);
	if (!browsed && (!browseError.empty() || StrTools::equals_ignore_case(startText, "DX.dx") || !BrowseStartMap(defaultURL, browseError)))
		Exception::Throw(LocalizeMessage("Engine", "Errors", "FailedBrowse", "Failed to enter %s: %s", { startText, browseError }));

	// A server's address to start with: joined from the menu's map, as the
	// original's first browse does.
	if (launchServer)
		ClientTravel(LaunchInfo.url, ETravelType::TRAVEL_Absolute, false);

	// The original's EXEC=, read into an FString's 4,096 characters
	// (DeusEx.exe WinMain 0x1090947a, Core.dll 0x10146e0e), run as the
	// command "exec <value>" on the client's viewport once the engine is up
	// (0x109097e1): the console's exec, its file the value's first token,
	// its output to the log; then the launcher's splash can go.
	std::string execFile;
	if (!dedicated && viewport->Actor() && OriginalCommandLine::Get().Value("EXEC", execFile, 4096))
	{
		ExecOutput out;
		RunCommand("exec " + execFile, &out);
		for (const std::string& text : out.Lines)
			LogMessage(text);
	}
	LauncherLine::Get().Ready();

	objprop = GC::Alloc<UObjectProperty>(NameString(), nullptr, ObjectFlags::NoFlags);
	vecprop = GC::Alloc<UStructProperty>(NameString(), nullptr, ObjectFlags::NoFlags);
	colorprop = GC::Alloc<UStructProperty>(NameString(), nullptr, ObjectFlags::NoFlags);
	// A Color, as a package's struct property of one is typed
	// (UStructProperty::Load): CursorRequested's out color is copied into the
	// event's own Color, which takes no plain struct (ToColor), where a
	// vector or a rotator takes one
	colorprop->ValueType = ExpressionValueType::ValueColor;
	rotprop = GC::Alloc<UStructProperty>(NameString(), nullptr, ObjectFlags::NoFlags);

	if (const char* stress = std::getenv("DXE_GC_STRESS"))
		GarbageStressFrames = std::max(std::atoi(stress), 0);

	std::unique_ptr<Timeline> timeline;
	if (!LaunchInfo.timelinePath.empty())
	{
		timeline = std::make_unique<Timeline>();
		if (!timeline->Load(LaunchInfo.timelinePath))
			timeline.reset();
	}

	// The frames a second, counted once a second past it, as the original's
	// main loop counts them into the engine (DeusEx.exe MainLoop
	// 0x10914630): GETCURRENTTICKRATE's
	auto secondStart = std::chrono::steady_clock::now();
	int tickCount = 0;

	while (!quit)
	{
#ifndef WIN32
		if (StopSignalled)
		{
			LogMessage("Stopped by a signal");
			quit = true;
			break;
		}
#endif
		LauncherLine::Get().Poll([this](const std::string& line) { OnLauncherLine(line); });
		// A dedicated server's typed lines, as the original's log window
		// takes them between frames (bugs.md D.65)
		if (dedicated)
			ServerConsole::Get().Poll([this](const std::string& line) { RunTypedLine(line); });
		if (quit)
			break;

		// A frame, as the original's game engine has it (dx-reverse-info
		// engine-dll.md, the game engine's frame): the windows' tick, the
		// levels' tick, a save asked for, a travel -- the frame ending
		// there --, a join under way, the draw.
		float realTimeElapsed = CalcTimeElapsed();
		tickCount++;
		auto tickTime = std::chrono::steady_clock::now();
		// The frame's tick timed for STAT GLOBAL, the time it waits before
		// it left out, as the original's TickCycles (Engine.dll 0x103911ea)
		struct TickStat
		{
			Engine* engine;
			std::chrono::steady_clock::time_point start;
			~TickStat() { engine->TickStatTime = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); }
		} tickStat{ this, tickTime };
		double sinceSecond = std::chrono::duration<double>(tickTime - secondStart).count();
		if (sinceSecond > 1.0)
		{
			CurrentTickRate = (float)(tickCount / sinceSecond);
			secondStart = tickTime;
			tickCount = 0;
		}

		float entryLevelElapsed = EntryLevel ? realTimeElapsed * clamp(EntryLevelInfo->TimeDilation(), 0.0025f, 25.0f) : 0.0f;
		float levelElapsed = realTimeElapsed * clamp(LevelInfo->TimeDilation(), 0.0025f, 25.0f);

		// The windows tick before the engine, as the original's
		// (Extension.dll XGameEngineExt::Tick 0x100266a0): what the level
		// makes this frame has been laid out by the time they next tick.
		// They tick by real seconds since their last tick, not the level's
		// time (XRootWindow::TickWindows 0x1003a4a0).
		if (dxRootWindow)
			dxRootWindow->TickTree(UWindow::GetWindowsTickOffset(true));

		TotalTime += realTimeElapsed;

		if (EntryLevel && EntryLevel != Level)
			EntryLevelInfo->TimeSeconds() += entryLevelElapsed;
		LevelInfo->TimeSeconds() += levelElapsed;
		Logger::Get()->SetTimeSeconds(LevelInfo->TimeSeconds());

		// Update the time fields: the local time to the millisecond, as the
		// original's ULevel::UpdateTime has it from GetLocalTime.
		auto clockNow = std::chrono::system_clock::now();
		std::time_t now = std::chrono::system_clock::to_time_t(clockNow);
		std::tm* timedesc = std::localtime(&now);
		int milliseconds = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(clockNow.time_since_epoch()).count() % 1000);

		// As the original has them: the full year and the month 1 to 12, which is
		// what the scripts read them as -- StatLog pads a month below 10 and
		// writes the year as it stands, and MenuScreenSaveGame builds its
		// timestamp from the save info's the same way. tm counts the year
		// from 1900 and the month from 0.
		LevelInfo->Year() = timedesc->tm_year + 1900;
		LevelInfo->Month() = timedesc->tm_mon + 1;
		LevelInfo->Day() = timedesc->tm_mday;
		LevelInfo->DayOfWeek() = timedesc->tm_wday;
		LevelInfo->Hour() = timedesc->tm_hour;
		LevelInfo->Minute() = timedesc->tm_min;
		LevelInfo->Second() = timedesc->tm_sec;
		LevelInfo->Millisecond() = milliseconds;

		// The player's input takes at most 0.4 s of a long frame, as the
		// original's level tick holds it (dx-reverse-info engine-dll.md, a
		// level's tick).
		if (!dedicated)
			UpdateInput(std::min(realTimeElapsed, 0.4f));

		SetPause(!LevelInfo->Pauser().empty());

		if (timeline)
			timeline->Tick(realTimeElapsed);

		// The original's game engine clears these before each level tick
		// when it has a client, and nothing sets them: no detail is dropped
		// for the frame rate (engine-dll.md, a level's tick).
		if (!dedicated)
		{
			LevelInfo->bDropDetail() = false;
			LevelInfo->bAggressiveLOD() = false;
		}

		// A client's packets come in before its level ticks and go out
		// after, as the original's level tick has them; all of it STAT
		// GLOBAL's Game, as the original's GameCycles (0x1039006a-0x10390204).
		auto gameStart = std::chrono::steady_clock::now();
		if (LevelNetDriver)
			LevelNetDriver->TickDispatch(levelElapsed);

		// So do the level's demo's, by the frame's own time: a playback's
		// packets up to this frame, its player's coming and its end, as the
		// original's level tick has them (ULevel::Tick, Engine.dll
		// 0x103a51a0), paused or not.
		if (LevelDemoDriver)
		{
			LevelDemoDriver->TickDispatch(realTimeElapsed);
			if (LevelDemoDriver->ServerConnection)
				TickDemoPlayback();
		}

		Level->Tick(levelElapsed, m_GamePaused);

		// A server replicates to its clients before the packets go; a client
		// its own player's pawn to the server: the level's Net, as the
		// original's TickNetServer and TickNetClient clock it.
		if (LevelNetDriver && !LevelNetDriver->ServerConnection && ServerLevel)
		{
			StatClock clock(Level->Stats.Net);
			ServerLevel->TickNetServer(levelElapsed);
		}
		else if (LevelNetDriver && LevelNetDriver->ServerConnection && ClientLevel)
		{
			StatClock clock(Level->Stats.Net);
			ClientLevel->TickNetClient(*LevelNetDriver);
		}
		if (LevelNetDriver)
			LevelNetDriver->TickFlush();

		// A recording takes the level after its tick, then its packets go
		// into the file.
		if (LevelDemoDriver)
		{
			if (!LevelDemoDriver->ServerConnection)
				TickDemoRecord();
			LevelDemoDriver->TickFlush();
		}
		GameStatTime = std::chrono::duration<double>(std::chrono::steady_clock::now() - gameStart).count();

		// Then the Entry level, and the viewport's player's level when it is
		// not the game's, as the original's game engine ticks them
		// (engine-dll.md, a level's tick): so the Entry level a second time,
		// its time stepped again, while a joining client's player waits
		// there for its pawn.
		if (EntryLevel && EntryLevel != Level)
		{
			EntryLevel->Tick(entryLevelElapsed, m_GamePaused);
			UPlayerPawn* viewer = viewport ? viewport->Actor() : nullptr;
			if (viewer && viewer->XLevel() == EntryLevel)
			{
				EntryLevelInfo->TimeSeconds() += entryLevelElapsed;
				EntryLevel->Tick(entryLevelElapsed, m_GamePaused);
			}
		}

		// A save asked for during the tick, written before a travel takes
		// its level.
		WritePendingSave();

		// A frame that travels ends with its load, drawing nothing, as the
		// original's game engine returns from its tick there: the next
		// frame's time takes in the load. A travel asked for during a draw
		// -- a console's tick -- waits for the next frame's tick, the old
		// level ticking once more.
		if (TakeTravel(realTimeElapsed))
		{
			// The load's collection is part of the frame, as the original's
			// is part of its load.
			if (!GarbageRequest.empty())
				CollectGarbage();
			continue;
		}

		// A join under way, ticked in a frame that takes no travel, as the
		// original's: the frame that loads its map draws it twice, first
		// before the JOIN goes (LoadClientMap).
		if (PendingLevel)
			TickPendingLevel(realTimeElapsed);

		// The draw, STAT GLOBAL's client, as the original's ClientCycles
		// (0x1039117e-0x10391198)
		auto clientStart = std::chrono::steady_clock::now();
		Draw(realTimeElapsed, levelElapsed, PendingLevel.get());
		ClientStatTime = std::chrono::duration<double>(std::chrono::steady_clock::now() - clientStart).count();

		// A save asked for during the draw: a console's tick
		WritePendingSave();

		if (GarbageStressFrames > 0 && ++GarbageStressCount >= GarbageStressFrames)
		{
			GarbageStressCount = 0;
			RequestGarbageCollection("stress");
		}
		if (!GarbageRequest.empty())
			CollectGarbage();
	}

	LogMessage("Shutting down...");
	CloseNetDriver();
	CloseDemoDriver();
	if (window)
		window->UnlockCursor();

	LogMessage("Saving configurations...");
	RestoreRunOnlySettings();
	if (packages->MissingDXESystemIni())
	{
		// Add the missing Subsystem entries; the software device's in the
		// original's WindowedRenderDevice's place are that class's, saved by
		// a SET alone (TakeWindowedRenderDevice)
		client->SaveConfig();
		audiodev->SaveConfig();
		if (!renderdev->IsWindowedRenderDevice())
			renderdev->SaveConfig();
	}
	packages->SaveAllIniFiles();

	LogMessage("Closing window...");
	CloseWindow();
}

void Engine::ApplyRunOnlySettings()
{
	const OriginalCommandLine& original = OriginalCommandLine::Get();
	auto set = [](RunOnlySetting& setting, int& value, int forTheRun)
	{
		setting.Active = true;
		setting.Configured = value;
		setting.ForTheRun = forTheRun;
		value = forTheRun;
	};

	// -defaultres: both sizes 640x480; the colour depths stay.
	if (original.Param("defaultres"))
	{
		LogMessage("-defaultres: 640x480");
		set(runWindowedX, client->WindowedViewportX, 640);
		set(runWindowedY, client->WindowedViewportY, 480);
		set(runFullscreenX, client->FullscreenViewportX, 640);
		set(runFullscreenY, client->FullscreenViewportY, 480);
	}

	ApplyNoDDraw();
}

// -noddraw with the software device (safe mode's "Run the game in a
// window", -nohard -noddraw): in the original, the software renderer with
// no DirectDraw for its fullscreen modes, so a window -- where it is chosen,
// and where it takes the place of a GPU device that did not start, as the
// original's WindowedRenderDevice takes GameRenderDevice's
void Engine::ApplyNoDDraw()
{
	if (OriginalCommandLine::Get().Param("noddraw") && chosenRenderDevice == RenderDeviceType::Software && client->StartupFullscreen)
	{
		LogMessage("-noddraw: the software device in a window");
		runStartupFullscreen.Active = true;
		runStartupFullscreen.Configured = 1;
		runStartupFullscreen.ForTheRun = 0;
		client->StartupFullscreen = false;
	}
}

void Engine::SaveClientConfig(bool asResized)
{
	// A resize's save writes the live client's values, which its class's
	// defaults then take (UClient's, as each resize of the original's
	// viewport saves it, WinDrv.dll 0x1110e354; bugs.md ENG.30); a SET's
	// writes the class's defaults
	if (asResized)
		client->TakeLiveValues();
	auto configured = [](const RunOnlySetting& setting, int value) { return setting.Active && value == setting.ForTheRun ? setting.Configured : value; };
	int windowedX = client->WindowedViewportX, windowedY = client->WindowedViewportY;
	int fullscreenX = client->FullscreenViewportX, fullscreenY = client->FullscreenViewportY;
	bool startupFullscreen = client->StartupFullscreen;
	client->WindowedViewportX = configured(runWindowedX, windowedX);
	client->WindowedViewportY = configured(runWindowedY, windowedY);
	client->FullscreenViewportX = configured(runFullscreenX, fullscreenX);
	client->FullscreenViewportY = configured(runFullscreenY, fullscreenY);
	if (runStartupFullscreen.Active && !client->StartupFullscreen)
		client->StartupFullscreen = runStartupFullscreen.Configured != 0;
	client->SaveConfig();
	client->WindowedViewportX = windowedX;
	client->WindowedViewportY = windowedY;
	client->FullscreenViewportX = fullscreenX;
	client->FullscreenViewportY = fullscreenY;
	client->StartupFullscreen = startupFullscreen;
}

void Engine::FlushConfig()
{
	// Not before the client's and the devices' settings are read: the
	// engine's own SET of the net speed comes first, and their defaults
	// would be written in their place (LoadEngineSettings)
	if (!engineSettingsLoaded)
		return;
	if (packages->MissingDXESystemIni())
	{
		SaveClientConfig(false);
		audiodev->SaveConfig();
		if (!renderdev->IsWindowedRenderDevice())
			renderdev->SaveConfig();
	}
	packages->FlushIniFiles();
}

void Engine::RestoreRunOnlySettings()
{
	auto restore = [](RunOnlySetting& setting, int& value)
	{
		if (setting.Active && value == setting.ForTheRun)
			value = setting.Configured;
		setting.Active = false;
	};
	restore(runWindowedX, client->WindowedViewportX);
	restore(runWindowedY, client->WindowedViewportY);
	restore(runFullscreenX, client->FullscreenViewportX);
	restore(runFullscreenY, client->FullscreenViewportY);
	if (runStartupFullscreen.Active && !client->StartupFullscreen)
		client->StartupFullscreen = runStartupFullscreen.Configured != 0;
	runStartupFullscreen.Active = false;
}

void Engine::ExecFile(const std::string& filename, ExecOutput& out)
{
	// As the original's EXEC (UViewport::ExecMacro, Engine.dll 0x10374700): a
	// relative file from the game's System folder (the original's working
	// directory, which nothing sets here), found in any case and by its
	// backslashes as Windows finds it, its text read as the original reads a
	// text file -- a byte a character, or UTF-16 after its byte-order mark,
	// to its first NUL (appLoadFileToString) --, and each line as ParseLine
	// takes it into 256 characters (Core.dll 0x10147e60) run on the
	// viewport, its output the command's: a // outside quotes drops the rest
	// of its line, a | outside quotes chains the next command, and a line
	// past 255 characters runs its rest as the next (bugs.md PKG.27), where
	// each line was run whole. A blank line or a comment alone runs nothing.
	// A file not there is Core's FileNotFound on the command's output, as the
	// original writes it (bugs.md WIN.23).
	std::string text;
	try
	{
		fs::path file = convert_path_separators(filename);
		if (file.is_relative())
			file = packages->GetSystemFolderPath() / file;
		text = IniFile::ReadGameText(Directory::resolveCase(file).string());
	}
	catch (...)
	{
		out.Log(LocalizeMessage("Core", "Errors", "FileNotFound", "Can't find file '%s'", { filename }));
		return;
	}
	LogMessage("Execing " + filename);
	size_t pos = 0;
	std::string line;
	while (StrTools::ue1_parse_line(text, pos, line, 256))
		RunCommand(line, &out);
}

// A failure (a SHOT whose file cannot be written) logged and the game going
// on, as a console line's is under its native (Frame::CallNative) and the
// original's SHOT carries on, writing nothing: it left Run through the
// window's events and ended the game. A runaway script still stops it.
void Engine::RunCommand(const std::string& command, ExecOutput* out, bool onEngine)
{
	ExecOutput discarded;
	try
	{
		if (!onEngine && viewport && !LaunchInfo.dedicatedServer)
			ViewportExec(command, out ? *out : discarded);
		else
			EngineExec(command, out ? *out : discarded);
	}
	catch (const ScriptCriticalError&)
	{
		throw;
	}
	catch (const std::exception& e)
	{
		if (Frame::Halted)
			throw;
		LogMessage(std::string("Script error: ") + e.what());
	}
}

// A dedicated server's typed line (bugs.md D.65), as the original's log
// window runs one (WTerminal::TypeChar, the SDK's Window/Inc/Window.h): the
// characters it takes, none below a blank, 254 at most, on the game engine,
// each line of its output logged, and Core's Exec, "Unrecognized command",
// logged where nothing takes it. A line of nothing runs nothing.
void Engine::RunTypedLine(const std::string& typed)
{
	std::string line;
	size_t characters = 0;
	for (char byte : typed)
	{
		unsigned char c = (unsigned char)byte;
		if (c < 32 || c == 127)
			continue;
		// A character is a UTF-8 sequence's first byte and those after it
		if ((c & 0xc0) != 0x80 && characters++ == 254)
			break;
		line += byte;
	}
	if (line.empty())
		return;
	ExecOutput out;
	bool taken = false;
	try
	{
		taken = EngineExec(line, out);
	}
	catch (const ScriptCriticalError&)
	{
		throw;
	}
	catch (const std::exception& e)
	{
		if (Frame::Halted)
			throw;
		LogMessage(std::string("Script error: ") + e.what());
		taken = true;
	}
	for (const std::string& text : out.Lines)
		LogMessage(text);
	if (!taken)
		LogMessage(LocalizeMessage("Core", "Errors", "Exec", "Unrecognized command"));
}

// The desktop's clipboard through the window, as UTF-8; a dedicated server,
// which has no window, holds the text itself, a copy to paste back, where the
// original's goes to Windows' clipboard as any game's (Core's
// appClipboardCopy and appClipboardPaste, from PlayerPawn's natives,
// Engine.dll 0x103df580, 0x103df700), by choice.
void Engine::SetClipboard(const std::string& text)
{
	if (window)
		window->SetClipboardText(text);
	else
		ServerClipboard = text;
}

std::string Engine::GetClipboard()
{
	return window ? window->GetClipboardText() : ServerClipboard;
}

void Engine::Relaunch(const std::string& line)
{
	// The original's RELAUNCH (Core.dll's USystem::Exec) logs the rest of the
	// line past the word, as ParseCommand leaves it, saves the config, starts
	// DeusEx.exe with it and exits. The Display screen's Rendering Device
	// sends -changevideo, the launcher's renderer page; the Host screen's
	// dedicated server, its map URL with -server in it. The inis are saved
	// as the engine shuts down.
	LogMessage("Relaunch: " + line);

	// The recreated launcher stands for DeusEx.exe: it starts again with the
	// line once the game has exited. Without it the engine starts itself
	// again (GameApp), which only a dedicated server needs: there is no
	// renderer page to show, and starting the same game again would only
	// lose the one being played.
	if (LauncherLine::Get().Present())
	{
		LauncherLine::Get().Relaunch(line);
	}
	else if (OriginalCommandLine(line).Param("server"))
	{
		RelaunchLine = line;
	}
	else
	{
		LogMessage("Relaunch: no launcher to start again");
		return;
	}
	quit = true;
}

void Engine::OnLauncherLine(const std::string& line)
{
	// A second launch's command line, forwarded as the original's log
	// window takes one: the game to the front, then its first word opened.
	if (line == "TakeFocus")
	{
		if (window)
			window->ActivateWindow();
	}
	else if (line.compare(0, 5, "Open ") == 0 && line.size() > 5)
	{
		LogMessage("Launcher: open " + line.substr(5));
		RunCommand("open " + line.substr(5), nullptr, true);
	}
	else if (line == "Quit")
	{
		// The launcher's stop, where there are no signals (on POSIX it
		// sends SIGTERM): quit as a closed window does, the inis written
		// back.
		LogMessage("Launcher: quit");
		quit = true;
	}
	else
	{
		LogMessage("Launcher: unknown line: " + line);
	}
}

void Engine::UpdateAudio()
{
	// Timed for STAT GLOBAL's Audio, as the original's draw clocks the
	// audio's update into its game level's (Engine.dll 0x1038f5f8-0x1038f660)
	StatClock clock(Level->Stats.Audio);
	audiodev->SetViewport(viewport);
	audiodev->Update();
}

// A class default as the original's GET gives it (Core.dll's
// UObject::StaticExec, 0x101531b0: the property the class's field walk
// finds, its own first, ExportText localized, not delimited): a string
// bare, where PrintValue quotes it, and an object as its class and path
// name. The game's menus compare what they get: the Host screen's victory
// condition is "Frags", not "\"Frags\"".
static std::string ExportForGet(UObject* defaults, const NameString& propertyName)
{
	UProperty* prop = defaults->PropertyData.Class->FindProperty(propertyName);
	if (!prop)
		Exception::Throw("Class Property '" + defaults->Name.ToString() + "." + propertyName.ToString() + "' not found");
	return defaults->ExportPropertyText(prop);
}

// The command line past its first `skip` words, less the spaces before it.
static std::string ArgsFrom(const std::string& commandline, size_t skip)
{
	size_t i = 0;
	for (size_t n = 0; n < skip; n++)
	{
		i = commandline.find_first_not_of(" \t", i);
		if (i == std::string::npos)
			return {};
		i = commandline.find_first_of(" \t", i);
		if (i == std::string::npos)
			return {};
	}
	i = commandline.find_first_not_of(" \t", i);
	return i == std::string::npos ? std::string() : commandline.substr(i);
}

// The command word as the original's ParseCommand matches each of its words
// (Core.dll 0x10147a20; StrTools::ue1_parse_command): past blanks and tabs,
// the run of ASCII letters and digits, in lower case -- every word being
// such a run, a word followed by any other character is that word: exec"x"
// is EXEC, shot. SHOT, where the line's first blank-separated word was
// taken whole --; rest the text past it and the blanks and tabs after it,
// where ParseCommand leaves the line for the word's own parse (bugs.md
// PKG.27). Empty when the line begins with no letter or digit.
static std::string CommandWord(const std::string& line, std::string& rest)
{
	auto alnum = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
	size_t start = line.find_first_not_of(" \t");
	if (start == std::string::npos)
	{
		rest.clear();
		return {};
	}
	size_t end = start;
	while (end < line.size() && alnum(line[end]))
		end++;
	std::string word = line.substr(start, end - start);
	for (char& c : word)
		c = (char)std::tolower((unsigned char)c);
	size_t after = end > start ? line.find_first_not_of(" \t", end) : start;
	rest = after == std::string::npos ? std::string() : line.substr(after);
	return word;
}

// PreTravel to every actor of the level that has the function, with no
// argument, as the original's UGameEngine::SetClientTravel (Engine.dll
// 0x1038fa10) sends it: the mission scripts' (the intro's sound volume put
// back, Mission02's flags), not the travelling player's alone.
static void SendPreTravel(ULevel* level)
{
	if (!level)
		return;
	Array<UActor*> actors = level->Actors;
	for (UActor* actor : actors)
	{
		if (actor && !actor->bDeleteMe())
			CallEvent(actor, "PreTravel", {});
	}
}

void Engine::ClientTravel(const std::string& newURL, ETravelType travelType, bool transferItems)
{
	// No URL is no travel, as the original's SetClientTravel sets it and its
	// game engine's tick takes it (Engine.dll 0x1038fa10, 0x1038fe90): the
	// viewport's travel set to nothing, which the tick takes none of, one
	// asked for before it gone, PreTravel sent all the same -- a console's
	// open with nothing after it, or a script's ClientTravel("") (a mission
	// script's PreTravel stops its timer for the level).
	if (newURL.empty())
	{
		ClientTravelInfo.URL.Clear();
		ClientTravelInfo.TransferItems = transferItems;
		SendPreTravel(Level);
		return;
	}

	// The URL's player keys are written once its map is in (LoadMap), not
	// here: a travel whose map does not load writes none.

	// The last URL loses its ?loadonly and its portal first, as the
	// original's game engine takes them off it before a travel
	// (UGameEngine::Tick, Engine.dll 0x1038fe90): a travel from a restarted
	// level, entered by MapName?loadonly, carries no ?loadonly on.
	for (size_t i = LastURL.Options.size(); i > 0; i--)
	{
		if (StrTools::equals_ignore_case(LastURL.Options[i - 1], "loadonly"))
			LastURL.Options.erase(LastURL.Options.begin() + (i - 1));
	}
	LastURL.Portal.clear();

	// The URL read on the last one, as the original's game engine reads it
	// at the travel (FURL(&LastURL, URL, type), Engine.dll 0x103f7b60): a
	// partial travel, the console's open, on its options alone, "carry name,
	// reset server" (Engine.u); a relative one, a map exit's, on its server,
	// port and map too, so a server travelling sends its clients its next
	// map; an absolute one, the console's start, with its player options. A
	// text it refuses is the default URL, Index.dx with no option, which the
	// browse goes to as to any map: the original's game engine refuses it
	// (0x1038ad30), and Deus Ex's browse then loads its map all the same
	// (DeusEx.dll 0x10010b90).
	ClientTravelInfo.URL = UnrealURL(&LastURL, newURL, travelType);
	ClientTravelInfo.TransferItems = transferItems;
	SendPreTravel(Level);
}

UnrealURL Engine::GetDefaultURL(const std::string& map)
{
	UnrealURL url;
	std::string teleporterTag = "";
	std::string finalMapName = map;

	size_t tagPos = map.find('#');

	if (tagPos != std::string::npos)
	{
		teleporterTag = map.substr(tagPos + 1);
		finalMapName = map.substr(0, tagPos);
	}
	if (map.find("." + packages->GetMapExtension()) == std::string::npos)
		url.Map = finalMapName + "." + packages->GetMapExtension();
	else
		url.Map = finalMapName;

	if (!teleporterTag.empty())
		url.Portal = teleporterTag;
	for (const std::string& option : DefaultPlayerOptions())
		url.Options.push_back(option);
	return url;
}

Array<std::string> Engine::DefaultPlayerOptions()
{
	// The key with no name too, its option =x, where it was left out
	// (bugs.md PKG.27)
	Array<std::string> options;
	for (const auto& [key, value] : packages->GetIniKeyValues("user", "DefaultPlayer"))
		options.push_back(key + "=" + value);
	return options;
}

void Engine::LoadEntryMap()
{
	// The entry map is the map you see in the game when no other map is playing. For example when disconnected from a server. It is always loaded and running.
	// Loaded as the original's Init loads it, entry.dx with no option: its
	// game begins with none, the player's options coming with a return to it.
	// One that does not load is fatal, as the original's (Engine.dll
	// 0x103891c0: Engine.int's FailedBrowse for Entry and the error).
	std::string error;
	if (!LoadMap(UnrealURL("entry." + packages->GetMapExtension()), {}, false, &error))
		Exception::Throw(LocalizeMessage("Engine", "Errors", "FailedBrowse", "Failed to enter %s: %s", { "Entry", error }));
	EntryLevelInfo = LevelInfo;
	EntryLevel = Level;
	EntryDeusExLevelInfo = DeusExLevelInfo;
	EntryLevelPackage = LevelPackage;
	EntryGameInfo = GameInfo;
	// As the original's: no collection at a load of Entry.
	GarbageRequest.clear();
	LevelInfo = nullptr;
	Level = nullptr;
	LevelPackage = nullptr;
	GameInfo = nullptr;
	viewport->Actor() = nullptr;
}

void Engine::ReturnToEntry(const UnrealURL& url)
{
	// UGameEngine::Browse's ?failed and ?entry (Engine.dll 0x1038ad30): the
	// level left, the Entry level played with no level action, a player
	// spawned there for the viewport, logged in with the browse's URL (a lost
	// server's ?failed with the player's options); a failure with no join
	// pending says so for 6 s.
	const bool failed = url.HasOption("failed");
	LogMessage(LocalizeMessage("Engine", "Errors", "AbortToEntry", "Failed; returning to Entry"));
	if (!EntryLevel)
		return;
	if (Level)
		CallEvent(console, EventName::NotifyLevelChange);
	if (!LaunchInfo.dedicatedServer)
		audiodev->StopSounds();
	UnloadMap();
	Level = EntryLevel;
	LevelInfo = EntryLevelInfo;
	LevelPackage = EntryLevelPackage;
	GameInfo = EntryGameInfo;
	DeusExLevelInfo = EntryDeusExLevelInfo;
	LevelInfo->LevelAction() = LEVACT_None;
	LoginPlayer(url);
	if (failed && !PendingLevel)
		SetProgress(LocalizeMessage("Engine", "Errors", "ConnectionFailed", "Connection failed"), "", 6.0f);
}

void Engine::UnloadMap()
{
	// The level's drivers go with it, a demo's file closed, as the
	// original's map load deletes them (UGameEngine::LoadMap, Engine.dll
	// 0x1038c1f0).
	CloseNetDriver();
	CloseDemoDriver();

	// A joining client's player waiting in the Entry level goes as any
	// other, the original's LoadMap destroying each viewport's actor where
	// it stands: a failed join, a lost server, a travel in the wait.
	if (UPlayerPawn* pawn = viewport->Actor(); pawn && Level && pawn->XLevel() != Level)
	{
		pawn->Player() = nullptr;
		viewport->Actor() = nullptr;
		pawn->Destroy();
	}

	// The Entry level stays; the player it was given goes, as the original's
	// LoadMap destroys each viewport's actor where it stands.
	if (Level && Level == EntryLevel)
	{
		if (UPlayerPawn* pawn = viewport->Actor())
		{
			pawn->Player() = nullptr;
			viewport->Actor() = nullptr;
			pawn->Destroy();
		}
		LevelInfo = nullptr;
		DeusExLevelInfo = nullptr;
		Level = nullptr;
		LevelPackage = nullptr;
		GameInfo = nullptr;
		dxRootWindow = nullptr;
		return;
	}

	if (!LevelPackage)
		return;

	LevelInfo = nullptr;
	DeusExLevelInfo = nullptr;
	Level = nullptr;
	viewport->Actor() = nullptr;
	dxRootWindow = nullptr;
	GameInfo = nullptr;
	CameraActor = nullptr;
	packages->UnloadPackage(LevelPackage);
	LevelPackage = nullptr;
}

// The viewport drawn, as the original's game engine draws it
// (UGameEngine::Draw, Engine.dll 0x1038f110): with a join pending, its
// message first (UpdateConnectingMessage); the player's PlayerCalcView; the
// audio from the view; then the console's tick and the frame.
void Engine::Draw(float realTimeElapsed, float levelElapsed, const NetPendingLevel* pending)
{
	if (pending)
		UpdateConnectingMessage(*pending);

	// To do: improve CallEvent so parameter passing isn't this painful
	UFunction* funcPlayerCalcView = viewport->Actor() ? FindEventFunction(viewport->Actor(), "PlayerCalcView") : nullptr;
	if (funcPlayerCalcView)
	{
		vecprop->Struct = UObject::Cast<UStructProperty>(funcPlayerCalcView->Properties[1])->Struct;
		rotprop->Struct = UObject::Cast<UStructProperty>(funcPlayerCalcView->Properties[2])->Struct;
		CameraActor = viewport->Actor();
		CameraLocation = viewport->Actor()->Location();
		CameraRotation = viewport->Actor()->Rotation();
		CameraFovAngle = viewport->Actor()->FovAngle();
		CallEvent(viewport->Actor(), EventName::PlayerCalcView, {
			ExpressionValue::Variable(&CameraActor, objprop),
			ExpressionValue::Variable(&CameraLocation, vecprop),
			ExpressionValue::Variable(&CameraRotation, rotprop)
			});
	}

	if (LaunchInfo.dedicatedServer)
		return;

	UpdateAudio();

	// After a map's load with the render device's UsePrecache on, a frame
	// under the level action Precaching, its console's tick too -- the
	// console's PRECACHING --, the old action back after it, as the
	// original's Draw gives it while the device's PrecacheOnFlip is set and
	// the viewport's bSuspendPrecaching is not (UGameEngine::Draw, Engine.dll
	// 0x1038f110); nothing to precache, DXEngine's textures uploading as they
	// are drawn.
	const bool precaching = PrecacheOnFlip && LevelInfo && !viewport->bSuspendPrecaching();
	uint8_t action = 0;
	if (precaching)
	{
		action = LevelInfo->LevelAction();
		LevelInfo->LevelAction() = LEVACT_Precaching;
	}

	// The console ticks as the draw begins, after the level's tick and by
	// the real time since the last draw, as the original's
	// UConsole::PreRender ticks it (dx-reverse-info engine-dll.md, the
	// loading screen); paused or not, so its messages time out. The tick and
	// the frame's script events work their sums out at 24 bits, as the
	// original's render holds its x87 to 24 bits from before the console's
	// tick to after its PostRender (X87).
	X87::Single = true;
	render->BeginFrameStats();
	TickConsole(realTimeElapsed);

	viewport->SetViewportRect(0, 0, window->GetPixelWidth(), window->GetPixelHeight());
	render->DrawGame(levelElapsed);
	X87::Single = false;
	if (precaching)
	{
		LevelInfo->LevelAction() = action;
		PrecacheOnFlip = false;
	}
}

// The original's PaintProgress (Engine.dll 0x10388740): the viewport's
// player's flash at a tenth and its fog black for one frame, drawn and shown
// as any frame is, both put back. The flash ends before the player's and the
// console's PostRender, so the view and the windows come out at a tenth of
// their brightness and the console's word at full. The console ticks as the
// draw begins, as every frame's draw ticks it (UConsole::PreRender); no other
// clock moves, and no event is taken: input mid-load could start a travel or
// a save.
void Engine::PaintProgress()
{
	UPlayerPawn* player = viewport ? viewport->Actor() : nullptr;
	if (!render || !Level || !player)
		return;

	vec3 flashScale = player->FlashScale();
	vec3 flashFog = player->FlashFog();
	player->FlashScale() = vec3(0.1f);
	player->FlashFog() = vec3(0.0f);
	X87::Single = true;
	render->BeginFrameStats();
	TickConsole(0.0f);
	viewport->SetViewportRect(0, 0, window->GetPixelWidth(), window->GetPixelHeight());
	render->DrawGame(0.0f);
	X87::Single = false;
	player->FlashScale() = flashScale;
	player->FlashFog() = flashFog;
}

// The original's map load, before anything of the new map is read
// (UGameEngine::LoadMap 0x1038c1f0): the level's action Loading, or
// Connecting for a join's map; no pauser, and no menu to hide the word; the
// frame painted; the sounds stopped and the music left playing, as the
// original hands its audio the viewport it has; the action none again,
// whatever it was.
void Engine::PaintLoading(bool connecting)
{
	LevelInfo->LevelAction() = connecting ? LEVACT_Connecting : LEVACT_Loading;
	LevelInfo->Pauser() = u"";
	if (viewport && viewport->Actor())
		viewport->Actor()->bShowMenu() = false;
	PaintProgress();
	audiodev->StopEffects();
	LevelInfo->LevelAction() = LEVACT_None;
}

// An option starting "load", whatever follows -- ?load, ?load=9, ?loadonly,
// ?loadgame=3 --, as the original's LoadMap asks GetOption("load"), which
// matches the option's start.
static bool HasLoadOption(const UnrealURL& url)
{
	for (const std::string& option : url.Options)
	{
		if (option.size() >= 4 && StrTools::equals_ignore_case(option.substr(0, 4), "load"))
			return true;
	}
	return false;
}

// The player's options a loaded level's URL has, written to the user ini's
// [DefaultPlayer], as the original's LoadMap writes them once the level is
// in, given a client (FURL::SaveURLConfig, Engine.dll 0x1038e33d-
// 0x1038e3c1): a travel whose map does not load writes none.
static void SavePlayerKeys(PackageManager* packages, const UnrealURL& url)
{
	for (const char* key : { "Name", "Team", "Class", "Skin", "Face", "Voice", "OverrideClass" })
	{
		if (HasValueOption(url, key))
			packages->SetIniValue("User", "DefaultPlayer", key, url.GetOption(key));
	}
}

bool Engine::LoadMap(const UnrealURL& url, const std::map<std::string, std::string>& travelInfo, bool asClient, std::string* error)
{
	// A map's load counts its own jumps, as the original's (UGameEngine::LoadMap)
	Frame::InitRunaway();

	if (url.HasOption("entry") || url.HasOption("failed"))
	{
		ReturnToEntry(url);
		return true;
	}

	// The level left's place in the hub stack, which the original's LoadMap
	// reads before anything else (Engine.dll 0x1038c1f0): 0 with no level.
	const int32_t hubStackLeft = LevelInfo ? LevelInfo->HubStackLevel() : 0;

	// The loading screen. None at the start, with no level yet: the
	// original's Init loads its first map before it has a viewport.
	if (Level && !LaunchInfo.dedicatedServer)
		PaintLoading(asClient);

	// Within a mission, a map saved into Current is revisited as the
	// player left it: the file SaveCurrentLevel wrote, found whatever case
	// the URL spells the map in (a stock exit names 12_vandenberg_cmd), with
	// or without its .dx, as the original's Browse cuts and Windows' file
	// system finds it -- a ?load= value's map in place of the URL's
	// (MissionMapName), the URL's map loaded from Maps where Current has none.
	const std::string missionMap = MissionMapName(url);
	std::string currentMap;
	if (dxTravelUsesCurrent)
	{
		dxTravelUsesCurrent = false;
		currentMap = packages->FindCurrentSave(missionMap);
	}
	const bool loadedFromCurrent = !currentMap.empty();

	// A level from Current is entered by its file, as the original's Browse
	// loads it (DeusEx.dll 0x10010b90, FURL(URL, file, TRAVEL_Relative)):
	// the map the save path's Current\<map>.<save extension>, the map tested
	// (MissionMapName) cut at its first .dx --
	// ..\Save\Current\01_NYC_UNATCOIsland.dxs --, with the URL's options but
	// PUSH, POP, PEER and LOAD, and its portal.
	UnrealURL enteredURL = url;
	if (loadedFromCurrent)
	{
		std::string map = missionMap;
		size_t dx = map.find(".dx");
		if (dx != std::string::npos)
			map.resize(dx);
		enteredURL.Map = packages->GetIniValue("System", "Core.System", "SavePath") + "\\Current\\" + map + "." + packages->GetSaveExtension();
		enteredURL.Options.clear();
		for (const std::string& option : url.Options)
		{
			if (!StrTools::equals_ignore_case(option, "push") && !StrTools::equals_ignore_case(option, "pop") &&
				!StrTools::equals_ignore_case(option, "peer") && !StrTools::equals_ignore_case(option, "load"))
				enteredURL.Options.push_back(option);
		}
	}

	// The new level's package, checked before anything of the old level
	// goes, as the original's LoadMap checks it after its LOADING frame
	// (CheckLevelPackage): one that fails leaves the level playing, its
	// pauser emptied and its sounds stopped by the frame, the error given
	// back. A client's map may be a download, in the cache under its GUID.
	std::string levelName, levelFile;
	if (loadedFromCurrent)
	{
		LogMessage("Loading " + missionMap + " from Current");
		levelFile = packages->SaveFilePath(currentMap);
	}
	else
	{
		levelFile = asClient ? packages->FindPackageFile(url.Map, {}) : std::string();
		if (!levelFile.empty())
			levelName = url.Map;
		else if (packages->MapExists(url.Map))
			levelFile = packages->MapFilePath(url.Map);
	}
	if (levelName.empty())
		levelName = fs::path(levelFile).stem().string();
	std::string loadError;
	Package* newPackage = CheckLevelPackage(enteredURL.Map, levelName, levelFile, loadError);
	if (!newPackage)
	{
		if (error)
			*error = loadError;
		return false;
	}

	if (Level)
		CallEvent(console, EventName::NotifyLevelChange);

	// A ?push saves the level left first, as the original's LoadMap does
	// (SaveHubLevel), the viewport's player destroyed before it, as the
	// original's LoadMap destroys each viewport's actor before the level
	// left is saved (Engine.dll 0x1038e5b0), so the hub save holds none
	// (bugs.md PKG.42).
	if (Level && enteredURL.HasOption("push"))
	{
		if (UPlayerPawn* pawn = viewport->Actor(); pawn && pawn->XLevel() == Level)
		{
			pawn->Player() = nullptr;
			viewport->Actor() = nullptr;
			pawn->Destroy();
		}
		SaveHubLevel(hubStackLeft);
	}

	UnloadMap();

	LevelPackage = newPackage;
	levelFromCurrent = loadedFromCurrent;
	LevelPackage->SetIsLevel();

	GetLevelInfoObject();

	LevelInfo->ComputerName() = UTF8ToScript(ComputerName());
	// The level's place in the hub stack, as the original's LoadMap gives it
	// (Engine.dll 0x1038e5b0 on): with a ?load option, whole, its file's
	// own; else the level left's plus one on ?push, one less on ?pop but not
	// under 0, the same on ?peer, and 0 for any other travel -- a level from
	// Current too, which is entered with none of them. No stock map, exit or
	// script travels so, nor reads it.
	if (!enteredURL.HasOption("load"))
		LevelInfo->HubStackLevel() = enteredURL.HasOption("push") ? hubStackLeft + 1 : enteredURL.HasOption("pop") ? std::max<int32_t>(hubStackLeft - 1, 0) : enteredURL.HasOption("peer") ? hubStackLeft : 0;
	SetEngineVersion();
	LevelInfo->bHighDetailMode() = true;
	LevelInfo->NetMode() = asClient ? NM_Client : LaunchInfo.dedicatedServer ? NM_DedicatedServer : url.HasOption("listen") ? NM_ListenServer : NM_Standalone;
	LevelInfo->DefaultTexture() = engine->DefaultTexture;

	GetLevelObject();

	// The level's URL is the one it was entered by, as the original's
	// LoadMap sets it (Engine.dll 0x1038c1f0), but for a load -- an option
	// starting "load", ?loadonly, ?loadgame or ?load=, as its GetOption
	// matches -- whose level keeps the URL it holds: a map file's Index.dx,
	// so a restarted level's reads Index.dx, or a save's. The last URL is
	// the one entered by, which the game and the player log in with; a
	// level from Current's own once the player is in (BrowseMap).
	LevelInfo->URL = HasLoadOption(enteredURL) ? LevelHeldURL() : enteredURL;
	LastURL = enteredURL;

	Level->TravelInfo = travelInfo; // Initially used travel info for level restart

	// The map's actors outside the load's context were never made, their
	// places in the list None (Package::LoadContext): the editor's, and on a
	// dedicated server those for the client alone. A game with a client
	// keeps its client-only actors, as the original's does in single player.

	// A level from Current still holds the pawn that left it, as the
	// original's does: GameInfo.Login finds and reuses it (its Player is
	// None), so travel does not spawn a second one, and the pruned
	// subsystems are made afresh by the game's own login.
	(void)loadedFromCurrent;

	LinkActorsToLevel();

	// Deus Ex's movers filed into the level's BSP, as the original's LoadMap
	// makes its brush tracker for every map but Entry -- with a client only:
	// a dedicated server files none, and its lines pass its movers
	if (!LaunchInfo.dedicatedServer && !StrTools::equals_ignore_case(LevelPackage->GetPackageName().ToString(), packages->GetIniValue("System", "URL", "EntryMap", "Entry")))
		Level->Collision.FileMovers();

	if (asClient)
	{
		// A client keeps the map's static and no-delete actors, their roles
		// turned to this side, and has no game: the server sends the rest.
		for (UActor* actor : Level->Actors)
		{
			if (!actor)
				continue;
			if (actor->bStatic() || actor->bNoDelete())
				std::swap(actor->Role(), actor->RemoteRole());
			else
				actor->Destroy();
		}
		GameInfo = nullptr;
		LevelInfo->Game() = nullptr;
		BeginPlay(url);
		RequestGarbageCollection("client map " + url.Map, true);
		PrecacheOnFlip = renderdev && renderdev->UsePrecache;
		SavePlayerKeys(packages.get(), url);
		return true;
	}

	// A server listens before its game begins, so the game's scripts see it;
	// a dedicated one always does.
	if (url.HasOption("listen") || LaunchInfo.dedicatedServer)
		Listen(url);

	// A level already begun -- one from Current -- keeps the game it was
	// saved with, as the original's LoadMap spawns a game only when the
	// level has none: a second would have no mutators, and its IsRelevant
	// then refuses every actor spawned (the player's augmentation and skill
	// managers, the key ring).
	GameInfo = UObject::TryCast<UGameInfo>(LevelInfo->Game());
	if (!GameInfo)
	{
		// Find the game info class
		UClass* gameInfoClass = packages->FindClass(LastURL.GetOption("game"));
		if (!gameInfoClass)
			gameInfoClass = LevelInfo->DefaultGameType();
		if (!gameInfoClass)
			gameInfoClass = packages->FindClass(packages->GetIniValue("system", "Engine.Engine", "DefaultGame"));
		if (!gameInfoClass)
			Exception::Throw("Could not find any gameinfo class!");

		// Spawn GameInfo actor
		// Named and flagged as the original's spawned game: by the class's
		// count, as any spawned actor (MakeUniqueObjectName: Entry's
		// DeusExGameInfo0, the menu map's 1, the next map's 2), and with the
		// flags any spawned actor has (UActor::Spawn).
		GameInfo = UObject::Cast<UGameInfo>(LevelPackage->NewObject(LevelPackage->MakeUniqueObjectName(gameInfoClass), gameInfoClass, ObjectFlags::Transactional | ObjectFlags::LoadContextFlags | ObjectFlags::HasStack));
		GameInfo->XLevel() = Level;
		GameInfo->Level() = LevelInfo;
		Level->Collision.AddToCollision(GameInfo);
		GameInfo->Tag() = gameInfoClass->Name;
		GameInfo->bTicked() = false;
		GameInfo->InitActorZone();
		GameInfo->Index = (int)Level->Actors.size();
		Level->Actors.push_back(GameInfo);
		Level->ActorsVersion++;

		LevelInfo->Game() = GameInfo;
	}

	if (ServerLevel)
		ServerLevel->BuildMasterMap(GameInfo);

	BeginPlay(url);

	RequestGarbageCollection("map " + url.Map, true);

	// The render device flushed, a precache allowed (UGameEngine::LoadMap's
	// Flush): its UsePrecache sets PrecacheOnFlip, as D3DDrv's does
	PrecacheOnFlip = renderdev && renderdev->UsePrecache;

	// As the original's map load collects what the new level does not use:
	// a server's downloads go, but a pending join's.
	Array<NameString> keep;
	if (PendingLevel)
	{
		for (const NetPendingLevel::UsedPackage& package : PendingLevel->Uses)
			keep.push_back(package.Name);
	}
	packages->ReleaseNetPackages(keep);

	if (!LaunchInfo.dedicatedServer)
		SavePlayerKeys(packages.get(), url);
	return true;
}

// A ?push's save of the level left, as the original's LoadMap writes it
// once the new map has passed its check (Engine.dll 0x1038c825-0x1038c88c):
// its destroyed actors let go, then the level into <SavePath>\Game%04i.dxs,
// numbered by the level's HubStackLevel. Nothing loads it: the original's
// ?pop looks for <SavePath>\Game%i.usa (UGameEngine::Browse 0x1038b303) and
// then goes to the map as any travel does. A file that cannot be written is
// logged, as the original's SavePackage logs it. The original's second
// ?push in a run asserts as it lets the destroyed actors go
// (UStruct::CleanupDestroyed), which DXEngine does not copy; and its player,
// destroyed at every map's load, is not in its file, where DXEngine, which
// destroys none, keeps it in its own.
void Engine::SaveHubLevel(int32_t hubStackLevel)
{
	char name[32];
	std::snprintf(name, sizeof(name), "Game%04d.dxs", hubStackLevel);
	const fs::path file = Directory::resolveCase(packages->GetSaveFolderPath() / name);
	packages->CloseStreams();
	std::error_code ec;
	fs::remove(file, ec);
	Level->CleanupDestroyed(true);
	SetLevelURLForSave();
	try
	{
		LevelPackage->Save(Level, file.string());
	}
	catch (const std::exception& e)
	{
		const std::string what = e.what();
		LogMessage("Hub save not written: " + file.string() + ": " + what.substr(0, what.find('\n')));
	}
}

void Engine::BeginPlay(const UnrealURL& url)
{
	if (!LevelInfo->bBegunPlay())
	{
		LevelInfo->TimeSeconds() = 0.0f;
		LevelInfo->bBegunPlay() = true;

		std::string options = url.GetOptions();

		auto stringProp = GC::Alloc<UStrProperty>("", nullptr, ObjectFlags::NoFlags);
		ScriptString error;

		LevelInfo->bStartup() = true;
		if (GameInfo)
		{
			CallEvent(GameInfo, EventName::InitGame, { ExpressionValue::StringValue(options), ExpressionValue::Variable(&error, stringProp) });
			if (!error.empty())
				Exception::Throw("InitGame failed: " + ScriptToUTF8(error));
		}

		// Each pass runs over the list as it grows, as the original's level
		// start does: an actor spawned during the start takes the later
		// passes too, after the same events from its spawn. A map's carcass
		// spawns its items in PostBeginPlay, and only the start's
		// SetInitialState sends them to the Idle2 it names: left in Pickup,
		// they lie shown at the carcass. Indexes, not iterators: the events
		// spawn.
		for (size_t i = 0; i < Level->Actors.size(); i++) { if (Level->Actors[i]) CallEvent(Level->Actors[i], EventName::PreBeginPlay); }
		for (size_t i = 0; i < Level->Actors.size(); i++) { if (Level->Actors[i]) CallEvent(Level->Actors[i], EventName::BeginPlay); }

		// Then each actor's zone where it is, unannounced, as the original's
		// level start finds it, whatever the map saved: DXMP_Silo's saved
		// regions put its items, crates and robots in none, which their first
		// fall took for leaving the world
		for (size_t i = 0; i < Level->Actors.size(); i++)
		{
			UActor* actor = Level->Actors[i];
			ULevelInfo* info = UObject::TryCast<ULevelInfo>(actor);
			if (actor && !actor->bDeleteMe())
				actor->Region() = info && info == actor->Level() ? info->OwnRegion() : actor->FindRegion();
		}

		// The original's level start has every actor 10 s undrawn, whatever
		// the map file kept, before PostBeginPlay: a ScriptedPawn out of
		// sight starts from its StartUp state's code, not its Tick -- one
		// out of the world stays in Idle, where its Tick would go on to its
		// orders.
		for (UActor* actor : Level->Actors) { if (actor) actor->LastRenderTime() = -10.0f; }

		for (size_t i = 0; i < Level->Actors.size(); i++) { if (Level->Actors[i]) CallEvent(Level->Actors[i], EventName::PostBeginPlay); }
		for (size_t i = 0; i < Level->Actors.size(); i++) { if (Level->Actors[i]) CallEvent(Level->Actors[i], EventName::SetInitialState); }
		for (size_t i = 0; i < Level->Actors.size(); i++) { if (Level->Actors[i]) Level->Actors[i]->InitBase(); }
		LevelInfo->bStartup() = false;
	}

	CallPostPostBeginPlay();
	Level->SortActors();

	if (LevelInfo->Game())
		CallEvent(LevelInfo->Game(), "DetailChange", {});
}

// Deus Ex: PostPostBeginPlay for every actor of every level loaded -- begun
// here, from a save or returned to from Current -- as the original's map
// load ends, before the player logs in: the pawns and the player bind their
// conversation events again, and the player takes back the themes the user
// chose over the save's.
void Engine::CallPostPostBeginPlay()
{
	for (size_t i = 0; i < Level->Actors.size(); i++) { if (Level->Actors[i]) CallEvent(Level->Actors[i], "PostPostBeginPlay"); }
}

// A slot's number as the original's appAtoi reads one: its leading digits,
// 0 for none -- a word typed into the console's open is a slot with no
// save, not a number to throw on.
static int32_t SlotNumber(const std::string& text)
{
	long n = std::strtol(text.c_str(), nullptr, 10);
	return (int32_t)std::clamp(n, (long)INT32_MIN, (long)INT32_MAX);
}

bool Engine::LoadSavedGame(const UnrealURL& url)
{
	// A save's load counts its own jumps, a map's load as the original's is
	// (UGameEngine::LoadMap's GInitRunaway)
	Frame::InitRunaway();
	if (!LoadFromSaveFile(url))
		return false;
	// The saved player logs in, as the original's load logs it in with the
	// load's URL, the default player's options on it: the game's Login finds
	// it unoccupied and keeps it -- Deus Ex's as saved, for the ?loadgame its
	// Browse loads a slot with (?load?loadonly?loadgame) -- and its
	// TravelPostAccept spawns the level's mission script, which no save holds.
	UnrealURL loginURL = GetDefaultURL(LevelInfo->URL.Map);
	loginURL.AddOrReplaceOption("load");
	loginURL.AddOrReplaceOption("loadonly");
	loginURL.AddOrReplaceOption("loadgame");
	LoginPlayer(loginURL);
	return true;
}

bool Engine::LoadFromSaveFile(const UnrealURL& url)
{
	if (url.HasOption("entry")) // Not sure what the purpose of this kind of travel is - do nothing for now.
		return false;

	std::string deusExMapName;
	bool hasSaveInfo = false;

	if (HasValueOption(url, "loadgame"))
	{
		// The original's Browse ?loadgame=N (dx-reverse-info/deusex-dll.md, travel
		// and saving): the slot's SaveInfo read -- -1 the quick save's, -2
		// Current's, any other Save%04d's (LoadSaveInfo, DeusEx.dll
		// 0x100108a0) --, with none no load (0x10010f96); else Current
		// emptied, the quick save copied into it when that SaveInfo's
		// DirectoryIndex is -1, else Save%04d of the URL's number -- for -2
		// Save-002, which only a save to slot -2 makes, so with the SaveInfo
		// of a slot's load in Current it empties Current and fails --, and
		// the info's map loaded from Current. Only a load puts a SaveInfo in
		// Current.
		const int32_t slot = SlotNumber(url.GetOption("loadgame"));
		const std::string folder = SaveSlotFolderName(slot);
		packages->ScanSaveInfos();
		if (Package* infoPkg = packages->GetSaveInfoPackage(folder))
		{
			if (auto info = UObject::Cast<UDXSaveInfo>(infoPkg->GetUObject("DeusExSaveInfo", "MyDeusExSaveInfo")))
			{
				deusExMapName = ScriptToUTF8(info->MapName());
				const std::string from = info->DirectoryIndex() == -1 ? SaveSlotFolderName(-1) : SaveNumberedFolderName(slot);
				DeleteSaveGameFiles("Current");
				CopySaveGameFiles(from, "Current");
				hasSaveInfo = true;
			}
		}
		if (!hasSaveInfo)
			LogMessage("LoadGame: no save in slot " + std::to_string(slot));
	}

	if (!hasSaveInfo)
		return false;

	// The loading screen, as the original's load of a save is a map's
	// load; none for an empty slot.
	if (Level && !LaunchInfo.dedicatedServer)
		PaintLoading(false);

	// The level's package checked before anything of the old level goes, as
	// the original's map load checks the save's file (LOAD_Verify, Engine.dll
	// 0x1038c1f0; CheckLevelPackage), Current keeping the slot's copy: one
	// that fails fails the load, its error kept apart from the browse's
	// (DeusEx.dll 0x10011323), so a start browses DX.dx in its place and a
	// load in play leaves the level playing (bugs.md ENG.28).
	const std::string savePath = "Current/" + deusExMapName + "." + packages->GetSaveExtension();
	const std::string saveURLMap = packages->GetIniValue("System", "Core.System", "SavePath") + "\\Current\\" + deusExMapName + "." + packages->GetSaveExtension();
	const std::string saveFile = packages->SaveFilePath(savePath);
	std::string loadError;
	Package* savefilePackage = CheckLevelPackage(saveURLMap, fs::path(saveFile).stem().string(), saveFile, loadError);
	if (!savefilePackage)
		return false;

	if (Level)
		CallEvent(console, EventName::NotifyLevelChange);

	UnloadMap();

	LevelPackage = savefilePackage;
	LevelPackage->SetIsLevel();

	GetLevelInfoObject();

	// Same as LoadMap: these are session/engine identity, not save data, and must be
	// re-established on every load regardless of what the package/save file contains.
	// The hub stack's place is the file's own, as the original's load of a
	// save keeps it for the ?load its URL has (Engine.dll 0x1038e5b0).
	LevelInfo->ComputerName() = UTF8ToScript(ComputerName());
	SetEngineVersion();
	LevelInfo->bHighDetailMode() = true;
	LevelInfo->NetMode() = 0; // NM_StandAlone
	LevelInfo->DefaultTexture() = engine->DefaultTexture;

	GetLevelObject();

	// The level's URL the one the save holds, the URL its level was
	// entered by, as the original's load keeps it (its ?load?loadonly?loadgame
	// sets none) and its Browse makes it the last URL (DeusEx.dll
	// 0x10010b90): the next travel goes on from it.
	LevelInfo->URL = LevelHeldURL();
	LastURL = LevelInfo->URL;

	LinkActorsToLevel();
	// Filed as a map's load files them, with a client only
	if (!LaunchInfo.dedicatedServer)
		Level->Collision.FileMovers();

	// engine->GameInfo is otherwise only assigned in LoadMap, so without this it keeps
	// pointing at the previous, now-unloaded level's GameInfo. LevelInfo->Game() is a normal
	// script property, so it round-trips through the save correctly; just re-point at it.
	GameInfo = UObject::Cast<UGameInfo>(LevelInfo->Game());
	if (!GameInfo)
		Exception::Throw("Save file has no GameInfo actor for " + LevelPackage->GetPackageName().ToString() + "!");

	CallPostPostBeginPlay();
	Level->SortActors();

	RequestGarbageCollection("saved level " + LevelPackage->GetPackageName().ToString(), true);
	PrecacheOnFlip = renderdev && renderdev->UsePrecache;
	// The travel's player keys, as a map's load writes them (SavePlayerKeys)
	if (!LaunchInfo.dedicatedServer)
		SavePlayerKeys(packages.get(), url);
	return true;
}

// Deus Ex's ShowMainMenu sets the travel variable bIgnoreNextShowMenu when
// Escape (or a pad's START) skips the intro, to swallow a second menu event
// the original engine delivered for that same press. DXEngine delivers
// one, so the flag would carry into the first level and eat the player's
// first real attempt to open the menu. Nothing else sets it; clear it once a
// level is running.
// The level's engine and net versions: Deus Ex's 1100 for both, which a
// server's query answer reports and the Join screens compare with their own.
void Engine::SetEngineVersion()
{
	LevelInfo->EngineVersion() = u"1100";
	LevelInfo->MinNetVersion() = u"1100";
}

static void ClearIgnoreNextShowMenu(UObject* player)
{
	if (!player)
		return;
	for (UProperty* prop : player->PropertyData.Class->Properties)
	{
		if (prop->Name == "bIgnoreNextShowMenu")
		{
			player->SetBool(prop->Name, false);
			return;
		}
	}
}

void Engine::SaveGameToSlot(int32_t slotNum, const std::string& saveDescription)
{
	// The original's SaveGame (DeusEx.dll 0x1000ef10) saves nothing with no
	// viewport's player, and with no DeusExLevelInfo logs that it has none
	// and saves nothing, no slot made, emptied or copied and no frame
	// painted (0x1000f02d-0x1000f04d): 99_Endgame4, the credits' dance party,
	// has none, where a quick save would otherwise overwrite the player's.
	if (!viewport || !viewport->Actor())
		return;
	if (!DeusExLevelInfo)
	{
		LogMessage("WARNING!  DeusExLevelInfo MISSING!");
		return;
	}

	// The original's SaveGame (dx-reverse-info/deusex-dll.md, travel and saving):
	// slot 0 is a new slot, the highest SaveNNNN plus one; -1 the quick
	// save; any other number a Save%04d, a negative one too (the game's
	// SaveGame(-3) writes Save-003, and SaveGame(-2) writes its info to
	// Save-002 while SaveCurrentLevel saves the level into Current). The
	// slot's folder is made with the folders it is in, as the original's
	// MakeDirectory makes the whole tree (0x1000f644-0x1000f64b), and found
	// in any case, as Windows' file system finds it; then it is emptied,
	// Current copied in, the SaveInfo filled and written to the slot alone
	// (0x1000f8e6), and the level saved on top of the copy.
	if (slotNum == 0)
		slotNum = NextSaveSlot();
	const auto slotFolderName = slotNum == -1 ? SaveSlotFolderName(slotNum) : SaveNumberedFolderName(slotNum);
	const auto saveSlotFolder = Directory::resolveCase(packages->GetSaveFolderPath() / slotFolderName);
	std::error_code ec;
	fs::create_directories(saveSlotFolder, ec);
	DeleteSaveGameFiles(slotFolderName);
	CopySaveGameFiles("Current", slotFolderName);

	// Its map the DeusExLevelInfo's MapName, as the original's
	// (0x1000f283): 99_Endgame1's Endgame1, where its package's is the
	// map's name
	dxSaveInfo->DirectoryIndex() = slotNum;
	dxSaveInfo->Description() = !saveDescription.empty() ? UTF8ToScript(saveDescription) : LevelInfo->Title();
	dxSaveInfo->MissionLocation() = DeusExLevelInfo->MissionLocation();
	dxSaveInfo->MapName() = DeusExLevelInfo->MapName();
	if (auto player = UObject::TryCast<UDeusExPlayer>(viewport->Actor()))
	{
		dxSaveInfo->SaveCount() = ++player->saveCount();
		dxSaveInfo->SaveTime() = (int)player->saveTime();
		dxSaveInfo->bCheatsEnabled() = (bool)player->bCheatsEnabled();
	}
	dxSaveInfo->UpdateTimeStamp();

	// The copy from Current is overwritten in place: the file goes first,
	// or the writer keeps it as a .old beside it, which no original save
	// carries.
	const auto saveInfoName = "SaveInfo." + packages->GetSaveExtension();
	const fs::path saveInfoFile = Directory::resolveCase(saveSlotFolder / saveInfoName);
	fs::remove(saveInfoFile);
	dxSaveInfo->package->Save(dxSaveInfo, saveInfoFile.string());

	SaveCurrentLevel(slotNum);

	packages->RemoveSaveInfoPackage(slotFolderName);
}

// The original's SaveGame makes a texture beside the save info and has the
// root window put a 160x120 snapshot in it -- none under its OpenGL driver
// (dx-reverse-info/deusex-dll.md, travel and saving). The Save Game screen hides the
// UI two frames before it asks, so the frame last drawn is the world alone.
void Engine::TakeSaveSnapshot()
{
	if (!dxSaveInfo)
		return;

	std::string device = packages->GetIniValue("system", "Engine.Engine", "GameRenderDevice");
	if (!dxRootWindow || StrTools::equals_ignore_case(device, "OpenGLDrv.OpenGLRenderDevice"))
	{
		dxSaveInfo->Snapshot() = nullptr;
		return;
	}

	dxRootWindow->SetSnapshotSize(160.0f, 120.0f);
	dxSaveInfo->Snapshot() = dxRootWindow->MakeSnapshot(dxSaveInfo->Snapshot(), dxSaveInfo->package);
}

// The original's SHOT: the frame as the player saw it, into the next free
// ShotNNNN.bmp in the game's System folder, 24-bit and bottom-up; with
// --userdir, into its Screenshots folder.
void Engine::TakeScreenshot()
{
	Array<TextureColor> pixels;
	int width = 0, height = 0;
	if (!render || !render->ReadLastFrame(pixels, width, height))
		return;

	fs::path folder = packages->GetSystemFolderPath();
	if (Directory::hasEngineUserDir())
	{
		folder = Directory::engineUserDir() / "Screenshots";
		Directory::create(folder.string());
	}
	fs::path filename;
	for (int i = 0; i < 10000; i++)
	{
		char name[16];
		std::snprintf(name, sizeof(name), "Shot%04d.bmp", i);
		if (!fs::exists(folder / name))
		{
			filename = folder / name;
			break;
		}
	}
	if (filename.empty())
		return;

	// The device reads back blue, green, red, alpha, top row first.
	int rowSize = (width * 3 + 3) & ~3;
	uint32_t imageSize = (uint32_t)rowSize * height;
	std::vector<uint8_t> file(54 + imageSize, 0);
	auto put16 = [&](size_t at, uint16_t v) { file[at] = v & 0xff; file[at + 1] = v >> 8; };
	auto put32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) file[at + i] = (v >> (i * 8)) & 0xff; };
	file[0] = 'B';
	file[1] = 'M';
	put32(2, (uint32_t)file.size());
	put32(10, 54);
	put32(14, 40);
	put32(18, (uint32_t)width);
	put32(22, (uint32_t)height);
	put16(26, 1);
	put16(28, 24);
	put32(34, imageSize);
	for (int y = 0; y < height; y++)
	{
		const uint8_t* src = (const uint8_t*)(pixels.data() + (size_t)(height - 1 - y) * width);
		uint8_t* dest = file.data() + 54 + (size_t)y * rowSize;
		for (int x = 0; x < width; x++)
		{
			dest[x * 3] = src[x * 4];
			dest[x * 3 + 1] = src[x * 4 + 1];
			dest[x * 3 + 2] = src[x * 4 + 2];
		}
	}

	auto out = File::create_always(filename.string());
	out->write(file.data(), file.size());
	LogMessage("Screenshot: " + filename.filename().string());
}

std::string Engine::SaveSlotFolderName(int32_t slot) const
{
	if (slot == -1)
		return "QuickSave";
	if (slot == -2)
		return "Current";
	return SaveNumberedFolderName(slot);
}

// Save%04d, as the original formats it: a negative slot's sign takes one of
// the four places (Save-003)
std::string Engine::SaveNumberedFolderName(int32_t slot) const
{
	char buf[32];
	snprintf(buf, sizeof(buf), "Save%04d", slot);
	return buf;
}

// The next slot is the highest existing SaveNNNN plus one, never a gap.
int32_t Engine::NextSaveSlot() const
{
	int32_t highest = 0;
	const auto saveFolderPath = packages->GetSaveFolderPath();
	if (fs::exists(saveFolderPath))
	{
		for (const auto& entry : fs::directory_iterator(saveFolderPath))
		{
			if (!entry.is_directory())
				continue;
			const std::string name = entry.path().filename().string();
			if (name.size() < 5 || name.compare(0, 4, "Save") != 0 || name[4] < '0' || name[4] > '9')
				continue;
			try
			{
				highest = std::max(highest, Convert::to_int32(name.substr(4)));
			}
			catch (...)
			{
			}
		}
	}
	return highest + 1;
}

// The save files of one save folder into another, as the original's
// CopySaveGameFiles (DeusEx.dll 0x10010000) copies those its FindFiles
// gives for *.<save extension>, into the folder it makes: a .old or any
// other file stays. Each folder found in any case, as Windows' file system
// finds it.
void Engine::CopySaveGameFiles(const std::string& fromFolder, const std::string& toFolder) const
{
	const auto saveFolderPath = packages->GetSaveFolderPath();
	const auto from = Directory::resolveCase(saveFolderPath / fromFolder);
	const auto to = Directory::resolveCase(saveFolderPath / toFolder);
	if (!fs::exists(from) || !fs::is_directory(from))
		return;
	std::error_code ec;
	fs::create_directories(to, ec);
	const std::string saveExt = "." + packages->GetSaveExtension();
	for (const auto& entry : fs::directory_iterator(from))
	{
		if (entry.is_regular_file() && StrTools::equals_ignore_case(entry.path().extension().string(), saveExt))
			fs::copy_file(entry.path(), to / entry.path().filename(), fs::copy_options::overwrite_existing);
	}
}

// The folder's files, the folder itself kept; "" means Current, as the
// original's DeleteSaveGameFiles. The folder found in any case, as Windows'
// file system finds it.
void Engine::DeleteSaveGameFiles(const std::string& folder) const
{
	// The package streams closed first, as the original resets its loaders
	// before it deletes: Windows refuses to delete a file held open for
	// reading, and the save scan and a load keep up to ten open.
	packages->CloseStreams();
	const std::string name = folder.empty() ? "Current" : folder;
	const auto dir = Directory::resolveCase(packages->GetSaveFolderPath() / name);
	if (!fs::exists(dir) || !fs::is_directory(dir))
		return;
	for (const auto& entry : fs::directory_iterator(dir))
	{
		if (entry.is_regular_file())
			fs::remove(entry.path());
	}
	packages->RemoveSaveInfoPackage(name);
}

// Removes the slot with its contents, for the DELETEGAME console command: a
// numbered slot alone, Save%04d, as the original's DeleteGame (DeusEx.dll
// 0x10010710) removes it, found in any case; the command takes only a slot
// written with digits, so the Save Game screen's Delete on the quick save's
// row, DeleteGame -1, removes nothing.
void Engine::DeleteGame(int32_t slot) const
{
	packages->CloseStreams();
	const std::string name = SaveNumberedFolderName(slot);
	std::error_code ec;
	fs::remove_all(Directory::resolveCase(packages->GetSaveFolderPath() / name), ec);
	packages->RemoveSaveInfoPackage(name);
}

// A map's mission number is its name's two leading digits; anything else -1.
int32_t Engine::DeusExMissionNumber(const std::string& mapName) const
{
	if (mapName.size() < 2 ||
		mapName[0] < '0' || mapName[0] > '9' || mapName[1] < '0' || mapName[1] > '9')
		return -1;
	return (mapName[0] - '0') * 10 + (mapName[1] - '0');
}

// A map's load that fails at its check, as the original's LoadMap catches
// it (Engine.dll 0x1038e62c): Core's FailedLoad logged with the URL's map
// and the error, a warning (UObject::SafeLoadError, Core.dll 0x10154430),
// where the map's file failed, then with the check's load of MyLevel, the
// progress lines Core's UrlFailed over the error for 6 s, the error given
// back; nothing of the level goes. No map is given for a file whose MyLevel
// alone failed.
void Engine::FailLoad(const std::string& map, const std::string& reason, std::string& error)
{
	error = reason;
	if (!map.empty())
		LogMessage(LocalizeMessage("Core", "Errors", "FailedLoad", "Failed to load '%s': %s", { map, error }));
	LogMessage(LocalizeMessage("Core", "Errors", "FailedLoad", "Failed to load '%s': %s", { "Level None.MyLevel", error }));
	SetProgress(LocalizeMessage("Core", "Errors", "UrlFailed", "Failed launching URL"), error, 6.0f);
}

// The check of a package's file that is there (LOAD_Verify), as the
// original's linker opens one (Core.dll 0x10160000): a file that does not
// begin with the package tag logs Core's BinaryFormat with the file's name
// and fails the load with Core's Aborted (bugs.md ENG.28). A file cut short
// -- before the tag's end, or within its tables -- stays fatal, as the
// original's reader stops the game there (GError's "ReadFile beyond EOF").
// Then every import is checked, as the linker's Verify checks each
// (0x10160d80): one its package lacks, or a package it names that is not
// there, fails the load with the linker's error (bugs.md PKG.38), a package
// not there logged first, as its own load failed before the map's.
bool Engine::VerifyPackageFile(const std::string& map, const std::string& file, std::string& error)
{
	if (!PackageManager::LacksPackageTag(file))
	{
		std::string importError, failedPackage;
		if (packages->VerifyFileImports(file, importError, failedPackage))
			return true;
		if (!failedPackage.empty())
			LogMessage(LocalizeMessage("Core", "Errors", "FailedLoad", "Failed to load '%s': %s", { failedPackage, importError }));
		FailLoad(map, importError, error);
		return false;
	}
	LogMessage(LocalizeMessage("Core", "Errors", "BinaryFormat", "The file '%s' contains unrecognizable data", { file }));
	FailLoad(map, LocalizeMessage("Core", "Errors", "Aborted", "Aborted"), error);
	return false;
}

// The packages a package imports, and those they import in turn, each found
// as the original's linker finds an import's package as it opens a package
// (VerifyImport, Core.dll 0x10160fd0, its GetPackageLinker): false, with the
// error and the packages whose load it failed, the one not found first, for
// one not on the paths -- Core's PackageNotFound -- or not a package --
// Aborted, its BinaryFormat logged. Each new one's tables are read: one cut
// short stops the engine, as the original's reader stops the game. A
// package already loaded was checked as it opened.
bool Engine::ImportsFound(Package* package, std::set<std::string>& seen, Array<std::string>& failed, std::string& error)
{
	for (const NameString& name : package->GetImportedPackages())
	{
		if (packages->IsPackageLoaded(name) || !seen.insert(NameString::Folded(name.ToString())).second)
			continue;
		const std::string file = packages->HasPackage(name) ? packages->FindPackageFile(name, {}) : std::string();
		if (file.empty())
		{
			failed.push_back(name.ToString());
			error = LocalizeMessage("Core", "Errors", "PackageNotFound", "Can't find file for package '%s'", { name.ToString() });
			return false;
		}
		if (PackageManager::LacksPackageTag(file))
		{
			LogMessage(LocalizeMessage("Core", "Errors", "BinaryFormat", "The file '%s' contains unrecognizable data", { file }));
			failed.push_back(name.ToString());
			error = LocalizeMessage("Core", "Errors", "Aborted", "Aborted");
			return false;
		}
		if (!ImportsFound(packages->GetPackage(name), seen, failed, error))
		{
			failed.push_back(name.ToString());
			return false;
		}
	}
	return true;
}

// The original's check of a level's package before its load, a load of its
// MyLevel with LOAD_Verify (UGameEngine::LoadMap, Engine.dll 0x1038c6ab),
// whose failure it catches (0x1038e62c), the level left playing on: the
// package made of the file, named as given, or none, the load failed
// (FailLoad) and the error given back. A file that is not there fails with
// Core's FileNotFound, one that is not a package as VerifyPackageFile has
// it; then its tables are read -- a file cut short within them stops the
// engine, and so does one of a version under 60, as the original's reader
// stops the game --, any version above that read as the original reads it;
// then the packages it imports (ImportsFound), each whose load failed logged
// before the map; then its MyLevel, Core's ObjectNotFound with none.
Package* Engine::CheckLevelPackage(const std::string& map, const std::string& name, const std::string& file, std::string& error)
{
	std::error_code ec;
	if (file.empty() || !fs::is_regular_file(file, ec))
	{
		FailLoad(map, LocalizeMessage("Core", "Errors", "FileNotFound", "Can't find file '%s'", { map }), error);
		return nullptr;
	}
	if (!VerifyPackageFile(map, file, error))
		return nullptr;
	Package* package = packages->LoadMapFile(name, file);
	std::set<std::string> seen;
	Array<std::string> failed;
	std::string importError;
	if (!ImportsFound(package, seen, failed, importError))
	{
		for (const std::string& failedPackage : failed)
			LogMessage(LocalizeMessage("Core", "Errors", "FailedLoad", "Failed to load '%s': %s", { failedPackage, importError }));
		FailLoad(map, importError, error);
		return nullptr;
	}
	if (package->FindObjectReference("Level", "MyLevel", {}) <= 0)
	{
		FailLoad("", LocalizeMessage("Core", "Errors", "ObjectNotFound", "Failed to find object '%s %s.%s'", { "Level", "None", "MyLevel" }), error);
		return nullptr;
	}
	return package;
}

// The start's browse of a map, as Deus Ex's first browse takes one (DeusEx.dll
// 0x10010b90): Current emptied, as from the Entry level, which holds no
// DeusExLevelInfo, or from no level, a dedicated server's, the map is
// outside any mission or of a new one (dx-reverse-info deusex-dll.md, travel
// and saving), so no level an earlier run left there comes back; then the
// load, the player logged in. A map whose load fails its check -- not there,
// not a package (bugs.md ENG.28), without its MyLevel or a package it
// imports -- is followed by Dx.Dx on the same URL, as a travel's by the
// level's own map (BrowseMap), there being no level yet, its load setting
// the error none again; one that fails there too fails the browse with that
// load's error.
bool Engine::BrowseStartMap(const UnrealURL& url, std::string& error)
{
	DeleteSaveGameFiles("Current");
	if (!LoadMap(url, {}, false, &error))
	{
		UnrealURL loadURL = url;
		loadURL.Map = "Dx.Dx";
		error.clear();
		if (!LoadMap(loadURL, {}, false, &error))
			return false;
	}
	if (!LaunchInfo.dedicatedServer)
		LoginPlayer();
	return true;
}

// The original's Browse of a map (DeusEx.dll 0x10010b90): the pre-travel
// first, then the load, which paints its LOADING frame before its check, as
// the original's LoadMap does, so the pauser goes, and the sounds stop,
// whether the map loads or not (LoadMap). Within the mission a map that does
// not load leaves the level playing, pruned and saved into Current as the
// pre-travel left it, after SAVING and LOADING; otherwise, Current emptied,
// the level's own map is loaded again on the travel's URL, with its travel
// info, its LOADING painted again: the level begins again from Maps, the
// failure's message gone with the pawn that showed it, or plays on where its
// URL names no map there.
bool Engine::BrowseMap(const UnrealURL& url, const std::map<std::string, std::string>& travelInfo)
{
	// ?failed and ?entry go to the Entry level, whose return logs the player
	// in, as the original's game engine takes them before any map.
	if (url.HasOption("failed") || url.HasOption("entry"))
	{
		LoadMap(url, travelInfo);
		return true;
	}
	DeusExPreTravel(url);
	const bool withinMission = dxTravelUsesCurrent;
	UnrealURL loadURL = url;
	loadURL.Map = LevelInfo->URL.Map;
	if (!LoadMap(url, travelInfo) && (withinMission || !LoadMap(loadURL, travelInfo)))
		return false;
	if (!LaunchInfo.dedicatedServer)
		LoginPlayer();
	// A level from Current makes its own URL the last one once the player is
	// in, as the original's Browse does after its load (DeusEx.dll
	// 0x10011c7e-0x10011c88): the one it was entered by, or, with a ?load=
	// on the URL, the one its file holds, which the next travel builds on
	// and GetURLMap reads.
	if (levelFromCurrent)
		LastURL = LevelInfo->URL;
	return true;
}

// The original's (DeusEx.dll's Browse, 0x10010b90): the DeusExLevelInfo's
// MapName -- there the first among all objects (GetDeusExLevelInfo), the
// level's own once those of the levels left are collected -- as the map, as
// it is, no URL read from it (FURL(MapName)), with the last URL's portal,
// ?loadonly and no other option, so the player logs in with no name; with
// none, the last URL as it is.
UnrealURL Engine::RestartURL()
{
	if (!DeusExLevelInfo)
		return LastURL;
	UnrealURL url;
	url.Map = ScriptToUTF8(DeusExLevelInfo->MapName());
	url.Portal = LastURL.Portal;
	url.AddOrReplaceOption("loadonly");
	return url;
}

// The original's Browse, before an ordinary map change, in its order: within
// one mission the departing level is pruned and saved into Current; a new
// mission, a map outside any, or a player starting a new game empties Current.
void Engine::DeusExPreTravel(const UnrealURL& url)
{
	dxTravelUsesCurrent = false;
	if (!Level || url.Map.empty())
		return;
	if (url.HasOption("restart") || url.HasOption("entry") || url.HasOption("failed") ||
		HasValueOption(url, "loadgame"))
		return;

	// For a map numbered within a mission and no ?loadonly: within the same
	// mission, for a player not starting a new game, the level is pruned and
	// saved into Current; otherwise the player's bStartingNewGame is cleared.
	// Every other way empties Current, a map outside any mission or a
	// ?loadonly with no mission's test. The map tested is a ?load= value's,
	// where the URL has one (MissionMapName): ?load=9 is a map outside any
	// mission (DeusEx.dll 0x10011928-0x100119b6). The level's mission is its
	// own DeusExLevelInfo's: the original's is the first in its object list
	// (GetDeusExLevelInfo, 0x100120a0), which a level left behind and still
	// held can be, where DXEngine eliminates a level left behind.
	const int32_t next = DeusExMissionNumber(MissionMapName(url));
	if (next != -1 && !url.HasOption("loadonly"))
	{
		auto player = UObject::TryCast<UDeusExPlayer>(viewport->Actor());
		const int32_t current = DeusExLevelInfo ? DeusExLevelInfo->MissionNumber() : -1;
		if (next == current && !(player && player->bStartingNewGame()))
		{
			PruneTravelActors();
			SaveCurrentLevel(-2);
			dxTravelUsesCurrent = true;
			return;
		}
		if (player)
			player->bStartingNewGame() = false;
	}
	DeleteSaveGameFiles("Current");
}

// Destroys what travels with the player, so it is not saved twice: the
// augmentations and skills with their managers, the flag base and a carried
// decoration (below), as the original's PruneTravelActors (DeusEx.dll
// 0x1000f9b0).
void Engine::PruneTravelActors() const
{
	for (UActor* actor : Level->Actors)
	{
		if (actor && (actor->IsA("Augmentation") || actor->IsA("AugmentationManager") ||
			actor->IsA("Skill") || actor->IsA("SkillManager")))
			actor->Destroy();
	}
	// The flag base goes too, after DeleteAllFlags, as the original's: the
	// flags cross in the pawn's travel graph, captured before this prune.
	if (auto pawnExt = UObject::TryCast<UPlayerPawnExt>(viewport->Actor()))
	{
		if (UFlagBase* flagBase = pawnExt->FlagBase())
		{
			flagBase->DeleteAllFlags();
			pawnExt->FlagBase() = nullptr;
		}
	}
	// Then the decoration in the player's hands, and the player lets go of
	// it. Left there, it stays in the level, a translucent ghost saved into
	// Current, and the pawn found again there on a return holds it still:
	// travel never sets a reference to None.
	if (auto pawn = UObject::TryCast<UPawn>(viewport->Actor()))
	{
		if (UDecoration* carried = pawn->carriedDecoration())
		{
			carried->Destroy();
			pawn->carriedDecoration() = nullptr;
		}
	}
}

// The level package into a save directory: -2 Current, -1 the quick save,
// else the numbered slot. The file goes first, or the writer keeps a .old.
void Engine::SaveCurrentLevel(int32_t slot)
{
	// With no DeusExLevelInfo the original's saves nothing and logs that it
	// has none (DeusEx.dll 0x1000fac0); nothing asks for it then, SaveGame
	// and the pre-travel saving only a level that has one.
	if (!DeusExLevelInfo)
	{
		LogMessage("WARNING!  DeusExLevelInfo MISSING!");
		return;
	}

	// SAVING on the screen first, at every save, the quick save's too, as
	// the original's SaveCurrentLevel (DeusEx.dll 0x1000fac0) marks the
	// level saving and paints it; the action none again at the end.
	LevelInfo->LevelAction() = LEVACT_Saving;
	PaintProgress();

	// The folder made with the folders it is in, as the original's
	// MakeDirectory makes the whole tree, and found in any case
	const auto folder = Directory::resolveCase(packages->GetSaveFolderPath() / SaveSlotFolderName(slot));
	std::error_code ec;
	fs::create_directories(folder, ec);
	// Named by the DeusExLevelInfo's MapName, as the original's
	// (0x1000fe35): 99_Endgame1 is Endgame1.dxs, thirteen stock maps' in
	// another case than the map's; over its file in any case, as Windows'
	// file system writes it, a file there from another session or the
	// original spelt otherwise (bugs.md VM.25)
	const fs::path levelFile = Directory::resolveCase(folder / (ScriptToUTF8(DeusExLevelInfo->MapName()) + "." + packages->GetSaveExtension()));
	packages->CloseStreams();
	fs::remove(levelFile);
	// The destroyed actors let go first, as the original's save does: the
	// file holds none of them.
	Level->CleanupDestroyed(true);
	SetLevelURLForSave();
	LevelPackage->Save(Level, levelFile.string());
	// And makes its brush tracker again, as the original's SaveGame does:
	// the movers filed where they are, a destroyed one's polygons gone
	Level->Collision.FileMovers();
	LevelInfo->LevelAction() = LEVACT_None;
}

// The level's own URL as the original's is when saved (ULevel::Serialize):
// the one it was entered by, or for a level loaded the one it held.
void Engine::SetLevelURLForSave() const
{
	if (!Level || !LevelInfo)
		return;
	const UnrealURL& url = LevelInfo->URL;
	Level->Protocol = url.Protocol;
	Level->Host = url.Host;
	Level->Map = url.Map;
	Level->Portal = url.Portal;
	Level->Options = url.Options;
	Level->Port = url.Port;
	Level->Unknown = url.Valid ? 1 : 0;
}

// The URL the level holds, as its file has it: a map's the editor's,
// Index.dx, a save's the one its level was entered by.
UnrealURL Engine::LevelHeldURL() const
{
	UnrealURL url;
	url.Protocol = Level->Protocol;
	url.Host = Level->Host;
	url.Map = Level->Map;
	url.Portal = Level->Portal;
	url.Options = Level->Options;
	url.Port = Level->Port;
	url.Valid = Level->Unknown != 0;
	return url;
}

// A server's travel with its items, as the original's game engine's tick
// takes it (UGameEngine::Tick, Engine.dll 0x1038fe90): the level's own
// travel info, and over it each player's by its name, every pawn with a
// player, a remote one's too.
std::map<std::string, std::string> Engine::CreateServerTravelInfo()
{
	auto travelInfo = Level->TravelInfo;
	for (UActor* actor : Level->Actors)
	{
		UPlayerPawn* pawn = UObject::TryCast<UPlayerPawn>(actor);
		if (pawn && pawn->Player() && pawn->PlayerReplicationInfo())
			travelInfo[ScriptToUTF8(pawn->PlayerReplicationInfo()->PlayerName())] = ActorTravelInfo::Create(pawn, true);
	}
	return travelInfo;
}

// A client's travel, as the same tick takes it: with its items the
// viewport's player alone, by its name, a net client's too; none of the
// level's own travel info, nor another player's; without its items,
// nothing, the player's own travel properties neither. A player whose
// replication info has not come is left out, where the original's reads
// it.
std::map<std::string, std::string> Engine::CreateClientTravelInfo(bool transferItems)
{
	std::map<std::string, std::string> travelInfo;
	UPlayerPawn* pawn = viewport ? viewport->Actor() : nullptr;
	if (transferItems && pawn && pawn->PlayerReplicationInfo())
		travelInfo[ScriptToUTF8(pawn->PlayerReplicationInfo()->PlayerName())] = ActorTravelInfo::Create(pawn, true);
	return travelInfo;
}

// With the URL the level was entered by, as the original's LoadMap logs
// the player in with its own: a restarted level's ?loadonly, which its
// level's URL, the map file's, has not.
void Engine::LoginPlayer()
{
	LoginPlayer(LastURL);
}

// A net game's login: in Deus Ex a name the player never chose -- none, or
// the game's stock "Player" -- is the project's own, so DXEngine's players show
// as such on a server. Single player keeps the game's.
void Engine::SetMultiplayerName(UnrealURL& url)
{
	std::string name = url.GetOption("Name");
	if (name.empty() || name == "Player")
		url.AddOrReplaceOption("Name=DXEnginePlayer");
}

// A player logged in with no net driver -- single player, the Entry level
// -- is made an admin, and with -alladmin any player, a server's too, as the
// original's SpawnPlayActor makes one after its roles (Engine.dll
// 0x103971ad), the game's Login having set it by the admin password: never
// unmade here. So the player's admin command runs in single player.
static void MakeAdmin(UPlayerPawn* pawn, bool netDriver)
{
	if (!netDriver || OriginalCommandLine::Get().Param("alladmin"))
		pawn->bAdmin() = true;
}

void Engine::LoginPlayer(const UnrealURL& loginURL)
{
	// A listen server's host is a player of the net game too.
	UnrealURL url = loginURL;
	if (LevelInfo && LevelInfo->NetMode() == NM_ListenServer)
		SetMultiplayerName(url);

	auto stringProp = GC::Alloc<UStrProperty>("", nullptr, ObjectFlags::NoFlags);
	ScriptString error;

	std::string portal = url.GetPortal();
	std::string options = url.GetOptions();

	std::string playerPawnClass = url.GetOption("Class");
	if (playerPawnClass.empty())
		playerPawnClass = packages->GetIniValue("system", "URL", "Class");
	UClass* pawnClass = packages->FindClass(playerPawnClass);

	// No PreLogin for the local player, as the original's SpawnPlayActor
	// (Engine.dll 0x10396bd0) runs none: a remote player's goes through
	// Engine::PreLogin with the four arguments GameInfo.PreLogin takes. The
	// three-argument call here bound the error to Address, so a GamePassword
	// in the ini, or an IPPolicies deny, threw at every local login.

	// Create viewport pawn
	UPlayerPawn* pawn = UObject::Cast<UPlayerPawn>(CallEvent(LevelInfo->Game(), EventName::Login, {
		ExpressionValue::StringValue(portal),
		ExpressionValue::StringValue(options),
		ExpressionValue::Variable(&error, stringProp),
		ExpressionValue::ObjectValue(pawnClass)
		}).ToObject());
	if (!pawn || !error.empty())
		Exception::Throw("GameInfo login failed: " + ScriptToUTF8(error));

	// The player's settings are its class's config, its defaults as the
	// class read them from the user ini and each SaveConfig read them again
	// (UClass::LoadProperties), as the original's spawned pawn has them;
	// nothing reads the ini again here (bugs.md ENG.24).

	// Assign the pawn to the viewport
	viewport->Actor() = pawn;
	viewport->Actor()->Player() = viewport;
	CallEvent(viewport->Actor(), EventName::Possess);

	// The local player is the authority, simulated on any client, its view
	// showing the actors (the overlays with them) and lit, as the original's
	// SpawnPlayActor for a viewport has it (Engine.dll 0x10396bd0), a loaded
	// or travelled player too.
	pawn->Role() = ROLE_Authority;
	pawn->RemoteRole() = ROLE_SimulatedProxy;
	pawn->ShowFlags() = 0x480c;
	pawn->RendMap() = 5;
	MakeAdmin(pawn, LevelNetDriver != nullptr);

	// Transfer travel actors to the new map

	CallEvent(pawn, EventName::TravelPreAccept);
	AcceptTravel(pawn, url);
	CallEvent(pawn, EventName::TravelPostAccept);
	CallEvent(LevelInfo->Game(), EventName::PostLogin, { ExpressionValue::ObjectValue(pawn) });

	ClearIgnoreNextShowMenu(pawn);

	// The sounds the load and the login started stop, the music playing on:
	// the original's map load hands its audio the viewport it has once the
	// players are in (UGameEngine::LoadMap, 0x1038dfc6). The new player's
	// PostLogin has asked for the level's song: the one playing fades out
	// first.
	audiodev->StopEffects();
	render->OnMapLoaded();
}

// What an arriving player takes of the level's travel info, as the
// original's SpawnPlayActor takes it (Engine.dll 0x10396bd0), a local
// player's and a remote one's alike: on any login but a ?loadonly one,
// whatever the travel, and whether or not Login spawned a pawn -- a Login
// that reuses a pawn keeps its travelling items; a save's load, a restart
// and the LoadMap window's map log in with ?loadonly and take none. The
// level holds travel info only after a travel with items. The info is
// keyed by the player's PlayerName, which the stock Login cuts to 20
// characters: the URL's name is looked up whole and in any case, as the
// original's TMap finds it; then, DXEngine's own, cut to 20; standalone,
// with none found, the first entry, as the original's. Then the travelled
// items' TravelPreAccept, the game's AcceptInventory and the items'
// TravelPostAccept, each run over the items last to first, inside the
// player's own pair, as the original's SpawnPlayActor runs them: an item's
// GiveTo then links the chain in the original's order, and the player, whose
// travel entry is among the accepted, gets each of its events once.
void Engine::AcceptTravel(UPlayerPawn* pawn, const UnrealURL& url)
{
	const std::map<std::string, std::string>& travelInfo = Level->TravelInfo;
	auto find = [&](const std::string& name)
	{
		return std::find_if(travelInfo.begin(), travelInfo.end(), [&](const auto& entry) { return StrTools::equals_ignore_case(entry.first, name); });
	};

	Array<UObject*> acceptedObjects;
	if (!HasWholeOption(url, "loadonly") && !travelInfo.empty())
	{
		std::string playerName = url.GetOption("Name");
		if (playerName.empty())
			playerName = packages->GetIniValue("system", "URL", "Name");

		auto it = find(playerName);
		if (it == travelInfo.end() && playerName.size() > 20)
			it = find(playerName.substr(0, 20));
		if (it == travelInfo.end() && LevelInfo->NetMode() == NM_Standalone)
			it = travelInfo.begin();
		if (!playerName.empty() && it != travelInfo.end())
			acceptedObjects = ActorTravelInfo::Accept(pawn, it->second);
		else if (playerName.empty())
			LogMessage("Skipping travel transfer. Player name is empty");
		else
			LogMessage("Skipping travel transfer. Player '" + playerName + "' not found in travel info");
	}

	for (size_t i = acceptedObjects.size(); i-- > 0;)
	{
		if (acceptedObjects[i] != pawn)
			CallEvent(acceptedObjects[i], EventName::TravelPreAccept);
	}

	CallEvent(LevelInfo->Game(), EventName::AcceptInventory, { ExpressionValue::ObjectValue(pawn) });

	for (size_t i = acceptedObjects.size(); i-- > 0;)
	{
		if (acceptedObjects[i] != pawn)
			CallEvent(acceptedObjects[i], EventName::TravelPostAccept);
	}
}

void Engine::LoginEntryPlayer(const UnrealURL& url)
{
	viewport->Actor() = nullptr;
	if (!EntryLevel || Level == EntryLevel)
		return;

	// LoginPlayer logs in to the engine's level: Entry's for the call, as
	// ReturnToEntry plays it, the game's put back after. Its OnMapLoaded
	// sets up Entry's lights, which are its own (ULevel::Light).
	ULevel* level = Level;
	ULevelInfo* levelInfo = LevelInfo;
	Package* levelPackage = LevelPackage;
	UGameInfo* gameInfo = GameInfo;
	UDeusExLevelInfo* deusExLevelInfo = DeusExLevelInfo;
	Level = EntryLevel;
	LevelInfo = EntryLevelInfo;
	LevelPackage = EntryLevelPackage;
	GameInfo = EntryGameInfo;
	DeusExLevelInfo = EntryDeusExLevelInfo;
	LoginPlayer(url);
	Level = level;
	LevelInfo = levelInfo;
	LevelPackage = levelPackage;
	GameInfo = gameInfo;
	DeusExLevelInfo = deusExLevelInfo;
}

void Engine::BeginConnect(const UnrealURL& url)
{
	CancelPending();
	LogMessage("Net: joining " + url.Host + ":" + std::to_string(url.Port));

	// Each join finds its packages afresh: another server's downloads are
	// not this one's.
	packages->RestorePackageFiles();

	PendingLevel = std::make_unique<NetPendingLevel>(url);
	if (!PendingLevel->Error.empty())
	{
		SetProgress("Networking Failed", PendingLevel->Error, 6.0f);
		LogMessage("Net: could not join: " + PendingLevel->Error);
		PendingLevel.reset();
	}
}

void Engine::TickPendingLevel(float realTimeElapsed)
{
	PendingLevel->Tick(realTimeElapsed);

	// Refused, lost, or a package not to be had: the level plays on.
	std::string error = PendingLevel->Error;
	if (error.empty() && PendingLevel->Success && PendingLevel->FilesNeeded == 0)
		PendingLevel->VerifyPackages(error);
	if (!error.empty())
	{
		if (!PendingLevel->ErrorShown)
			SetProgress(LocalizeMessage("Engine", "Errors", "ConnectionFailed", "Connection failed"), error, 4.0f);
		LogMessage("Net: " + LocalizeMessage("Engine", "Errors", "Pending", "Pending connect to '%s' failed; %s", { PendingLevel->URL.ToString(), error }));
		CancelPending();
	}
	else if (PendingLevel->Success && PendingLevel->FilesNeeded == 0)
	{
		std::unique_ptr<NetPendingLevel> pending = std::move(PendingLevel);
		if (pending->IsDemo())
			LoadDemoMap(static_cast<NetDemoPendingLevel*>(pending.get()));
		else
			LoadClientMap(pending.get());
	}
}

void Engine::CancelPending()
{
	if (!PendingLevel)
		return;
	PendingLevel->Close();
	PendingLevel.reset();
}

void Engine::SetProgress(const std::string& line1, const std::string& line2, float seconds)
{
	// Set through the pawn's own functions: the lines, white, and until
	// when.
	UPlayerPawn* pawn = viewport ? viewport->Actor() : nullptr;
	if (!pawn)
		return;
	if (seconds == -1.0f)
		CallEvent(pawn, "ShowUpgradeMenu");
	Color white = { 255, 255, 255, 0 };
	CallEvent(pawn, "SetProgressMessage", { ExpressionValue::StringValue(line1), ExpressionValue::IntValue(0) });
	CallEvent(pawn, "SetProgressColor", { ExpressionValue::ColorValue(white), ExpressionValue::IntValue(0) });
	CallEvent(pawn, "SetProgressMessage", { ExpressionValue::StringValue(line2), ExpressionValue::IntValue(1) });
	CallEvent(pawn, "SetProgressColor", { ExpressionValue::ColorValue(white), ExpressionValue::IntValue(1) });
	CallEvent(pawn, "SetProgressTime", { ExpressionValue::FloatValue(seconds) });
}

void Engine::UpdateConnectingMessage(const NetPendingLevel& pending)
{
	// The viewport's player's time against its own level's, as the
	// original's (Engine.dll 0x1038895f): a player waiting in the Entry
	// level for its pawn by the Entry level's clock, not the game's.
	UPlayerPawn* pawn = viewport ? viewport->Actor() : nullptr;
	if (!pawn || !pawn->Level() || pawn->ProgressTimeOut() >= pawn->Level()->TimeSeconds())
		return;
	// A demo's playback shows its file's name alone (Engine.dll
	// 0x10388976-0x103889c5).
	if (pending.IsDemo())
	{
		SetProgress("", pending.URL.Map, 60.0f);
		return;
	}
	std::string line1 = LocalizeMessage("Engine", "Progress", "ConnectingText", "Connecting (F10 Cancels):");
	std::string line2 = LocalizeMessage("Engine", "Progress", "ConnectingURL", "unreal://%s/%s", { pending.URL.Host, pending.URL.Map });
	SetProgress(line1, line2, 60.0f);
}

void Engine::LoadClientMap(NetPendingLevel* pending)
{
	UnrealURL url = pending->URL;
	LogMessage("Net: loading " + url.Map + " as a client");
	// A map that fails its check gives the join up, Engine's ConnectionFailed
	// over the load's error for 4 s, as the original's tick takes it
	// (UGameEngine::Tick, Engine.dll 0x10390f1e-0x10390f71): the level plays on.
	std::string error;
	if (!LoadMap(url, {}, true, &error))
	{
		SetProgress(LocalizeMessage("Engine", "Errors", "ConnectionFailed", "Connection failed"), error, 4.0f);
		pending->Close();
		return;
	}

	// Another server's downloads go once this one's map is in, as the
	// original's map load collects them.
	Array<NameString> keep;
	for (const NetPendingLevel::UsedPackage& package : pending->Uses)
		keep.push_back(package.Name);
	packages->ReleaseNetPackages(keep);

	CameraActor = nullptr;
	LoginEntryPlayer(url);
	// As after any map's load (LoginPlayer): the sounds stop, the music
	// plays on.
	audiodev->StopEffects();
	render->OnMapLoaded();

	// The driver is the level's now.
	LevelNetDriver = std::move(pending->Driver);
	ClientLevel = std::make_unique<NetClientLevel>();
	LevelNetDriver->Notify = ClientLevel.get();
	NetConnection* connection = LevelNetDriver->ServerConnection.get();

	// Objects by number in the server's order of packages; the map is the
	// level just loaded (checked as the server's before it loaded).
	NetPackageMap& map = connection->PackageMap;
	map.List.clear();
	for (const NetPendingLevel::UsedPackage& used : pending->Uses)
	{
		NetPackageMap::PackageInfo info;
		bool isLevel = StrTools::equals_ignore_case(used.Name.ToString(), LevelPackage->GetPackageName().ToString());
		info.Pkg = isLevel ? LevelPackage : packages->GetPackage(used.Name);
		info.Name = used.Name;
		info.Guid = used.Guid;
		info.Flags = used.Flags;
		info.FileSize = used.Size;
		info.RemoteGeneration = used.Generation;
		map.List.push_back(info);
	}
	map.Compute();

	// Where this side has another generation of a package than the server,
	// the server is told which.
	for (const NetPackageMap::PackageInfo& info : map.List)
	{
		if (info.LocalGeneration != info.RemoteGeneration)
			connection->SendText("HAVE GUID=" + info.Guid + " GEN=" + std::to_string(info.LocalGeneration));
	}

	if (!pending->LonePlayer)
	{
		LevelInfo->LevelAction() = LEVACT_Connecting;
		if (EntryLevelInfo)
			EntryLevelInfo->LevelAction() = LEVACT_Connecting;

		// A frame drawn before the JOIN goes, the join still pending, as the
		// original's game engine draws one there (UGameEngine::Tick,
		// Engine.dll 0x1039106e, its client's tick), then the frame's own
		// after it: so the player waiting in the Entry level is given the
		// join's lines (UpdateConnectingMessage) for the next 60 s, and the
		// console's tick in this draw opens the game's window for them.
		Draw(0.0f, 0.0f, pending);

		connection->SendText("JOIN");
		connection->FlushNet();
	}
}

void Engine::HandleClientPlayer(NetConnection* connection, UPlayerPawn* pawn)
{
	LogMessage("Net: joined as " + pawn->Name.ToString());

	if (viewport->Actor())
		viewport->Actor()->Player() = nullptr;

	// The connection's speed and the server's update intervals, the
	// viewport's now: the player's moves are paced by them.
	viewport->CurrentNetSpeed() = connection->CurrentNetSpeed;
	for (UProperty* prop : viewport->PropertyData.Class->Properties)
	{
		if (prop->Name == "StaticUpdateInterval")
			*static_cast<float*>(viewport->GetProperty(prop)) = connection->StaticUpdateInterval;
		else if (prop->Name == "DynamicUpdateInterval")
			*static_cast<float*>(viewport->GetProperty(prop)) = connection->DynamicUpdateInterval;
	}

	pawn->Role() = ROLE_AutonomousProxy;
	pawn->ShowFlags() = 0x480c;
	pawn->RendMap() = 5;

	viewport->Actor() = pawn;
	pawn->Player() = viewport;
	CallEvent(pawn, EventName::Possess);
	ReleaseHeldInputKeys();

	LevelInfo->LevelAction() = LEVACT_None;
	if (EntryLevelInfo)
		EntryLevelInfo->LevelAction() = LEVACT_None;
	connection->State = ConnectionState::Open;
	connection->Actor = pawn;
}

void Engine::CloseNetDriver()
{
	if (!LevelNetDriver)
		return;
	// The other side hears the close with the control channel's.
	auto close = [](NetConnection* connection) {
		if (NetChannel* control = connection->Channels[0])
			control->Close();
		connection->FlushNet();
	};
	if (NetConnection* connection = LevelNetDriver->ServerConnection.get())
		close(connection);
	for (auto& connection : LevelNetDriver->ClientConnections)
		close(connection.get());
	LevelNetDriver.reset();
	ClientLevel.reset();
	ServerLevel.reset();
}

// DEMOREC's recording (ULevel::Exec, Engine.dll 0x1039dfe0;
// UDemoRecDriver::InitListen 0x10407870): the driver [Engine.Engine]
// DemoRecordingDevice names, its file made, or why not said. In single
// player and on a server an Engine.DemoRecSpectator is spawned at the
// level's origin, through which the playback's messages go, and the demo
// takes the packages a server's clients are told of; on a client, its
// server's. Then the welcome a client gets: CLIENTDEMO in single player and
// on a client, SERVERDEMO on a server.
void Engine::StartDemoRecord(const std::string& filename, ExecOutput& out)
{
	auto driver = std::make_unique<NetDemoDriver>(NetDemoDriverSection());
	auto notify = std::make_unique<NetDemoRecordLevel>();
	std::string error;
	if (!driver->InitRecord(notify.get(), filename, error))
	{
		out.Log("Demo recording failed: " + error);
		return;
	}
	LevelDemoDriver = std::move(driver);
	DemoNotify = std::move(notify);

	bool client = LevelInfo->NetMode() == NM_Client;
	Array<NetPackageMap::PackageInfo> masterMap;
	if (client)
	{
		if (LevelNetDriver && LevelNetDriver->ServerConnection)
			masterMap = LevelNetDriver->ServerConnection->PackageMap.List;
	}
	else
	{
		// The original's spawn that fails ends the game (SpawnDemoRecSpectator
		// 0x10408690, a cast of None): logged here, the recording going on
		UClass* cls = packages->FindClass("Engine.DemoRecSpectator");
		UActor* spectator = cls ? LevelInfo->Spawn(cls, nullptr, NameString(), vec3(0.0f), Rotator(0, 0, 0)) : nullptr;
		if (!UObject::TryCast<UPlayerPawn>(spectator))
			LogMessage("Demo: no Engine.DemoRecSpectator could be spawned");
		masterMap = NetBuildMasterMap(LevelPackage, GameInfo);
	}

	NetConnection* connection = LevelDemoDriver->RecordConnection();
	connection->CreateChannel(ChannelType::Control, true, 0);
	NetWelcome(connection, masterMap, LevelPackage, client || LevelInfo->NetMode() == NM_Standalone ? "CLIENTDEMO" : "SERVERDEMO");
	connection->PackageMap.Compute();
	out.Log("Demo recording started to " + filename);
}

// DEMOPLAY's playback (ULevel::Exec, Engine.dll 0x1039dfe0): a join under
// way given up, then a pending level of the demo's, which opens its file;
// one that cannot says why and is let go.
void Engine::StartDemoPlay(const UnrealURL& url, ExecOutput& out)
{
	CancelPending();
	packages->RestorePackageFiles();
	auto pending = std::make_unique<NetDemoPendingLevel>(url);
	if (!pending->Driver)
	{
		out.Log("Demo playback failed: " + pending->Error);
		return;
	}
	PendingLevel = std::move(pending);
}

// The level's demo let go: a recording's last packet, closing its control
// channel, goes into the file before it closes.
void Engine::CloseDemoDriver()
{
	if (!LevelDemoDriver)
		return;
	LevelDemoDriver.reset();
	DemoNotify.reset();
}

// A recording's tick, after the level's (ULevel::TickDemoRecord, Engine.dll
// 0x103a4b90): every actor the demo may have, each tick, none passed over
// for relevance or for its rate -- an actor with a remote role, or on a
// client one of the server's; a dynamic one or a zone; one sent once to
// keep, once; none whose class is static that is not --, replicated with
// bDemoRecording set, and on a client bClientDemoRecording and the roles
// exchanged, as its server would send it.
void Engine::TickDemoRecord()
{
	NetConnection* connection = LevelDemoDriver->RecordConnection();
	if (!connection)
		return;
	bool client = LevelInfo->NetMode() == NM_Client;
	NetPackageMap& map = connection->PackageMap;
	const PropertyDataOffset isStatic = PropOffsets_Actor.bStatic;
	for (size_t i = 0; i < Level->Actors.size(); i++)
	{
		UActor* actor = Level->Actors[i];
		if (!actor)
			continue;
		if (actor->RemoteRole() == ROLE_None && !(client && actor->Role() != ROLE_None && actor->Role() != ROLE_Authority))
			continue;
		if (i < Level->FirstDynamicActor && !UObject::TryCast<UZoneInfo>(actor))
			continue;
		const Array<UActor*>& sent = connection->SentTemporaries;
		if (actor->bNetTemporary() && std::find(sent.begin(), sent.end(), actor) != sent.end())
			continue;
		const uint8_t* defaults = static_cast<const uint8_t*>(actor->Class->PropertyData.Data);
		bool staticClass = (*reinterpret_cast<const uint32_t*>(defaults + isStatic.DataOffset) & isStatic.BitfieldMask) != 0;
		if (!actor->bStatic() && staticClass)
			continue;

		NetActorChannel* channel = connection->FindActorChannel(actor);
		if (!channel && map.ObjectToIndex(actor->Class) != -1)
		{
			channel = static_cast<NetActorChannel*>(connection->CreateChannel(ChannelType::Actor, true));
			if (channel)
				channel->SetChannelActor(actor);
		}
		// The original asserts a channel is had and is not closing
		if (!channel || channel->Closing || !channel->IsNetReady(false))
			continue;

		actor->bDemoRecording() = true;
		actor->bClientDemoRecording() = client;
		if (client)
			std::swap(actor->Role(), actor->RemoteRole());
		channel->ReplicateActor();
		if (client)
			std::swap(actor->Role(), actor->RemoteRole());
		actor->bDemoRecording() = false;
		actor->bClientDemoRecording() = false;
	}
}

// A playback's tick, before the level's (ULevel::TickDemoPlayback,
// Engine.dll 0x103a4f70): the level's Connecting action ends once its player
// is in, and the demo's end takes the level back to Entry.
void Engine::TickDemoPlayback()
{
	NetConnection* connection = LevelDemoDriver->ServerConnection.get();
	if (LevelInfo->LevelAction() == LEVACT_Connecting && connection->State != ConnectionState::Pending)
	{
		LevelInfo->LevelAction() = LEVACT_None;
		SetProgress("", "", 0.0f);
	}
	if (connection->State == ConnectionState::Closed)
		ClientTravel("?entry", ETravelType::TRAVEL_Absolute, false);
}

// A playback's map, once its WELCOME has come (UGameEngine::Tick, Engine.dll
// 0x10390f1e; LoadMap 0x1038c1f0): the level the demo used, loaded as a
// client's, its player waiting in the Entry level; the demo's driver the
// level's, its packages numbered as the recording's; and a frame drawn
// under the Connecting action, as for any join, the demo's WELCOME giving no
// LONE=. Nothing is sent: a playback's join is no JOIN
// (UDemoPlayPendingLevel::SendJoin 0x10405da0).
void Engine::LoadDemoMap(NetDemoPendingLevel* pending)
{
	UnrealURL url = pending->URL;
	url.Map = pending->LevelName;
	LogMessage("Demo: loading " + url.Map);
	std::string error;
	if (!LoadMap(url, {}, true, &error))
	{
		SetProgress(LocalizeMessage("Engine", "Errors", "ConnectionFailed", "Connection failed"), error, 4.0f);
		return;
	}
	// The level's URL is the demo's, as the original's map load gives a
	// level its pending level's (LoadMap 0x1038c1f0): its GetLocalURL names
	// the demo's file.
	LevelInfo->URL = pending->URL;
	LastURL = pending->URL;

	Array<NameString> keep;
	for (const NetPendingLevel::UsedPackage& package : pending->Uses)
		keep.push_back(package.Name);
	packages->ReleaseNetPackages(keep);

	CameraActor = nullptr;
	LoginEntryPlayer(pending->URL);
	audiodev->StopEffects();
	render->OnMapLoaded();

	LevelDemoDriver.reset(static_cast<NetDemoDriver*>(pending->Driver.release()));
	DemoNotify = std::make_unique<NetDemoClientLevel>();
	LevelDemoDriver->Notify = DemoNotify.get();

	NetPackageMap& map = LevelDemoDriver->ServerConnection->PackageMap;
	map.List.clear();
	for (const NetPendingLevel::UsedPackage& used : pending->Uses)
	{
		NetPackageMap::PackageInfo info;
		bool isLevel = StrTools::equals_ignore_case(used.Name.ToString(), LevelPackage->GetPackageName().ToString());
		info.Pkg = isLevel ? LevelPackage : packages->GetPackage(used.Name);
		info.Name = used.Name;
		info.Guid = used.Guid;
		info.Flags = used.Flags;
		info.FileSize = used.Size;
		info.RemoteGeneration = used.Generation;
		map.List.push_back(info);
	}
	map.Compute();

	LevelInfo->LevelAction() = LEVACT_Connecting;
	if (EntryLevelInfo)
		EntryLevelInfo->LevelAction() = LEVACT_Connecting;
	Draw(0.0f, 0.0f, pending);
}

// A playback's player (UDemoRecConnection::HandleClientPlayer, Engine.dll
// 0x10406cc0): the recorded player's pawn the viewport's, as a joining
// client's; or with 3rdperson, and always in a server's demo, a spectator of
// the class DemoSpectatorClass names -- Engine.Spectator for none -- at the
// level's first PlayerStart, flying, an admin, owned here. Either way the
// player takes the level's song, faded in, and a console's timedemo plays
// the demo with no cap on its frames.
void Engine::HandleDemoClientPlayer(NetConnection* connection, UPlayerPawn* pawn)
{
	UPlayerPawn* player = pawn;
	if (!LevelDemoDriver->ClientThirdPerson)
	{
		HandleClientPlayer(connection, pawn);
	}
	else
	{
		// The stock ini names UnrealShare.UnrealSpectator, which Deus Ex has
		// not: the original's load of it ends the game (0x10406dbf, a load
		// that may not fail); Engine.Spectator here, logged
		std::string className = LevelDemoDriver->DemoSpectatorClass.empty() ? std::string("Engine.Spectator") : LevelDemoDriver->DemoSpectatorClass;
		UClass* cls = packages->FindClass(className);
		if (!cls)
		{
			LogMessage("Demo: no spectator class " + className + ", Engine.Spectator taken");
			cls = packages->FindClass("Engine.Spectator");
		}
		vec3 location(0.0f);
		Rotator rotation(0, 0, 0);
		for (UActor* actor : pawn->XLevel()->Actors)
		{
			if (UObject::TryCast<UPlayerStart>(actor))
			{
				location = actor->Location();
				rotation = actor->Rotation();
				break;
			}
		}
		UPlayerPawn* spectator = UObject::TryCast<UPlayerPawn>(cls ? LevelInfo->Spawn(cls, nullptr, NameString(), location, rotation, true) : nullptr);
		if (!spectator)
		{
			LogMessage("Demo: no spectator could be spawned");
			return;
		}
		if (viewport->Actor())
			viewport->Actor()->Player() = nullptr;
		viewport->Actor() = spectator;
		spectator->Player() = viewport;
		CallEvent(spectator, EventName::Possess);
		spectator->Role() = ROLE_Authority;
		spectator->ShowFlags() = 0x480c;
		spectator->RendMap() = 5;
		spectator->bAdmin() = true;
		spectator->bNetOwner() = true;
		spectator->Physics() = PHYS_Flying;
		ReleaseHeldInputKeys();
		connection->State = ConnectionState::Open;
		connection->Actor = spectator;
		player = spectator;
	}

	player->Song() = LevelInfo->Song();
	player->SongSection() = LevelInfo->SongSection();
	player->CdTrack() = LevelInfo->CdTrack();
	player->Transition() = MTRAN_Fade;
	if (console && console->bTimeDemo())
		LevelDemoDriver->NoFrameCap = true;
}

void Engine::Listen(const UnrealURL& url)
{
	// The URL's port -- the game's [URL] Port unless the URL names one -- or
	// the next free of the 20 from it, as the original's server binds; with
	// PORT= on the command line that port alone, appAtoi of the text after
	// it (IpDrv.dll UTcpNetDriver::InitBase 0x1070abb0, Core's int Parse
	// 0x10147730, true whenever it finds the name)
	int port = url.Port;
	int tries = 20;
	if (OriginalCommandLine::Get().Number("PORT", port))
		tries = 1;
	ServerLevel = std::make_unique<NetServerLevel>(LevelPackage);
	LevelNetDriver = std::make_unique<NetDriver>();
	std::string error;
	if (!LevelNetDriver->InitListen(ServerLevel.get(), port, tries, error))
	{
		LogMessage("Net: cannot listen: " + error);
		LevelNetDriver.reset();
		ServerLevel.reset();
		LevelInfo->NetMode() = NM_Standalone;
		return;
	}
	port = LevelNetDriver->BoundPort;
	LogMessage("Net: listening on port " + std::to_string(port));
	// The level's URL takes the address and port bound, as the original's
	// net driver writes them into it (IpDrv.dll UTcpNetDriver::InitListen):
	// GetLocalURL reads 0.0.0.0/<map>?..., GetAddressURL 0.0.0.0:<port>.
	LevelInfo->URL.Host = NetAddressString(LevelNetDriver->LocalAddr);
	LevelInfo->URL.Port = port;

	// The game engine's ServerActors -- in the section of the class
	// [Engine.Engine] GameEngine names --, each a class and then its config
	// values as Key=Value: the LAN beacon, the query answerer, the master
	// servers' uplinks.
	std::string gameEngine = packages->GetIniValue("system", "Engine.Engine", "GameEngine", "Engine.GameEngine");
	for (const std::string& entry : packages->GetIniValues("system", gameEngine, "ServerActors"))
	{
		Array<std::string> tokens;
		for (size_t pos = 0; pos < entry.size();)
		{
			pos = entry.find_first_not_of(" \t", pos);
			if (pos == std::string::npos)
				break;
			std::string token;
			while (pos < entry.size() && entry[pos] != ' ' && entry[pos] != '\t')
			{
				if (entry[pos] == '"')
				{
					size_t end = entry.find('"', pos + 1);
					token += entry.substr(pos + 1, end == std::string::npos ? std::string::npos : end - pos - 1);
					pos = end == std::string::npos ? entry.size() : end + 1;
				}
				else
				{
					token += entry[pos++];
				}
			}
			tokens.push_back(token);
		}
		if (tokens.empty())
			continue;

		LogMessage("Net: spawning " + tokens[0]);
		UClass* cls = packages->FindClass(tokens[0]);
		UActor* actor = cls ? LevelInfo->Spawn(cls, nullptr, NameString(), vec3(0.0f), Rotator(0, 0, 0)) : nullptr;
		if (!actor)
			continue;
		for (size_t i = 1; i < tokens.size(); i++)
		{
			size_t equals = tokens[i].find('=');
			if (equals == std::string::npos)
				continue;
			NameString key = tokens[i].substr(0, equals);
			for (UProperty* prop : actor->PropertyData.Class->Properties)
			{
				if (prop->Name == key && AnyFlags(prop->PropFlags, PropertyFlags::Config))
					prop->SetValueFromString(actor->PropertyData.Ptr(prop), tokens[i].substr(equals + 1));
			}
		}
	}
	LevelInfo->NextSwitchCountdown() = LevelNetDriver->ServerTravelPause;
}

bool Engine::PreLogin(const std::string& options, const std::string& address, std::string& error, std::string& failcode)
{
	// The script's out strings are 16-bit: its own, made 8-bit after
	auto stringProp = GC::Alloc<UStrProperty>("", nullptr, ObjectFlags::NoFlags);
	ScriptString scriptError = UTF8ToScript(error), scriptFailcode = UTF8ToScript(failcode);
	UFunction* func = FindEventFunction(LevelInfo->Game(), "PreLogin");
	if (func && Frame::CallParms(func).size() >= 4)
	{
		CallEvent(LevelInfo->Game(), EventName::PreLogin, {
			ExpressionValue::StringValue(options),
			ExpressionValue::StringValue(address),
			ExpressionValue::Variable(&scriptError, stringProp),
			ExpressionValue::Variable(&scriptFailcode, stringProp),
			});
	}
	else
	{
		CallEvent(LevelInfo->Game(), EventName::PreLogin, {
			ExpressionValue::StringValue(options),
			ExpressionValue::Variable(&scriptError, stringProp),
			ExpressionValue::Variable(&scriptFailcode, stringProp),
			});
	}
	error = ScriptToUTF8(scriptError);
	failcode = ScriptToUTF8(scriptFailcode);
	// Refused by its error alone, as the original's server takes it
	// (ULevel::NotifyReceivedText, Engine.dll 0x1039f780): a fail code with
	// no error lets the player on.
	return error.empty();
}

UPlayerPawn* Engine::SpawnPlayActor(UObject* player, uint8_t remoteRole, const UnrealURL& url, std::string& error)
{
	auto stringProp = GC::Alloc<UStrProperty>("", nullptr, ObjectFlags::NoFlags);
	std::string playerPawnClass = url.GetOption("Class");
	if (playerPawnClass.empty())
		playerPawnClass = packages->GetIniValue("system", "URL", "Class");
	UClass* pawnClass = packages->FindClass(playerPawnClass);

	ScriptString scriptError = UTF8ToScript(error);
	UPlayerPawn* pawn = UObject::Cast<UPlayerPawn>(CallEvent(LevelInfo->Game(), EventName::Login, {
		ExpressionValue::StringValue(url.GetPortal()),
		ExpressionValue::StringValue(url.GetOptions()),
		ExpressionValue::Variable(&scriptError, stringProp),
		ExpressionValue::ObjectValue(pawnClass)
		}).ToObject());
	error = ScriptToUTF8(scriptError);
	if (!pawn)
	{
		LogMessage("Net: login failed: " + error);
		return nullptr;
	}

	// Possessed by its player, the server's own with its connection's roles.
	if (UPlayer* p = UObject::Cast<UPlayer>(player))
		p->Actor() = pawn;
	pawn->Player() = UObject::Cast<UPlayer>(player);
	CallEvent(pawn, EventName::Possess);
	LogMessage("Net: possessed " + pawn->Name.ToString());
	pawn->Role() = ROLE_Authority;
	pawn->RemoteRole() = remoteRole;
	pawn->ShowFlags() = 0x480c;
	pawn->RendMap() = 5;
	MakeAdmin(pawn, LevelNetDriver != nullptr);

	CallEvent(pawn, EventName::TravelPreAccept);
	AcceptTravel(pawn, url);
	CallEvent(pawn, EventName::TravelPostAccept);
	CallEvent(LevelInfo->Game(), EventName::PostLogin, { ExpressionValue::ObjectValue(pawn) });
	return pawn;
}

UZoneInfo* Engine::GetZoneActor(int zoneIndex)
{
	if (auto zone = UObject::TryCast<UZoneInfo>(Level->Model->Zones[zoneIndex].ZoneActor))
		return zone;
	else
		return LevelInfo;
}

void Engine::TickConsole(float firstElapsed)
{
	using namespace std::chrono;
	uint64_t now = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	float elapsed = consoleLastTick ? (now - consoleLastTick) / 1'000'000.0f : firstElapsed;
	consoleLastTick = now;
	CallEvent(console, EventName::Tick, { ExpressionValue::FloatValue(elapsed) });
}

// A client runs no more frames a second than its connection's speed over 64,
// as the original's GetMaxTickRate has it (UGameEngine::GetMaxTickRate,
// Engine.dll 0x1038fc60): 40 at the default 2,600. A dedicated server runs
// [IpDrv.TcpNetDriver]'s rate, 10 to 120: 20 on the internet, 35 on a LAN
// (--lanplay). A listen server runs no more than 144, where the original's
// runs as fast as it draws: a remote client's queue drains its rate over a
// tick, and the faster the ticks the less room an unreliable call finds
// (dx-reverse-info/network.md, packets) -- the sounds a remote player hears
// go so.
float Engine::GetMaxTickRate()
{
	int maxTickRate = (LevelNetDriver && LevelNetDriver->ServerConnection) ? LevelNetDriver->ServerConnection->CurrentNetSpeed / 64 : 0;
	if (LevelNetDriver && !LevelNetDriver->ServerConnection && LaunchInfo.dedicatedServer)
		maxTickRate = std::clamp(LaunchInfo.lanPlay ? LevelNetDriver->LanServerMaxTickRate : LevelNetDriver->NetServerMaxTickRate, 10, 120);
	else if (LevelDemoDriver && !LevelDemoDriver->ServerConnection && !(LevelNetDriver && LevelNetDriver->ServerConnection))
	{
		// A recording runs at its driver's rate, 10 to 120: 60 with the stock
		// [Engine.DemoRecDriver]; with --lanplay the level's net driver's LAN
		// rate, which the original's reads in single player too, where the
		// level has none (Engine.dll 0x1038fd9c-0x1038fdb9): the demo
		// driver's own there.
		int rate = LevelDemoDriver->NetServerMaxTickRate;
		if (LaunchInfo.lanPlay)
			rate = LevelNetDriver ? LevelNetDriver->LanServerMaxTickRate : LevelDemoDriver->LanServerMaxTickRate;
		maxTickRate = std::clamp(rate, 10, 120);
	}
	else if (LevelNetDriver && !LevelNetDriver->ServerConnection)
		maxTickRate = 144;
	return (float)maxTickRate;
}

float Engine::CalcTimeElapsed()
{
	using namespace std::chrono;

	int maxTickRate = (int)GetMaxTickRate();
	if (maxTickRate > 0 && lastTime != 0)
	{
		uint64_t minDelta = 1'000'000 / (uint64_t)maxTickRate;
		uint64_t now = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
		if (now - lastTime < minDelta)
			std::this_thread::sleep_for(microseconds(minDelta - (now - lastTime)));
	}

	uint64_t currentTime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	if (lastTime == 0)
		lastTime = currentTime;

	// Never a frame of nothing: the first frame was 0 s, and a physics that
	// takes the move's velocity from its tick (flying, rolling, a projectile,
	// a mover, a path) divided 0 by 0 for every such actor at rest then -- a
	// generator's scout, spawned as the level began, had a NaN location for
	// good, out of every zone (ScoutConsole). The original's first frame is
	// the loop's own time, microseconds; this one is the shortest, 1 us.
	uint64_t deltaTime = currentTime - lastTime;
	if (deltaTime == 0)
		deltaTime = 1;
	lastTime = currentTime;

	// Deus Ex's frame is its whole time, a map's load included, as the
	// original's main loop hands it on (DeusEx.exe MainLoop 0x10914630): the
	// level's clock takes all of it, the actors and the player's input at
	// most 0.4 s of it (ULevel::Tick, Engine::UpdateInput).
	float seconds = deltaTime / 1'000'000.0f;
	return seconds;
}

void Engine::WritePendingSave()
{
	if (SaveGameInfo.SaveGameSlot == DONT_SAVE_GAME)
		return;
	SaveGameToSlot(SaveGameInfo.SaveGameSlot, SaveGameInfo.SaveGameDescription);
	SaveGameInfo.SaveGameSlot = DONT_SAVE_GAME;
	SaveGameInfo.SaveGameDescription.clear();
}

bool Engine::TakeTravel(float realTimeElapsed)
{
	const bool dedicated = LaunchInfo.dedicatedServer;

	// A server's next map, once its countdown has run out, by the frame's
	// real time as the original's counts it down; the original's tick takes
	// it before a client's travel and ends there.
	if (!LevelInfo->NextURL().empty())
	{
		LevelInfo->NextSwitchCountdown() -= realTimeElapsed;
		if (LevelInfo->NextSwitchCountdown() <= 0.0f)
		{
			if (LevelNetDriver)
				LogMessage("Net: server switch level: " + ScriptToUTF8(LevelInfo->NextURL()));
			if (UnrealURL(ScriptToUTF8(LevelInfo->NextURL())).HasOption("restart"))
			{
				DeleteSaveGameFiles("Current"); // the original's ?restart empties Current
				// Carries nothing: the original's Browse loads the map with no
				// travel info and ?loadonly, whose login takes none of it, the
				// player's own or its items' (Engine.dll's SpawnPlayActor,
				// 0x10396bd0). A map that does not load -- DXMP_Area51Bunker's
				// DeusExLevelInfo names none -- leaves the level playing, its
				// NextURL cleared, as below.
				UnrealURL restartURL = RestartURL();
				if (!LoadMap(restartURL, {}))
					LevelInfo->NextURL().clear();
				else if (!dedicated)
					LoginPlayer();
			}
			else
			{
				// With bNextItems the players' travel info, captured
				// before the pre-travel prune deletes what crosses in the
				// pawn's graph, as the original's travel save runs before
				// its Browse; without it none. A browse that loads no
				// level clears NextURL, as the original's clears it after
				// the browse, failed or not.
				auto travelInfo = LevelInfo->bNextItems() ? CreateServerTravelInfo() : std::map<std::string, std::string>();
				if (!BrowseMap(UnrealURL(&LastURL, ScriptToUTF8(LevelInfo->NextURL()), ETravelType::TRAVEL_Relative), travelInfo))
					LevelInfo->NextURL().clear();
			}
			return true;
		}
	}

	// The viewport's travel goes after its browse, the browse failed or not,
	// as the original's game engine empties it (UGameEngine::Tick, Engine.dll
	// 0x10390c01 the browse, 0x10390c07): one asked during the browse -- by a
	// console's tick under its SAVING or LOADING frame -- is dropped. A
	// server's next map, above, leaves one for the next frame (0x1039071c).
	if (ClientTravelInfo.URL.HasOption("restart"))
	{
		DeleteSaveGameFiles("Current"); // the original's ?restart empties Current
		// Carries nothing, as the NextURL's ?restart above. A map that does
		// not load leaves the level playing, as the original's load fails it
		// (LoadMap): 00_Intro's DeusExLevelInfo names Intro, 99_Endgame1 to
		// 3's Endgame1 to Endgame3, none a map.
		UnrealURL restartURL = RestartURL();
		if (LoadMap(restartURL, {}) && !dedicated)
			LoginPlayer();
		ClientTravelInfo.URL.Clear();
		return true;
	}

	// A slot's load is ?loadgame=N, as the original's Browse asks for
	// "loadgame=" (DeusEx.dll 0x10010f57): a bare ?loadgame is a map's
	// browse, its map the default one, and so is a URL with a ?load= of any
	// value, which the game engine's browse leaves to Deus Ex's, loading no
	// save (Engine.dll 0x1038b61a-0x1038b65a): its value the map the browse
	// tests for its mission and looks for in Current (MissionMapName).
	if (HasValueOption(ClientTravelInfo.URL, "loadgame"))
	{
		UnrealURL url(ClientTravelInfo.URL);
		LoadSavedGame(url);
		ClientTravelInfo.URL.Clear();
		return true;
	}

	// Lost the server, or refused by it after the join: to the Entry
	// level, as the original's client level browses ?failed
	// (ULevel::TickNetClient, NotifyReceivedText) -- a join pending or
	// not: a server that travels closes the old connection as its clients
	// join it again, and the join then loads the next map.
	if (LevelNetDriver && LevelNetDriver->ServerConnection && (!NetFailure.empty() || LevelNetDriver->ServerConnection->State == ConnectionState::Closed))
	{
		LogMessage("Net: disconnected" + (NetFailure.empty() ? std::string() : ": " + NetFailure));
		NetFailure.clear();
		// The viewport's next travel, absolute, as the original's sets it:
		// the player's options from the last URL, the server's, on it
		ClientTravel("?failed", ETravelType::TRAVEL_Absolute, false);
	}

	// A URL of another protocol than the game's is no map and no server: the
	// original's browse hands it to the system to open (UGameEngine::Browse,
	// Engine.dll 0x1038b85f-0x1038b890; appLaunchURL, Core.dll 0x1016c770,
	// logs LaunchURL and the URL and opens it with ShellExecute), any error
	// dropped, the level playing on. DXEngine logs it alike and opens
	// nothing, by choice: a server's ClientTravel would open any URL, any
	// protocol's handler, on its clients' systems.
	if (!ClientTravelInfo.URL.IsInternal() && !ClientTravelInfo.URL.HasOption("failed") && !ClientTravelInfo.URL.HasOption("entry"))
	{
		LogMessage("LaunchURL " + ClientTravelInfo.URL.ToString() + " ");
		ClientTravelInfo.URL.Clear();
		return true;
	}

	if (!ClientTravelInfo.URL.Host.empty())
	{
		UnrealURL url(ClientTravelInfo.URL);
		BeginConnect(url);
		ClientTravelInfo.URL.Clear();
		return true;
	}
	else if (!ClientTravelInfo.URL.Map.empty())
	{
		UnrealURL url(ClientTravelInfo.URL);
		LogMessage("Client travel to " + url.ToString());
		// Captured before the pre-travel prune deletes what crosses in
		// the pawn's graph.
		auto travelInfo = CreateClientTravelInfo(ClientTravelInfo.TransferItems);
		BrowseMap(url, travelInfo);
		ClientTravelInfo.URL.Clear();
		return true;
	}
	return false;
}

// A device's class named bare or with its package: the game's SET and GET
// name a class without it (set D3DRenderDevice Coronas False), as the
// original's StaticExec finds a class in any package
static bool IsClassNamed(const NameString& name, const std::string& qualified)
{
	if (name == qualified)
		return true;
	size_t dot = qualified.find_last_of('.');
	return dot != std::string::npos && name == qualified.substr(dot + 1);
}

std::string Engine::ParseClassName(std::string className)
{
	// Workaround for broken unrealscript code referencing windrv.windowsclient directly
	if (className == "windrv.windowsclient")
	{
		className = "ini:Engine.ViewportManager";
	}

	if (className.size() < 4 || className.substr(0, 4) != "ini:")
		return className;

	size_t pos = className.find_last_of('.');
	if (pos == std::string::npos)
		Exception::Throw("Parse error");

	NameString sectionName = className.substr(4, pos - 4);
	NameString keyName = className.substr(pos + 1);

	// Override the ini file for things that are internal in DXEngine
	if (sectionName == "Engine.Engine")
	{
		if (keyName == "GameRenderDevice" || keyName == "WindowedRenderDevice")
		{
			return renderdev->Class;
		}
		else if (keyName == "AudioDevice")
		{
			return audiodev->Class;
		}
		else if (keyName == "NetworkDevice")
		{
			return netdev->Class;
		}
		else if (keyName == "ViewportManager")
		{
			return client->Class;
		}
	}

	return packages->GetIniValue("system", sectionName, keyName);
}

// One of the original's words as its ParseCommand matches it (Core.dll
// 0x10147a20): the line's word, as CommandWord takes it, in any case, with
// rest the text past it and the blanks and tabs after it.
static bool ParseWord(const std::string& line, const char* word, std::string& rest)
{
	std::string after;
	if (CommandWord(line, after) != word)
		return false;
	rest = after;
	return true;
}

// An object as the original's GetFullName names it: its class's name and its
// path name (Class DeusEx.DeusExPlayer).
static std::string FullName(UObject* obj)
{
	return obj ? obj->Class->Name.ToString() + " " + obj->GetPathName() : std::string("None");
}

static std::string Printf(const char* format, ...)
{
	char text[1024];
	va_list args;
	va_start(args, format);
	std::vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	return text;
}

// A pawn's ConsoleCommand goes to its own player (APlayerPawn::
// execConsoleCommand, Engine.dll 0x103ba200): the local player's to the
// viewport, a remote player's on a server to its connection; a pawn with no
// player runs nothing. What the command wrote, run together, is the answer.
std::string Engine::PawnConsoleCommand(UPlayerPawn* pawn, const std::string& line)
{
	ExecOutput out;
	if (!pawn || !pawn->Player())
		return {};
	if (viewport && pawn->Player() == viewport)
		ViewportExec(line, out);
	else if (NetConnection* connection = NetConnectionOfPlayer(pawn->Player()))
		ConnectionExec(connection, line, out);
	return out.Joined();
}

// The local viewport's commands, in the original's order (UWindowsViewport::
// Exec, WinDrv.dll 0x11106d90, which asks UViewport::Exec first, Engine.dll
// 0x10373a60; engine-dll.md, a key's binding): the input's words and aliases,
// the render device's words, the viewport's own, the console's exec
// functions with the viewport's player the executor, then the player's own
// commands (PlayerExec), and the window's words last, once none of those has
// taken the line. So a word an exec function shares is the viewport's (SHOT)
// or the render device's (GETRES), or else the exec function's (TIMEDEMO,
// the console's; TOGGLEFULLSCREEN, SETRES).
bool Engine::ViewportExec(const std::string& line, ExecOutput& out)
{
	UPlayerPawn* actor = viewport ? viewport->Actor() : nullptr;
	if (InputExec(line, IK_None, EInputType::IST_Press, 20, &out))
		return true;
	if (RenderDeviceExec(line, out))
		return true;
	if (ViewportWordExec(actor, line, out))
		return true;
	if (ScriptExec(console, actor, line, out))
		return true;
	// The original's viewport always has its player; between levels, here,
	// the game engine's words all the same
	if (actor ? PlayerExec(actor, line, out) : EngineExec(line, out))
		return true;
	return WindowExec(line, out);
}

// The render device's words, which the viewport asks before its own: Engine's
// URenderDevice::Exec (Engine.dll 0x10371b60), GETDETAILTEXTURES and
// TOGGLEDETAILTEXTURES, then D3DDrv's (0x100061b0; bugs.md REN.44), GETRES,
// LODBIAS and SHOWPOOLS.
bool Engine::RenderDeviceExec(const std::string& line, ExecOutput& out)
{
	if (!render)
		return false;
	std::string rest;
	if (ParseWord(line, "getdetailtextures", rest))
	{
		// The render device's DetailTextures, 1 or 0 (Engine.dll 0x10371b8a)
		out.Log(renderdev->DetailTextures ? "1" : "0");
		return true;
	}
	if (ParseWord(line, "toggledetailtextures", rest))
	{
		// Flipped and the device's config saved, nothing said
		// (0x10371bd3-0x10371bfb)
		renderdev->DetailTextures = !renderdev->DetailTextures;
		renderdev->SaveConfig();
		FlushConfig();
		return true;
	}
	if (ParseWord(line, "getres", rest))
	{
		out.Log(window->GetMenuResolutions());
		return true;
	}
	if (ParseWord(line, "lodbias", rest))
	{
		// D3DDrv's (0x10006377): the number past the word, none 0, the mip
		// bias from the next frame until the device's mode is set again
		// (ModeSetAgain), neither held nor saved; its line the output
		float bias = (float)std::atof(rest.c_str());
		render->Device->SetLODBias(bias);
		out.Log(Printf("Texture LodBias %f", bias));
		return true;
	}
	if (ParseWord(line, "showpools", rest))
	{
		// D3DDrv's (0x10006404) lists its Direct3D 7 texture pools under the
		// line, which DXEngine has not: the line alone, as the original's
		// before any mode is set
		out.Log("Dumping D3D Texture Pool Info");
		return true;
	}
	return false;
}

// The viewport's own words (UViewport::Exec, Engine.dll 0x10373a60), after
// the input's and the device's, and DXEngine's DXWINDOWDEBUG with them.
bool Engine::ViewportWordExec(UPlayerPawn* actor, const std::string& line, ExecOutput& out)
{
	std::string rest;
	if (ParseWord(line, "getping", rest))
	{
		// A local player's ping is 0, whatever its connection (0x10373aee);
		// a remote player's connection answers its own (ConnectionExec), and
		// the viewport has no GETLOSS (bugs.md WIN.20)
		out.Log("0");
		return true;
	}
	if (ParseWord(line, "inject", rest))
	{
		// On a client of a server, the rest of the line sent it as a line of
		// text, said first (0x10373b27)
		NetConnection* server = actor && actor->XLevel() == Level && LevelNetDriver ? LevelNetDriver->ServerConnection.get() : nullptr;
		if (server)
		{
			out.Log("Injecting: " + rest);
			server->SendText(rest);
		}
		return true;
	}
	bool lan = false;
	if (ParseWord(line, "netspeed", rest) || (lan = ParseWord(line, "lanspeed", rest)))
	{
		// The rate asked of servers, the number past the word (appAtoi), kept
		// for the next join; on a server of that kind (?LAN or not), asked
		// now, 500 or more (0x10373b74). Both keep it as the internet
		// speed, as the original's both do.
		int rate = std::atoi(rest.c_str());
		LogMessage(std::string("Net: ") + (lan ? "lanspeed " : "netspeed ") + std::to_string(rate));
		packages->SetIniValue("system", "Engine.Player", "ConfiguredInternetSpeed", std::to_string(rate));
		// Saved at once, as the original's viewport saves its config
		// (UViewport::Exec, Engine.dll 0x10373bb3, 0x10373c44; bugs.md ENG.27)
		FlushConfig();
		NetConnection* connection = LevelNetDriver ? LevelNetDriver->ServerConnection.get() : nullptr;
		if (rate >= 500 && connection && LevelInfo->URL.HasOption("LAN") == lan)
		{
			connection->CurrentNetSpeed = std::min(rate, LevelNetDriver->MaxClientRate);
			viewport->CurrentNetSpeed() = connection->CurrentNetSpeed;
			connection->SendText("NETSPEED " + std::to_string(rate));
		}
		return true;
	}
	if (ParseWord(line, "showall", rest))
	{
		// Every actor of the player's level shown, its bHidden cleared
		// (0x10373cb4; bugs.md WIN.31)
		if (actor && actor->XLevel())
		{
			for (UActor* each : actor->XLevel()->Actors)
			{
				if (each)
					each->bHidden() = false;
			}
		}
		return true;
	}
	if (ParseWord(line, "report", rest))
	{
		// A report of the game into the clipboard, nothing said: the
		// engine's version, the player's class, the level's URL, the
		// player's place, and the game's class and difficulty or a network
		// client's line (0x10373cef)
		if (actor)
		{
			UDeusExPlayer* dxplayer = UObject::TryCast<UDeusExPlayer>(actor);
			std::string text = "Report:\r\n";
			text += "   Version: " + (dxplayer ? dxplayer->GetDeusExVersion() : std::string()) + "\r\n";
			text += "   Player class: " + actor->Class->Name.ToString() + "\r\n";
			text += "   URL: " + (actor->Level() ? actor->Level()->URL.ToString() : std::string()) + "\r\n";
			vec3 at = actor->Location();
			text += Printf("   Location: %i %i %i\r\n", (int)at.x, (int)at.y, (int)at.z);
			UGameInfo* game = actor->Level() ? actor->Level()->Game() : nullptr;
			if (game)
			{
				text += "   Game class: " + game->Class->Name.ToString() + "\r\n";
				text += Printf("   Difficulty: %i\r\n", (int)game->Difficulty());
			}
			else
			{
				text += "   Network client\r\n";
			}
			SetClipboard(text);
		}
		return true;
	}
	if (ParseWord(line, "shot", rest))
	{
		TakeScreenshot();
		return true;
	}
	if (ParseWord(line, "showactors", rest))
	{
		// The player's ShowFlags' SHOW_Actors set and cleared, which the
		// view's actors are drawn by
		if (actor)
			actor->ShowFlags() |= 8;
		return true;
	}
	if (ParseWord(line, "hideactors", rest))
	{
		if (actor)
			actor->ShowFlags() &= ~8;
		return true;
	}
	if (ParseWord(line, "rmode", rest))
	{
		// The player's RendMap, the number past the word from 1 to 19, any
		// other leaving it (0x103741e5; bugs.md WIN.28): 6, REN_PlainTex,
		// which DeusExPlayer.Matrix() sends for the credits' THEREISNOSPOON
		// egg, draws no light map and every actor unlit (SceneLit), until
		// the level's next player
		int mode = std::atoi(rest.c_str());
		if (actor && mode > 0 && mode < 20)
			actor->RendMap() = mode;
		return true;
	}
	if (ParseWord(line, "exec", rest))
	{
		// UViewport::Exec's EXEC (Engine.dll 0x10374241): the file the first
		// token past the word, by ParseToken into 64 characters -- a quoted
		// one whole, 63 characters at most, exec"x" x --; with none nothing
		// runs, the command taken all the same
		size_t pos = 0;
		std::string filename;
		if (StrTools::ue1_parse_token(rest, pos, filename, 64))
			ExecFile(filename, out);
		return true;
	}
	if (ParseWord(line, "dxwindowdebug", rest))
	{
		m_DrawDebugDXWindowHierarchy = !m_DrawDebugDXWindowHierarchy;
		return true;
	}
	return false;
}

// The window's own words, after all of the viewport's and the player's
// (UWindowsViewport::Exec, WinDrv.dll 0x11106d90; bugs.md WIN.19, WIN.23).
bool Engine::WindowExec(const std::string& line, ExecOutput& out)
{
	if (!window)
		return false;
	std::string rest;
	if (ParseWord(line, "endfullscreen", rest))
	{
		EndFullscreen();
		return true;
	}
	if (ParseWord(line, "togglefullscreen", rest))
	{
		ToggleFullscreen();
		return true;
	}
	if (ParseWord(line, "getcurrentres", rest))
	{
		// The window's own size, as GETRES lists it, not the render size
		out.Log(std::to_string(window->GetNativePixelWidth()) + "x" + std::to_string(window->GetNativePixelHeight()));
		return true;
	}
	if (ParseWord(line, "getcurrentcolordepth", rest))
	{
		// 32, by choice (NATIVES.md, the window): the original's 8 times the
		// viewport's bytes a pixel, 16 where it runs at 16 bits
		out.Log("32");
		return true;
	}
	if (ParseWord(line, "getcolordepths", rest))
	{
		out.Log("16 32");
		return true;
	}
	if (ParseWord(line, "getcurrentrenderdevice", rest))
	{
		// The render device's class by its path name: the one the ini names,
		// which DXEngine's device stands for, its WindowedRenderDevice with
		// -nohard, as the original's starts that in its GameRenderDevice's
		// place
		const char* key = OriginalCommandLine::Get().Param("nohard") ? "WindowedRenderDevice" : "GameRenderDevice";
		out.Log(packages->GetIniValue("system", "Engine.Engine", key));
		return true;
	}
	if (ParseWord(line, "setres", rest))
	{
		window->SetResolution(rest);
		ModeSetAgain();
		// The new size is kept as the windowed size, or full screen the
		// mode taken, which the ini saves, as the original's SETRES keeps it.
		int width = 0, height = 0;
		if (window->IsFullscreen())
		{
			Size mode = window->GetClosestResolution(Size(window->GetPixelWidth(), window->GetPixelHeight()));
			if (sscanf(rest.c_str(), "%dx%d", &width, &height) == 2 && width >= 640 && height >= 480)
				mode = window->GetClosestResolution(Size(width, height));
			client->SetPropertyFromString("FullscreenViewportX", std::to_string((int)mode.width));
			client->SetPropertyFromString("FullscreenViewportY", std::to_string((int)mode.height));
		}
		else if (sscanf(rest.c_str(), "%dx%d", &width, &height) == 2 && width >= 640 && height >= 480)
		{
			client->SetPropertyFromString("WindowedViewportX", std::to_string(width));
			client->SetPropertyFromString("WindowedViewportY", std::to_string(height));
			AskWindowSize(width, height);
		}
		// Saved at once, as the original's viewport's resize saves the
		// client's config (bugs.md WIN.18, ENG.27)
		SaveClientConfig();
		FlushConfig();
		return true;
	}
	if (ParseWord(line, "preferences", rest))
	{
		// The original's opens its Advanced Options window, the window out
		// of full screen first, which DXEngine has not (the log says so;
		// bugs.md D.81)
		LogMessage("preferences: DXEngine has no Advanced Options window");
		return true;
	}
	return false;
}

// A player's own commands (UPlayer::Exec, Engine.dll 0x103d7ed0): its level's
// words (LevelExec), then the exec functions of the game, its HUD, itself and
// each item of its inventory in turn, the player the executor, then the game
// engine's (EngineExec). The local player's come after the viewport's own
// (ViewportExec), a remote one's after its connection's (ConnectionExec): a
// remote admin's "admin say" is the admin's own Say.
bool Engine::PlayerExec(UPlayerPawn* pawn, const std::string& line, ExecOutput& out)
{
	if (!pawn)
		return false;
	if (LevelExec(pawn->XLevel(), line, out))
		return true;
	if (pawn->Level() && ScriptExec(pawn->Level()->Game(), pawn, line, out))
		return true;
	if (ScriptExec(pawn->myHUD(), pawn, line, out))
		return true;
	if (ScriptExec(pawn, pawn, line, out))
		return true;
	for (UInventory* item = pawn->Inventory(); item; item = item->Inventory())
	{
		if (ScriptExec(item, pawn, line, out))
			return true;
	}
	return EngineExec(line, out);
}

// A remote player's commands through its connection (UNetConnection::Exec,
// Engine.dll 0x10401c50): GETPING its lag in milliseconds and GETLOSS its
// incoming packet loss in percent, each after a blank, which
// PlayerReplicationInfo.Timer takes for the player's ping and loss -- the
// scoreboard's, the ping command's, a server's GameSpy answers --; then the
// player's own (PlayerExec) (bugs.md WIN.20), on the pawn its player object
// holds, as the original's UPlayer::Exec (0x103d7ed0) on its Actor: set as
// the login spawns the pawn, before its Possess, so what its root window's
// InitWindow sends there runs, ConditionalBindMultiplayerKeys' KEYNAME
// lines through the game's exec functions.
bool Engine::ConnectionExec(NetConnection* connection, const std::string& line, ExecOutput& out)
{
	std::string rest;
	if (ParseWord(line, "getping", rest))
	{
		out.Log(" " + std::to_string((int)(connection->BestLag * 1000.0f)));
		return true;
	}
	if (ParseWord(line, "getloss", rest))
	{
		out.Log(" " + std::to_string((int)connection->InLoss));
		return true;
	}
	UPlayer* player = UObject::TryCast<UPlayer>(connection->PlayerObject);
	return PlayerExec(player ? player->Actor() : connection->Actor, line, out);
}

// A level's words (ULevel::Exec, Engine.dll 0x1039dfe0): its net driver's,
// SOCKETS (UNetDriver::Exec 0x104099c0); its demo driver's while a demo
// runs (UDemoRecDriver::Exec 0x104083c0); then DEMOREC and DEMOPLAY.
bool Engine::LevelExec(ULevel* level, const std::string& line, ExecOutput& out)
{
	if (!level)
		return false;
	std::string rest;
	if (level == Level && LevelNetDriver && ParseWord(line, "sockets", rest))
	{
		// Each connection, its address and state as IpDrv describes one
		// (UTcpipConnection::LowLevelDescribe, IpDrv.dll 0x107098d0), and
		// each open channel's as Engine's channels describe themselves
		// (0x103fbe00, 0x103fc970, 0x103ffb50, 0x10400e20)
		auto describe = [](NetConnection* connection)
		{
			static const char* const states[] = { "Invalid", "Closed", "Pending", "Open" };
			int state = (int)connection->State;
			return Printf(" %s state: %s", connection->RemoteAddressString().c_str(), state >= 0 && state < 4 ? states[state] : "Invalid");
		};
		auto channels = [&](NetConnection* connection)
		{
			for (NetChannel* channel : connection->OpenChannels)
			{
				std::string text;
				if (auto actorChannel = dynamic_cast<NetActorChannel*>(channel))
				{
					UActor* actor = actorChannel->Actor;
					text = actor && !channel->Closing ? Printf("Actor=%s (Role=%i RemoteRole=%i) ", FullName(actor).c_str(), (int)actor->Role(), (int)actor->RemoteRole()) : std::string("Actor=None ");
				}
				else if (channel->ChType == ChannelType::Control)
				{
					text = "Text ";
				}
				text += channel->Closing ? "State=closing" : "State=open";
				out.Log(Printf("      Channel %i: %s", channel->ChIndex, text.c_str()));
			}
		};
		out.Log("Connections:");
		if (NetConnection* server = LevelNetDriver->ServerConnection.get())
		{
			// A client's server by the host its URL names, a server's
			// clients by none
			out.Log("   Server " + LastURL.Host + describe(server));
			channels(server);
		}
		for (const auto& client : LevelNetDriver->ClientConnections)
		{
			out.Log("   Client " + describe(client.get()));
			channels(client.get());
		}
		return true;
	}
	// While a demo runs, DEMOREC and DEMOPLAY say which; STOPDEMO ends it,
	// its frames counted: a recording's driver let go, its file closed, a
	// playback's connection closed, which takes the level back to Entry.
	if (level == Level && LevelDemoDriver)
	{
		if (ParseWord(line, "demorec", rest) || ParseWord(line, "demoplay", rest))
		{
			out.Log((LevelDemoDriver->ServerConnection ? "Demo playback currently active: " : "Demo recording currently active: ") + LevelDemoDriver->DemoFilename);
			return true;
		}
		if (ParseWord(line, "stopdemo", rest))
		{
			out.Log(Printf("Demo %s stopped (%d frames)", LevelDemoDriver->DemoFilename.c_str(), LevelDemoDriver->FrameNum));
			if (NetConnection* server = LevelDemoDriver->ServerConnection.get())
				server->State = ConnectionState::Closed;
			else
				CloseDemoDriver();
			return true;
		}
	}
	bool play = false;
	if (ParseWord(line, "demorec", rest) || (play = ParseWord(line, "demoplay", rest)))
	{
		// The name past the word (ParseToken), .dem added where it has not
		// that ending -- a playback's read as a URL, the map its file and
		// the options its own --, the attempt logged (0x1039e065,
		// 0x1039e2dd).
		size_t pos = 0;
		std::string name;
		if (!StrTools::ue1_parse_token(rest, pos, name, 0))
		{
			out.Log("You must specify a filename");
			return true;
		}
		auto withDem = [](std::string file)
		{
			if (file.size() < 4 || !StrTools::equals_ignore_case(file.substr(file.size() - 4), ".dem"))
				file += ".dem";
			return file;
		};
		if (play)
		{
			UnrealURL url(name);
			url.Map = withDem(url.Map);
			LogMessage("Attempting to play demo " + url.Map);
			StartDemoPlay(url, out);
		}
		else
		{
			name = withDem(name);
			LogMessage("Attempting to record demo " + name);
			StartDemoRecord(name, out);
		}
		return true;
	}
	return false;
}

// The game engine's commands: an actor's ConsoleCommand (AActor::
// execConsoleCommand, Engine.dll 0x103ded00), a dedicated server's typed
// line (D.65), the end of a player's (PlayerExec): Deus Ex's and the game
// engine's own words (GameEngineExec), the level's (LevelExec), the game's
// exec functions with no executor (UGameEngine::Exec, 0x1038a030), then the
// engine's words (EngineWordExec, UEngine::Exec 0x10380ee0). None of the
// viewport's, the render device's or the window's (bugs.md WIN.21).
bool Engine::EngineExec(const std::string& line, ExecOutput& out)
{
	if (GameEngineExec(line, out))
		return true;
	if (LevelExec(Level, line, out))
		return true;
	if (LevelInfo && ScriptExec(LevelInfo->Game(), nullptr, line, out))
		return true;
	return EngineWordExec(line, out);
}

// Deus Ex's game engine's SAVEGAME and DELETEGAME (DDeusExGameEngine::Exec,
// DeusEx.dll 0x1000ed70), then the game engine's own words (UGameEngine::
// Exec, Engine.dll 0x1038a030), before its level's.
bool Engine::GameEngineExec(const std::string& line, ExecOutput& out)
{
	std::string rest;
	std::string command = CommandWord(line, rest);
	if (command == "savegame")
	{
		// Deus Ex's game engine's SAVEGAME (DDeusExGameEngine::Exec, DeusEx.dll
		// 0x1000ed70): a save only when the text past the word begins with a
		// digit (0x1000edc2-0x1000edd3), its slot appAtoi of that text -- no
		// sign, whatever follows the digits dropped -- and its description
		// empty, so the save takes the level's Title; any other form is taken,
		// doing nothing.
		if (!rest.empty() && rest[0] >= '0' && rest[0] <= '9')
		{
			SaveGameInfo.SaveGameSlot = SlotNumber(rest);
			SaveGameInfo.SaveGameDescription.clear();
			TakeSaveSnapshot();
		}
		return true;
	}
	if (command == "deletegame")
	{
		// Its DELETEGAME, which the Load and Save Game screens delete a save
		// with, read alike (0x1000ee8a-0x1000ee9a): DELETEGAME -1, the quick
		// save's row's, and +71 do nothing.
		if (!rest.empty() && rest[0] >= '0' && rest[0] <= '9')
			DeleteGame(SlotNumber(rest));
		return true;
	}
	bool start = false;
	if (ParseWord(line, "open", rest) || (start = ParseWord(line, "start", rest)))
	{
		// The rest of the line becomes the next travel, as the original's
		// UGameEngine::Exec sets the viewport's (Engine.dll 0x1038a030;
		// engine-dll.md, OPEN and START): past the word and the blanks
		// after it, whole -- the Join screen's address typed with a blank
		// after it, "open 1.2.3.4 ?lan", is the URL's to read --, and
		// nothing after the word no travel. OPEN partial, on the last URL's
		// options (a listen server's keeps ?listen and ?game=), START
		// absolute -- a map, a server's address or a slot's ?loadgame=N
		// alike. A map that is not there is the browse's to find, as the
		// original's. A dedicated server browses (ServerBrowse).
		ETravelType type = start ? ETravelType::TRAVEL_Absolute : ETravelType::TRAVEL_Partial;
		std::string error;
		if (!LaunchInfo.dedicatedServer)
			ClientTravel(rest, type, false);
		else if (!ServerBrowse(rest, type, error))
			out.Log((start ? "Start failed: " : "Open failed: ") + error);
		return true;
	}
	if (ParseWord(line, "servertravel", rest))
	{
		// The level's own event, whatever the rest, Deus Ex's engine being a
		// server always (DeusEx.exe 0x10908d79): its game tells the clients,
		// and the server follows once NextSwitchCountdown runs out
		if (LevelInfo)
			CallEvent(LevelInfo, "ServerTravel", { ExpressionValue::StringValue(rest), ExpressionValue::BoolValue(false) });
		return true;
	}
	if (LaunchInfo.dedicatedServer && ParseWord(line, "say", rest))
	{
		// A server with no client says the rest of the line to every player
		// and messaging spectator, with a beep, through the level's
		// BroadcastMessage (0x1038a374-0x1038a392; bugs.md WIN.22)
		if (LevelInfo)
			CallEvent(LevelInfo, "BroadcastMessage", { ExpressionValue::StringValue(rest), ExpressionValue::BoolValue(true), ExpressionValue::NameValue(NameString("None")) });
		return true;
	}
	if (ParseWord(line, "disconnect", rest))
	{
		// Back to the menu's map, the server told as the level goes (Deus
		// Ex's own goes to dx.dx).
		std::string error;
		if (!LaunchInfo.dedicatedServer)
			ClientTravel("dx.dx", ETravelType::TRAVEL_Absolute, false);
		else if (!ServerBrowse("dx.dx", ETravelType::TRAVEL_Absolute, error))
			out.Log("Disconnect failed: " + error);
		return true;
	}
	if (ParseWord(line, "reconnect", rest))
	{
		// The last URL again: a client's is its server's.
		std::string error;
		if (!LaunchInfo.dedicatedServer)
		{
			if (LevelInfo)
				ClientTravel(LastURL.ToString(), ETravelType::TRAVEL_Absolute, false);
		}
		else if (!ServerBrowse(LastURL.ToString(), ETravelType::TRAVEL_Absolute, error))
		{
			out.Log("Reconnect failed: " + error);
		}
		return true;
	}
	if (ParseWord(line, "getcurrenttickrate", rest))
	{
		// The frames a second the main loop last counted (0x1038a623)
		out.Log(Printf("%f", CurrentTickRate));
		return true;
	}
	if (ParseWord(line, "getmaxtickrate", rest))
	{
		// The rate the main loop holds itself to (0x1038a63a)
		out.Log(Printf("%f", GetMaxTickRate()));
		return true;
	}
	if (ParseWord(line, "gspylite", rest))
	{
		// GameSpy Lite started (appLaunchURL), which the game does not ship:
		// logged as the original logs a URL it hands to the system, opened
		// nowhere, as DXEngine's other launches
		LogMessage("LaunchURL GSpyLite.exe ");
		return true;
	}
	if (ParseWord(line, "cancel", rest))
	{
		// A join given up (the progress window's F10).
		if (PendingLevel)
			SetProgress(LocalizeMessage("Engine", "Progress", "CancelledConnect", "Cancelled Connect Attempt"), "", 2.0f);
		else
			SetProgress("", "", 0.0f);
		CancelPending();
		return true;
	}
	return false;
}

// A dedicated server's travel by a command: the URL read on the last one, as
// the original's game engine reads it with no viewport, and browsed with no
// PreTravel to the level's actors (UGameEngine::Exec, Engine.dll 0x1038a030);
// at the frame's travel, where no script runs, and not at once (bugs.md
// WIN.31). A server's address is refused, Engine.int's ServerOpen, as the
// original's browse refuses it with no client, the command's word's failure
// said (UGameEngine::Browse, 0x1038b811).
bool Engine::ServerBrowse(const std::string& text, ETravelType travelType, std::string& error)
{
	UnrealURL url(&LastURL, text, travelType);
	if (!url.Host.empty())
	{
		error = LocalizeMessage("Engine", "Errors", "ServerOpen", "Servers can't open network URLs");
		return false;
	}
	ClientTravelInfo.URL = url;
	ClientTravelInfo.TransferItems = false;
	return true;
}

// The engine's words (UEngine::Exec, Engine.dll 0x10380ee0), in its order: the
// system's, the objects', the memory cache's, the launcher's, the client's,
// the renderer's, the audio's, FLUSH and CRACKURL.
bool Engine::EngineWordExec(const std::string& line, ExecOutput& out)
{
	if (SystemExec(line, out))
		return true;
	if (ObjectExec(line, out))
		return true;
	std::string rest;
	if (ParseWord(line, "dumpcache", rest))
	{
		// The original's lists its memory cache's items, a line each with
		// its ID's byte, its size and whether it is locked, empty, stale or
		// fresh (FMemCache::Exec, Core.dll 0x10125500); DXEngine has none,
		// its textures and light maps the device's and the light system's,
		// and says so, as SHOWLOG of its log window, by choice
		LogMessage("dumpcache: DXEngine has no memory cache");
		return true;
	}
	if (LauncherExec(line, out))
		return true;
	if (!LaunchInfo.dedicatedServer && ParseWord(line, "brightness", rest))
	{
		// UClient::Exec's BRIGHTNESS (Engine.dll 0x10375966-0x10375b6a), the
		// client's: the live brightness a tenth up, 1 or more back to 0, the
		// render flushed, a precache allowed, the client's config saved with
		// it, and the player told "Brightness level <n>/10"
		float brightness = client->BrightnessInUse + 0.1f;
		if (brightness >= 1.0f)
			brightness = 0.0f;
		client->BrightnessInUse = brightness;
		client->Brightness = brightness;
		FlushRender();
		SaveClientConfig();
		FlushConfig();
		if (viewport->Actor())
		{
			std::string message = "Brightness level " + std::to_string((int)(brightness * 10.0f + 1.0f)) + "/10";
			CallEvent(viewport->Actor(), "ClientMessage", { ExpressionValue::StringValue(message), ExpressionValue::NameValue(NameString("None")), ExpressionValue::BoolValue(false) });
		}
		return true;
	}
	if (render && RendererExec(line, out))
		return true;
	// The audio subsystem's, Galaxy's, after the renderer's (0x10380fb3-
	// 0x10380fc9): none with no client, nor with no audio subsystem
	if (!LaunchInfo.dedicatedServer && !NoAudioSubsystem && audiodev && audiodev->Exec(line, out))
		return true;
	if (ParseWord(line, "flush", rest))
	{
		// Taken with no client too, flushing nothing
		if (render)
			FlushRender();
		return true;
	}
	if (ParseWord(line, "crackurl", rest))
	{
		// The rest of the line read as an absolute URL, its parts listed; one
		// that does not read, BAD URL (0x10380ffa). The original faults on
		// its String line for any URL that reads (0x1038111d-0x10381122),
		// which DXEngine gives (bugs.md WIN.31).
		UnrealURL url(nullptr, rest, ETravelType::TRAVEL_Absolute);
		if (url.Valid)
		{
			out.Log("     Protocol: " + url.Protocol);
			out.Log("         Host: " + url.Host);
			out.Log("         Port: " + std::to_string(url.Port));
			out.Log("          Map: " + url.Map);
			out.Log("   NumOptions: " + std::to_string((int)url.Options.size()));
			for (size_t i = 0; i < url.Options.size(); i++)
				out.Log("     Option " + std::to_string((int)i) + ": " + url.Options[i]);
			out.Log("       Portal: " + url.Portal);
			out.Log("       String: '" + url.ToString() + "'");
		}
		else
		{
			out.Log("BAD URL");
		}
		return true;
	}
	return false;
}

// The system's words (USystem::Exec, Core.dll 0x1016b790): MEMSTAT,
// CONFIGHASH, EXIT and QUIT, RELAUNCH. Its DEBUG CRASH, GPF, RECURSE and
// EATMEM, which end the game on purpose, DXEngine takes none of.
bool Engine::SystemExec(const std::string& line, ExecOutput& out)
{
	std::string rest;
	if (ParseWord(line, "memstat", rest))
	{
		// The memory available, as a 32-bit process of the original's sees it
		// (GlobalMemoryStatus): the physical, the commit limit's and the
		// address space's left, in K, each at most 4 GB, then the memory load
		uint64_t phys = 0, pagefile = 0, virt = 0;
		int load = 0;
#ifdef WIN32
		MEMORYSTATUSEX status = {};
		status.dwLength = sizeof(status);
		if (GlobalMemoryStatusEx(&status))
		{
			phys = status.ullAvailPhys;
			pagefile = status.ullAvailPageFile;
			virt = status.ullAvailVirtual;
			load = (int)status.dwMemoryLoad;
		}
#else
		uint64_t total = 0, available = 0, swapFree = 0;
		if (FILE* meminfo = std::fopen("/proc/meminfo", "r"))
		{
			char key[64];
			unsigned long long kb = 0;
			while (std::fscanf(meminfo, "%63s %llu kB\n", key, &kb) == 2)
			{
				if (!std::strcmp(key, "MemTotal:"))
					total = kb * 1024;
				else if (!std::strcmp(key, "MemAvailable:"))
					available = kb * 1024;
				else if (!std::strcmp(key, "SwapFree:"))
					swapFree = kb * 1024;
			}
			std::fclose(meminfo);
		}
		phys = available;
		pagefile = available + swapFree;
		virt = UINT64_MAX;
		load = total ? (int)(100 - available * 100 / total) : 0;
#endif
		auto kilobytes = [](uint64_t bytes) { return (int)(std::min<uint64_t>(bytes, 0xffffffffull) >> 10); };
		out.Log(Printf("Memory available: Phys=%iK Pagef=%iK Virt=%iK", kilobytes(phys), kilobytes(pagefile), kilobytes(virt)));
		out.Log(Printf("Memory load = %i%%", load));
		return true;
	}
	if (ParseWord(line, "confighash", rest))
	{
		// The config cache's dump, as the original's (0x1016b823-0x1016b82f,
		// the executable's FConfigCacheIni::Dump): "Files map:", the files
		// held and the map's hash slots -- 8, twice as many once a file is
		// added past twice the slots and 8 more -- and how many files each
		// slot holds by the appStrihash of the file's name: of DXEngine's,
		// its inis and .int files, by their paths
		Array<std::string> files = packages->GetConfigFilePaths();
		int slots = 8;
		for (size_t added = 1; added <= files.size(); added++)
		{
			if (slots * 2 + 8 < (int)added)
				slots *= 2;
		}
		Array<int> counts((size_t)slots, 0);
		for (const std::string& file : files)
			counts[StrTools::ue1_strihash(file) & (uint32_t)(slots - 1)]++;
		out.Log("Files map:");
		out.Log(Printf("TMapBase: %i items, %i hash slots", (int)files.size(), slots));
		for (int i = 0; i < slots; i++)
			out.Log(Printf("   Hash[%i] = %i", i, counts[i]));
		return true;
	}
	if (ParseWord(line, "exit", rest) || ParseWord(line, "quit", rest))
	{
		out.Log("Closing by request");
		quit = true;
		return true;
	}
	if (ParseWord(line, "relaunch", rest))
	{
		Relaunch(rest);
		return true;
	}
	return false;
}

// Whether a native number is taken, as the original's native table has its
// slots filled: below the first native's 0x70 by a token of the bytecode
// DXEngine's reader takes (Bytecode::ReadToken), 0x60 to 0x6F the extended
// natives', else by a native of DXEngine's of that number
static bool NativeNumberTaken(int index)
{
	if (index < (int)ExprToken::FirstNative)
		return !(index == 0x03 || index == 0x15 || index == 0x35 || index == 0x37 || (index >= 0x5A && index < (int)ExprToken::ExtendedNative));
	return (size_t)index < NativeFunctions::NativeByIndex.size() && NativeFunctions::NativeByIndex[index] != nullptr;
}

// An object of the class or a subclass, any package's, by its name, as the
// original's ParseObject finds one with ANY_PACKAGE (Core.dll 0x10101a96)
static UObject* FindObjectOfClass(UClass* cls, const std::string& name)
{
	std::string objectName = name.substr(name.find_last_of('.') == std::string::npos ? 0 : name.find_last_of('.') + 1);
	if (objectName.empty())
		return nullptr;
	// By its text, a name not there made by no lookup, as the original's
	// FNAME_Find
	bool classes = cls->Name == "Class";
	for (GCObject* gcObj : GC::GetObjects())
	{
		UObject* obj = dynamic_cast<UObject*>(gcObj);
		if (!obj || AnyFlags(obj->Flags, ObjectFlags::EliminateObject) || !StrTools::equals_ignore_case(obj->Name.ToString(), objectName))
			continue;
		if (classes && dynamic_cast<UClass*>(obj))
			return obj;
		for (UStruct* s = obj->Class; s; s = s->BaseStruct)
		{
			if (s == cls)
				return obj;
		}
	}
	return nullptr;
}

// A holder by its full name: a package's file and tables, no object of the
// game's, as its GCDescribe gives them
static std::string HolderName(GCObject* obj)
{
	UObject* uobj = dynamic_cast<UObject*>(obj);
	return uobj ? FullName(uobj) : obj->GCDescribe();
}

// Whether the object's outer is the package, as the original's GetOuter()
// is its UPackage: a top object of it, the package's own object aside
static bool OuterIsPackage(UObject* obj, Package* package)
{
	if (!obj || !package || obj->package != package || package->IsPackageObject(obj))
		return false;
	return !obj->Outer() || package->IsPackageObject(obj->Outer());
}

// A package loaded, by its name: none loaded for it
static Package* FindLoadedPackage(PackageManager* packages, const std::string& name)
{
	for (Package* package : packages->GetLoadedPackages())
	{
		if (StrTools::equals_ignore_case(package->GetPackageName().ToString(), name))
			return package;
	}
	return nullptr;
}

// An object's bytes as the original's OBJ LIST counts them, its serializer
// walking it with a counting archive (Core.dll 0x10153b04, FArchiveCountMem):
// its properties, then the arrays it holds, as used and as allocated -- of
// DXEngine's, a texture's mips, a sound's and a song's data, a struct's
// script
static void ObjectBytes(UObject* obj, size_t& num, size_t& max)
{
	num = max = obj->Class->StructSize;
	auto add = [&](const auto& data)
	{
		num += data.size();
		max += data.capacity();
	};
	if (UTexture* texture = dynamic_cast<UTexture*>(obj))
	{
		for (const Array<UnrealMipmap>* mips : { &texture->UsedMipmaps, &texture->UncompressedMipmaps, &texture->CompressedMipmaps })
		{
			for (const UnrealMipmap& mip : *mips)
				add(mip.Data);
		}
	}
	else if (USound* sound = dynamic_cast<USound*>(obj))
		add(sound->Data);
	else if (UMusic* music = dynamic_cast<UMusic*>(obj))
		add(music->Data);
	else if (UStruct* structure = dynamic_cast<UStruct*>(obj))
		add(structure->BytecodeRaw);
}

// The objects' words (UObject::StaticExec, Core.dll 0x10153110): MEM,
// DUMPNATIVES, GET, SET, OBJ's GARBAGE, MARK, MARKCHECK, REFS, HASH,
// CLASSES, DEPENDENCIES, LIST, VFHASH and LINKERS, and GTIME, the tables
// the original lists of its own -- its native table, the references its
// serializer walks, its name hash, its states' function hashes, its
// linkers -- DXEngine's counterparts of each.
bool Engine::ObjectExec(const std::string& line, ExecOutput& out)
{
	std::string rest;
	if (ParseWord(line, "mem", rest))
	{
		// The original's allocator dump, whose lines the game's build leaves
		// out: nothing logged (FMallocWindows::DumpAllocs)
		return true;
	}
	if (ParseWord(line, "dumpnatives", rest))
	{
		// Each of the 4,096 native numbers nothing takes, to the log, as the
		// original's lists its native table's undefined slots
		// (0x10153179-0x101531ab)
		for (int i = 0; i < 4096; i++)
		{
			if (!NativeNumberTaken(i))
				LogMessage(Printf("Native index %i is available", i));
		}
		return true;
	}
	bool set = false;
	if (ParseWord(line, "get", rest) || (set = ParseWord(line, "set", rest)))
	{
		// The class by its name in any package (an ini: name its ini's
		// value), then the property: either not found answered as the
		// original's GET and SET answer it, the command taken all the same
		// (0x101531b0; bugs.md ENG.32, WIN.31)
		Array<std::string> args = GetArgs(rest);
		std::string classToken = args.size() > 0 ? args[0] : std::string();
		std::string propertyToken = args.size() > 1 ? args[1] : std::string();
		NameString className;
		UClass* cls = nullptr;
		try
		{
			if (!classToken.empty())
			{
				className = ParseClassName(classToken);
				cls = packages->FindClassAnyPackage(className);
			}
		}
		catch (const std::exception&)
		{
			cls = nullptr;
		}
		// Deus Ex's input class is Extension's InputExt, so the multiplayer
		// key SETs land here too instead of failing on every map
		// (extension-dll.md, the engine and the input).
		if (set && !propertyToken.empty() && (className == "input" || className == "inputext"))
		{
			std::string value = ArgsFrom(rest, 2);
			keybindings[propertyToken] = value;
			packages->SetIniValue("user", "Engine.Input", propertyToken, value);
			FlushConfig();
			return true;
		}
		if (!cls)
		{
			out.Log("Unrecognized class " + classToken);
			return true;
		}
		NameString propertyName = propertyToken;
		if (!set)
		{
			if (IsClassNamed(className, renderdev->Class))
				out.Log(renderdev->GetPropertyAsString(propertyName));
			else if (IsClassNamed(className, audiodev->Class))
				out.Log(audiodev->GetPropertyAsString(propertyName));
			else if (IsClassNamed(className, netdev->Class))
				out.Log(netdev->GetPropertyAsString(propertyName));
			else if (IsClassNamed(className, client->Class))
				out.Log(client->GetPropertyAsString(propertyName));
			else if (propertyToken.empty() || !cls->FindProperty(propertyName))
				out.Log("Unrecognized property " + propertyToken);
			else
				out.Log(ExportForGet(cls, propertyName));
			return true;
		}

		// The value is the rest of the line, as the original's SET takes it
		// (Core.dll's UObject::StaticExec, 0x101531b0): a server's name has
		// spaces.
		std::string value = ArgsFrom(rest, 2);

		// A name the original's class has not is answered as its SET answers
		// it (UObject's StaticExec, Core.dll 0x101531b0), nothing set or
		// saved; the class's config is saved whole, its values as the types
		// write them, as GlobalSetProperty's SaveConfig saves it (bugs.md
		// ENG.32)
		if (IsClassNamed(className, renderdev->Class))
		{
			if (!renderdev->HasProperty(propertyName))
			{
				out.Log("Unrecognized property " + propertyToken);
				return true;
			}
			renderdev->SetPropertyFromString(propertyName, value);
			renderdev->SaveConfig();
		}
		else if (IsClassNamed(className, audiodev->Class))
		{
			if (!audiodev->HasProperty(propertyName))
			{
				out.Log("Unrecognized property " + propertyToken);
				return true;
			}
			audiodev->SetPropertyFromString(propertyName, value);
			audiodev->SaveConfig();
		}
		else if (IsClassNamed(className, netdev->Class))
		{
			netdev->SetPropertyFromString(propertyName, value);
		}
		else if (IsClassNamed(className, client->Class))
		{
			if (!client->HasProperty(propertyName))
			{
				out.Log("Unrecognized property " + propertyToken);
				return true;
			}
			client->SetPropertyFromString(propertyName, value);
			client->PostEditChange();
			SaveClientConfig(false);
		}
		else
		{
			// As the original's GlobalSetProperty (Core.dll 0x101523a0):
			// the property the class's field walk finds, its own first,
			// set in every object of the class or a subclass, each then
			// told of it (PostEditChange), then in the class's defaults,
			// and the class's config is saved. The devices and the
			// client keep their values in members of their own, set by
			// name.
			UProperty* prop = propertyToken.empty() ? nullptr : cls->FindProperty(propertyName);
			if (!prop)
			{
				out.Log("Unrecognized property " + propertyToken);
				return true;
			}
			try
			{
				for (GCObject* gcObj : GC::GetObjects())
				{
					UObject* obj = dynamic_cast<UObject*>(gcObj);
					if (!obj || obj == cls || AnyFlags(obj->Flags, ObjectFlags::EliminateObject) || obj->IsDefaultsObject())
						continue;
					for (UStruct* objClass = obj->Class; objClass; objClass = objClass->BaseStruct)
					{
						if (objClass == cls)
						{
							if (obj == renderdev || obj == audiodev || obj == netdev || obj == client)
								obj->SetPropertyFromString(propertyName, value);
							else
								prop->SetValueFromString(obj->PropertyData.Ptr(prop), value);
							obj->PostEditChange();
							break;
						}
					}
				}
				prop->SetValueFromString(cls->PropertyData.Ptr(prop), value);
				cls->SaveConfig();
				FlushConfig();
			}
			catch (const std::exception&)
			{
				LogMessage("Could not set property '" + propertyName.ToString() + "': " + line);
			}
			return true;
		}
		// A device's or the client's setting, its value in the ini: saved at
		// once, as the original's SET ends with its class's SaveConfig
		// (GlobalSetProperty, Core.dll 0x101523a0; bugs.md ENG.27)
		FlushConfig();
		return true;
	}
	if (ParseWord(line, "obj", rest))
	{
		std::string sub;
		if (ParseWord(rest, "garbage", sub))
		{
			// The original's collects at once (0x101533d2); DXEngine's at
			// this frame's end, its collector running with no script under
			// way, by choice
			RequestGarbageCollection("obj garbage");
			return true;
		}
		if (ParseWord(rest, "mark", sub))
		{
			// Every object marked, for MARKCHECK to leave out (0x1015341a)
			LogMessage("Marking objects");
			markedObjects.clear();
			for (GCObject* gcObj : GC::GetObjects())
			{
				if (UObject* obj = dynamic_cast<UObject*>(gcObj))
					markedObjects.insert(obj);
			}
			return true;
		}
		if (ParseWord(rest, "markcheck", sub))
		{
			// Each object made since, by its full name, to the log
			// (0x101534d5)
			LogMessage("Unmarked objects:");
			for (GCObject* gcObj : GC::GetObjects())
			{
				UObject* obj = dynamic_cast<UObject*>(gcObj);
				if (obj && !markedObjects.count(obj))
					LogMessage(FullName(obj));
			}
			return true;
		}
		if (ParseWord(rest, "refs", sub))
		{
			// What refers to an object and how it is reached, as the
			// original's (0x101535be-0x101537f0): the class by CLASS= and the
			// object by NAME=, any package's, else nothing. Each object whose
			// references -- the collector's walk of it, DXEngine's
			// counterpart of the original's serializer -- hold it; then the
			// shortest chain to it from a root, a walk out from the roots a
			// collection marks from along the references: the code, never
			// collected ("(native)"), or what the engine holds ("(root)")
			std::string className, objectName;
			UClass* cls = nullptr;
			try
			{
				if (StrTools::ue1_parse(sub, "CLASS=", className) && !className.empty())
					cls = packages->FindClassAnyPackage(NameString(className));
			}
			catch (const std::exception&)
			{
				cls = nullptr;
			}
			if (!cls)
				return true;
			UObject* target = StrTools::ue1_parse(sub, "NAME=", objectName) ? FindObjectOfClass(cls, objectName) : nullptr;
			if (!target)
				return true;
			out.Log("");
			out.Log("Referencers of " + FullName(target) + ":");
			for (GCObject* gcObj : GC::GetObjects())
			{
				bool refers = false;
				GC::ForEachReference(gcObj, [&](GCObject* ref) { refers = refers || ref == target; });
				if (refers)
					out.Log("   " + HolderName(gcObj));
			}
			out.Log("");
			out.Log("Shortest reachability from root to " + FullName(target) + ":");
			std::unordered_map<GCObject*, GCObject*> from;
			std::unordered_set<GCObject*> code;
			std::deque<GCObject*> queue;
			GC::ForEachRoot([this](GCMarker& marker) { MarkRoots(marker); }, [&](GCObject* root, bool isCode)
			{
				if (isCode)
					code.insert(root);
				if (from.emplace(root, nullptr).second)
					queue.push_back(root);
			});
			while (!queue.empty() && !from.count(target))
			{
				GCObject* obj = queue.front();
				queue.pop_front();
				GC::ForEachReference(obj, [&](GCObject* ref)
				{
					if (from.emplace(ref, obj).second)
						queue.push_back(ref);
				});
			}
			if (from.count(target))
			{
				Array<GCObject*> chain;
				for (GCObject* obj = target; obj; obj = from[obj])
					chain.push_back(obj);
				std::reverse(chain.begin(), chain.end());
				for (size_t i = 0; i < chain.size(); i++)
					out.Log("   " + HolderName(chain[i]) + (i ? "" : code.count(chain[i]) ? " (native)" : " (root)"));
			}
			else
			{
				out.Log("   (Object is not currently rooted)");
			}
			out.Log("");
			return true;
		}
		if (ParseWord(rest, "hash", sub))
		{
			// The name table's figures, as the original's FName::DisplayHash
			// gives them (0x1014e000): its names, and how many of its 4,096
			// buckets they fill by appStrihash -- DXEngine's names in the
			// original's table; the objects' hash, which the original counts
			// too, it gives not (0x1015381e-0x101538b4)
			int names = 0, bins = 0;
			NameString::HashFigures(4096, names, bins);
			out.Log(Printf("Hash: %i names, %i/%i hash bins", names, bins, 4096));
			return true;
		}
		if (ParseWord(rest, "classes", sub))
		{
			// The class tree from Object, each class under its parent two
			// blanks further in (0x101538b9)
			Array<UClass*> classes;
			UClass* root = nullptr;
			for (GCObject* gcObj : GC::GetObjects())
			{
				if (UClass* cls = dynamic_cast<UClass*>(gcObj))
				{
					classes.push_back(cls);
					if (!cls->BaseStruct && cls->Name == "Object")
						root = cls;
				}
			}
			std::function<void(UClass*, int)> list = [&](UClass* cls, int indent)
			{
				out.Log(std::string(indent, ' ') + cls->Name.ToString());
				for (UClass* child : classes)
				{
					if (child->BaseStruct == cls)
						list(child, indent + 2);
				}
			};
			if (root)
				list(root, 0);
			return true;
		}
		if (ParseWord(rest, "dependencies", sub))
		{
			// What a package's objects refer to outside it, as the original's
			// (0x10153958-0x10153afc): the package by PACKAGE=, one loaded,
			// else nothing; EXCLUDE0= to EXCLUDE15= packages left out. Each of
			// its top objects that refers -- the collector's walk of it -- to
			// an object whose outer is neither the package nor one left out,
			// by its full name with "references:", each such reference under
			// it. The original's names there the first reference where its
			// own object was meant (FArchiveShowReferences, 0x1015de70-
			// 0x1015de83), DXEngine its object, by choice
			std::string packageName;
			Package* package = StrTools::ue1_parse(sub, "PACKAGE=", packageName) ? FindLoadedPackage(packages.get(), packageName) : nullptr;
			if (!package)
				return true;
			Array<Package*> excluded;
			for (int i = 0; i < 16; i++)
			{
				std::string excludeName;
				if (StrTools::ue1_parse(sub, "EXCLUDE" + std::to_string(i) + "=", excludeName))
				{
					if (Package* exclude = FindLoadedPackage(packages.get(), excludeName))
						excluded.push_back(exclude);
				}
			}
			out.Log("Dependencies of " + package->GetPackageName().ToString() + ":");
			for (GCObject* gcObj : GC::GetObjects())
			{
				UObject* obj = dynamic_cast<UObject*>(gcObj);
				if (!OuterIsPackage(obj, package))
					continue;
				bool named = false;
				GC::ForEachReference(obj, [&](GCObject* refObj)
				{
					UObject* ref = dynamic_cast<UObject*>(refObj);
					if (!ref || OuterIsPackage(ref, package))
						return;
					for (Package* exclude : excluded)
					{
						if (OuterIsPackage(ref, exclude))
							return;
					}
					if (!named)
					{
						out.Log("   " + FullName(obj) + " references:");
						named = true;
					}
					out.Log("      " + FullName(ref));
				});
			}
			return true;
		}
		if (ParseWord(rest, "list", sub))
		{
			// The objects of a CLASS=, in a PACKAGE= or INSIDE= one, each by
			// its full name, then a line a class with its count, and the
			// total (0x10153b04), each object's bytes as used and as
			// allocated (ObjectBytes)
			UClass* filterClass = nullptr;
			std::string filterPackage, filterInside, value;
			if (StrTools::ue1_parse(sub, "CLASS=", value))
				filterClass = packages->FindClassAnyPackage(value);
			StrTools::ue1_parse(sub, "PACKAGE=", filterPackage);
			StrTools::ue1_parse(sub, "INSIDE=", filterInside);
			bool filtered = filterClass || !filterPackage.empty() || !filterInside.empty();
			struct ClassCount
			{
				UClass* Class = nullptr;
				int Count = 0;
				size_t Bytes = 0;
				size_t MaxBytes = 0;
			};
			struct Listed
			{
				std::string Name;
				size_t Bytes = 0;
				size_t MaxBytes = 0;
			};
			Array<ClassCount> counts;
			Array<Listed> listed;
			int total = 0;
			size_t totalBytes = 0, totalMaxBytes = 0;
			for (GCObject* gcObj : GC::GetObjects())
			{
				UObject* obj = dynamic_cast<UObject*>(gcObj);
				if (!obj || !obj->Class)
					continue;
				bool inClass = false;
				for (UStruct* s = obj->Class; s && filterClass; s = s->BaseStruct)
					inClass = inClass || s == filterClass;
				// PACKAGE= a package's own objects, INSIDE= all within it
				bool ofPackage = obj->package && (!obj->Outer() || obj->package->IsPackageObject(obj->Outer()));
				bool inPackage = !filterPackage.empty() && ofPackage && StrTools::equals_ignore_case(obj->package->GetPackageName().ToString(), filterPackage);
				bool inside = !filterInside.empty() && obj->package && StrTools::equals_ignore_case(obj->package->GetPackageName().ToString(), filterInside);
				if (filtered && !inClass && !inPackage && !inside)
					continue;
				size_t bytes = 0, maxBytes = 0;
				ObjectBytes(obj, bytes, maxBytes);
				if (filtered)
					listed.push_back({ FullName(obj), bytes, maxBytes });
				auto it = std::find_if(counts.begin(), counts.end(), [&](const ClassCount& c) { return c.Class == obj->Class; });
				if (it == counts.end())
				{
					counts.push_back({ obj->Class, 0, 0, 0 });
					it = counts.end() - 1;
				}
				it->Count++;
				it->Bytes += bytes;
				it->MaxBytes += maxBytes;
				total++;
				totalBytes += bytes;
				totalMaxBytes += maxBytes;
			}
			out.Log("Objects:");
			out.Log("");
			if (!listed.empty())
			{
				std::sort(listed.begin(), listed.end(), [](const Listed& a, const Listed& b) { return a.Bytes > b.Bytes; });
				out.Log(Printf("%60s % 10s % 10s", "Object", "NumBytes", "MaxBytes"));
				for (const Listed& item : listed)
					out.Log(Printf("%60s % 10i % 10i", item.Name.c_str(), (int)item.Bytes, (int)item.MaxBytes));
				out.Log("");
			}
			if (!counts.empty())
			{
				std::sort(counts.begin(), counts.end(), [](const ClassCount& a, const ClassCount& b) { return a.Bytes > b.Bytes; });
				out.Log(Printf(" %30s % 6s % 10s  % 10s ", "Class", "Count", "NumBytes", "MaxBytes"));
				for (const ClassCount& c : counts)
					out.Log(Printf(" %30s % 6i % 10iK % 10iK", c.Class->Name.ToString().c_str(), c.Count, (int)(c.Bytes >> 10), (int)(c.MaxBytes >> 10)));
				out.Log("");
			}
			out.Log(Printf("%i Objects (%.3fM / %.3fM)", total, totalBytes / 1048576.0, totalMaxBytes / 1048576.0));
			return true;
		}
		if (ParseWord(rest, "vfhash", sub))
		{
			// Each state's function hash, as the original's
			// (0x10153fd7-0x101540e2): "Class VfHashes:", then each state and
			// class by its name, and for each of the hash's 256 slots how many
			// of the functions and states it holds -- its own and those of the
			// states and classes above it -- fall in it by their name's number:
			// DXEngine's names'
			out.Log("Class VfHashes:");
			for (GCObject* gcObj : GC::GetObjects())
			{
				UState* state = dynamic_cast<UState*>(gcObj);
				if (!state)
					continue;
				int slots[256] = {};
				for (UStruct* s = state; s; s = s->BaseStruct)
				{
					for (UField* field = s->Children; field; field = field->Next)
					{
						if (dynamic_cast<UStruct*>(field))
							slots[field->Name.GetCompareIndex() & 255]++;
					}
				}
				out.Log(state->Name.ToString() + ":");
				for (int i = 0; i < 256; i++)
					out.Log(Printf("   %i: %i", i, slots[i]));
			}
			return true;
		}
		if (ParseWord(rest, "linkers", sub))
		{
			// Each package's tables, as the original's linkers'
			// (0x10154100-0x10154269): "Linkers:", then a line a package
			// DXEngine has loaded -- its file and its full name; its names,
			// at 4 bytes each and with their text's 2 bytes a character and 14
			// more; its imports at 28 bytes, its exports at 36, in K; its
			// generations; and its lazy arrays, none, DXEngine reading an
			// object's data as it loads it
			out.Log("Linkers:");
			for (Package* package : packages->GetLoadedPackages())
			{
				const auto& names = package->GetNameTable();
				int nameBytes = 0;
				for (const NameTableEntry& entry : names)
					nameBytes += (int)entry.Name.ToString().size() * 2 + 14;
				int nameCount = (int)names.size();
				int imports = package->GetImportCount();
				int exports = package->GetExportCount();
				out.Log(Printf("%s (%s): Names=%i (%iK/%iK) Imports=%i (%iK) Exports=%i (%iK) Gen=%i Lazy=%i",
					package->GetPackageFilePath().c_str(), ("Package " + package->GetPackageName().ToString()).c_str(),
					nameCount, nameCount * 4 / 1024, nameBytes / 1024, imports, imports * 28 / 1024, exports, exports * 36 / 1024,
					(int)package->GetGenerations().size(), 0));
			}
			return true;
		}
		return false;
	}
	if (ParseWord(line, "gtime", rest))
	{
		// The original's timing global, which nothing in the game sets
		// (Core.dll 0x10154288), to the log
		LogMessage("GTime = 0.000000");
		return true;
	}
	return false;
}

// The launcher's words, the executable's (DeusEx.exe's GLaunchExec,
// 0x10913650; launch-flow.md, console commands): SHOWLOG, HIDELOG and
// TAKEFOCUS; a dedicated server's PREFERENCES, its log window's Advanced
// Options. The log window and the Advanced Options are the original's
// alone (the log says so).
bool Engine::LauncherExec(const std::string& line, ExecOutput& out)
{
	std::string rest;
	if (ParseWord(line, "showlog", rest))
	{
		LogMessage("showlog: DXEngine has no log window");
		return true;
	}
	if (ParseWord(line, "takefocus", rest))
	{
		if (window)
			window->ActivateWindow();
		return true;
	}
	if (ParseWord(line, "hidelog", rest))
		return true;
	if (LaunchInfo.dedicatedServer && ParseWord(line, "preferences", rest))
	{
		LogMessage("preferences: DXEngine has no Advanced Options window");
		return true;
	}
	return false;
}

// The renderer's words (URender::Exec, Render.dll 0x10b147b0; bugs.md
// WIN.29): STAT toggles a panel a word -- Net, Fps, Global, Mesh, Actor,
// Filter, Reject, Span, Zone, Light, Occlusion, Game, Soft, Cache, PolyV,
// PolyC, Illum, Hardware, each looked for in turn after the one before, so
// "stat net fps" toggles both and "stat fps net" Fps alone --, and DXEngine's
// own Render panel (STAT RENDER, by choice); REND LEAK and REND T toggle
// options DXEngine draws nothing by; TLOD, MLOD, MLMODE and MLFIX set the
// mesh texture LOD distance, the shape LOD, its mode and its fixed factor,
// which the mesh detail takes (VisibleMesh), each answered.
bool Engine::RendererExec(const std::string& line, ExecOutput& out)
{
	std::string rest;
	if (ParseWord(line, "stat", rest))
	{
		static const char* const words[] = { "net", "fps", "global", "mesh", "actor", "filter", "reject", "span", "zone", "light", "occlusion", "game", "soft", "cache", "polyv", "polyc", "illum", "hardware" };
		std::string after;
		for (int i = 0; i < RenderSubsystem::StatCount; i++)
		{
			if (ParseWord(rest, words[i], after))
			{
				render->StatPanels[i] = !render->StatPanels[i];
				rest = after;
			}
		}
		if (ParseWord(rest, "render", after))
			render->ShowRenderStats = !render->ShowRenderStats;
		return true;
	}
	if (ParseWord(line, "rend", rest))
	{
		std::string after;
		if (ParseWord(rest, "leak", after))
			render->RendLeak = !render->RendLeak;
		else if (ParseWord(rest, "t", after))
			render->RendT = !render->RendT;
		else
			return false;
		out.Log("Rendering option recognized");
		return true;
	}
	if (ParseWord(line, "tlod", rest))
	{
		if (rest.empty())
		{
			out.Log(Printf("%f", render->MeshTextureLOD));
			return true;
		}
		float value = (float)std::atof(rest.c_str());
		if (value > 0.0f)
			render->MeshTextureLOD = value;
		out.Log(Printf("Global mesh texture LOD distance %f", render->MeshTextureLOD));
		return true;
	}
	if (ParseWord(line, "mlod", rest))
	{
		if (rest.empty())
		{
			out.Log(Printf("%f", render->ShapeLOD));
			return true;
		}
		float value = (float)std::atof(rest.c_str());
		if (value > 0.0f)
			render->ShapeLOD = value;
		out.Log(Printf("Global mesh shape LOD distance %f", render->ShapeLOD));
		return true;
	}
	if (ParseWord(line, "mlmode", rest))
	{
		if (rest.empty())
		{
			out.Log(Printf("%i", render->ShapeLODMode));
			return true;
		}
		int value = std::atoi(rest.c_str());
		if (value >= 0)
			render->ShapeLODMode = value;
		out.Log(Printf("Shape LOD draw mode %i", render->ShapeLODMode));
		return true;
	}
	if (ParseWord(line, "mlfix", rest))
	{
		if (rest.empty())
		{
			out.Log(Printf("%f", render->ShapeLODFix));
			return true;
		}
		float value = (float)std::atof(rest.c_str());
		if (value >= 0.0f)
			render->ShapeLODFix = value;
		out.Log(Printf("Shape LOD scaling fixed at %f", render->ShapeLODFix));
		return true;
	}
	return false;
}

Array<std::string> Engine::GetArgs(const std::string& commandline)
{
	Array<std::string> args;
	size_t i = 0;
	while (i < commandline.size())
	{
		size_t j = commandline.find_first_not_of(" \t", i);
		if (j == std::string::npos)
			break;
		i = j;
		j = commandline.find_first_of(" \t", i);
		if (j == std::string::npos)
			j = commandline.size();
		if (j > i)
			args.push_back(commandline.substr(i, j - i));
		i = j;
	}
	return args;
}

void Engine::LoadEngineSettings()
{
	// The rate a client asks of a server: the game's stock 2,600, or none,
	// is 20,000, by choice, set as a SET sets it -- the ini, the class's
	// defaults and the viewport -- for the engine and the scripts alike (a
	// mod's player sets its net speed from it once in): a net client runs
	// at most its rate / 64 frames a second, 40 at 2,600, as the original's,
	// and a server holds a client to its own MaxClientRate. Any other rate
	// the player set stays.
	std::string speed = packages->GetIniValue("system", "Engine.Player", "ConfiguredInternetSpeed");
	if (speed.empty() || speed == "2600")
		RunCommand("set Engine.Player ConfiguredInternetSpeed 20000", nullptr, true);

	if (packages->MissingDXESystemIni())
	{
		client->LoadProperties("WinDrv.WindowsClient");
		audiodev->LoadProperties("Galaxy.GalaxyAudioSubsystem");
		// Deus Ex's renderer settings are those of the device its ini names
		// (GameRenderDevice, the game's D3DDrv.D3DRenderDevice).
		renderdev->LoadProperties(packages->GetIniValue("system", "Engine.Engine", "GameRenderDevice", "D3DDrv.D3DRenderDevice"));
	}
	else
	{
		client->LoadProperties();
		audiodev->LoadProperties();
		renderdev->LoadProperties();
	}
	// The live client's brightness held to 0 to 1 as it starts, as the
	// original's UWindowsClient::Init calls PostEditChange (WinDrv.dll
	// 0x11102434; bugs.md ENG.30)
	client->PostEditChange();
	engineSettingsLoaded = true;
}

bool Engine::NameLess::operator()(const std::string& a, const std::string& b) const
{
	return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
		[](char x, char y) { return std::tolower((unsigned char)x) < std::tolower((unsigned char)y); });
}

void Engine::LoadKeybindings()
{
	for (int i = 0; i < 256; i++)
	{
		std::string keyname = keynames[i];
		keybindings[keyname] = packages->GetIniValue("user", "Engine.Input", keyname);
	}

	for (int i = 0; i < 40; i++)
	{
		std::string alias = packages->GetIniValue("user", "Engine.Input", "Aliases[" + std::to_string(i) + "]");

		// Total trash parsing, but it will do for the aliases I have! Feel free to improve it!
		std::string commandStart = "(Command=\"";
		std::string commandSplit = "\",Alias=";
		std::string commandEnd = ")";
		if (alias.size() > commandStart.size() + commandSplit.size() + commandEnd.size())
		{
			size_t pos = alias.find(commandSplit, commandStart.size());
			if (pos != std::string::npos)
			{
				size_t pos2 = alias.find(commandEnd, pos);
				if (pos2 != std::string::npos)
				{
					std::string aliasCommand = alias.substr(commandStart.size(), pos - commandStart.size());
					std::string aliasName = alias.substr(pos + commandSplit.size(), pos2 - pos - commandSplit.size());
					if (!aliasName.empty() && aliasName != "None")
						inputAliases[aliasName] = aliasCommand;
				}
			}
		}
	}
}

void Engine::UpdateInput(float timeElapsed)
{
	if (timeElapsed <= 0.0f)
		return;

	TickWindow();
	gamepad.Update(this, timeElapsed);
}

// The original's UInput::ReadInput with the player's tick, in that tick
// before PlayerInput (Engine.dll 0x10394580; engine-dll.md, a key's
// binding): every input float a script or the mouse left since the last read
// is scaled by 20 over the tick, as the original scales each -- a mouse's
// move its Axis added at its event (InputExec) --, then the keys held and the
// pads' sticks add theirs, already in those units (a held key's axis its
// speed x 20, the original's speed x the tick x 20 over the tick). The
// buttons are set and cleared by their keys' press and release alone
// (InputExec); each held key's line runs again as held, its input words
// alone, as the original's read runs it, its axes counted here.
void Engine::ReadInput(UPlayerPawn* player, float elapsed)
{
	if (elapsed <= 0.0f)
		return;
	player->ScaleInput(20.0f / elapsed);
	for (EInputKey key : std::set<EInputKey>(heldInputKeys))
		ExecInputLine(keybindings[keynames[key]], key, EInputType::IST_Hold, 0);
	std::map<std::string, float> axes;
	for (auto& it : activeInputAxes)
		axes[it.first.first] += it.second;
	for (auto& [axis, value] : axes)
	{
		if (UProperty* prop = InputProperty(player, axis, true))
			*static_cast<float*>(player->GetProperty(prop)) += value;
	}
}

RenderDeviceType Engine::ChooseRenderDevice()
{
	if (OriginalCommandLine::Get().Param("nohard"))
	{
		LogMessage("-nohard: the software device");
		return RenderDeviceType::Software;
	}
	const auto& settings = LauncherSettings::Get().RenderDevice;
	if (settings.TypeNamed)
		return settings.Type;
	std::string device = packages->GetIniValue("system", "Engine.Engine", "GameRenderDevice");
	if (StrTools::equals_ignore_case(device, "SoftDrv.SoftwareRenderDevice"))
	{
		LogMessage("GameRenderDevice=SoftDrv.SoftwareRenderDevice: the software device");
		return RenderDeviceType::Software;
	}
	return RenderDeviceType::Vulkan;
}

std::unique_ptr<GameWindow> Engine::CreateGameWindow(RenderDeviceType type)
{
	if (type != RenderDeviceType::Software)
	{
		const char* name = type == RenderDeviceType::D3D11 ? "D3D11" : type == RenderDeviceType::OpenGL ? "GL" : type == RenderDeviceType::GLES ? "GL ES" : "Vulkan";
#ifndef WIN32
		// Mesa's CPU driver for GL, llvmpipe, chosen before GL is loaded
		if (LauncherSettings::Get().RenderDevice.CPUDriver && (type == RenderDeviceType::OpenGL || type == RenderDeviceType::GLES))
			setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
#endif
		try
		{
			return GameWindow::Create(this, type);
		}
		catch (const std::exception& e)
		{
			// The error's first line, without the stack Exception::Throw adds
			std::string error = e.what();
			error = error.substr(0, error.find('\n'));
			LogMessage(std::string("The ") + name + " device did not start (" + error + "): the software device draws in its place");
			chosenRenderDevice = RenderDeviceType::Software;
		}
	}
	// The software device in the original's WindowedRenderDevice's place --
	// with -nohard, or for a GPU device that did not start --, which draws
	// with its own class's settings, SoftDrv's in the game's ini: no
	// coronas, no shiny surfaces (WinDrv.dll 0x111079a3, 0x111079c2;
	// bugs.md ENG.31)
	if (type != RenderDeviceType::Software || OriginalCommandLine::Get().Param("nohard"))
	{
		renderdev->TakeWindowedRenderDevice();
		LogMessage("The software device with WindowedRenderDevice's settings, " + renderdev->WindowedRenderDeviceClass() + "'s");
	}
	return GameWindow::Create(this, RenderDeviceType::Software);
}

void Engine::OpenWindow()
{
	// No window system started (no display over ssh, no SDL video): said, as
	// any failure is, where the window's creation read through null
	if (!DisplayBackend::Get())
		Exception::Throw("No window system to open the game's window in: no display (DISPLAY, WAYLAND_DISPLAY) and no SDL video");

	if (!window)
	{
		window = CreateGameWindow(chosenRenderDevice);
		// The software device in a GPU device's place: -noddraw's window
		ApplyNoDDraw();
		// Held to 640x480 as it is dragged, as the original's window is
		// (WM_GETMINMAXINFO, WinDrv.dll 0x1110a812): on Windows the whole
		// window, its frame too, as the original's; elsewhere, where the
		// desktop draws the frame, the window's own area
		window->SetWindowMinSize(640, 480);

		// The game's own icon (PEIcon.h); none if its executable gives none.
		auto icon = PEIcon::OfGame(LaunchInfo.gameRootFolder, LaunchInfo.gameExecutableName);
		if (!icon.empty())
			window->SetWindowIcon(icon);
	}

	int width = client->StartupFullscreen ? client->FullscreenViewportX : client->WindowedViewportX;
	int height = client->StartupFullscreen ? client->FullscreenViewportY : client->WindowedViewportY;
	bool fullscreen = client->StartupFullscreen;
	// The window backend that started, named in the log: DXE_DISPLAY_BACKEND
	// asks for one, the first that starts is taken (ENGINE.md)
	DisplayBackend* backend = DisplayBackend::Get();
	std::string windowName = backend->IsSDL3() ? "SDL3" : backend->IsSDL2() ? "SDL2" : backend->IsWayland() ? "Wayland" : backend->IsX11() ? "X11" : backend->IsWin32() ? "Win32" : "unknown";

	std::string versionString = !LaunchInfo.gameVersionString.empty() ? " (v" + LaunchInfo.gameVersionString + ")" : "";

	window->SetWindowTitle(LaunchInfo.gameName + versionString + " - DXEngine");
	// The ini's sizes are pixels; the frame's are DIPs, which the window
	// scales by the desktop's scale (a 1280x720 window on a desktop at 150%
	// is 853x480 DIPs), as SETRES has it. Its size alone: the window where the
	// system puts a new one, as the original's (WinDrv's OpenWindow,
	// CW_USEDEFAULT), its title bar on the screen
	double dpi = window->GetDpiScale();
	window->SetFrameSize(Size(width / dpi, height / dpi));
	viewport->SetViewportRect(0, 0, width, height);
	if (!fullscreen)
		AskWindowSize(width, height);

	// Full screen takes the display's mode nearest FullscreenViewportX and Y,
	// as the original's; with none (0), the desktop's (ENGINE.md, running on
	// our devices). The frame takes the window's size.
	if (fullscreen)
	{
		if (width > 0 && height > 0)
		{
			Size mode = window->GetClosestResolution(Size(width, height));
			window->SetFullscreenMode((int)mode.width, (int)mode.height);
			LogMessage(std::string("Window: full screen, " + std::to_string((int)mode.width) + "x" + std::to_string((int)mode.height)) + " (" + windowName + " window)");
		}
		else
		{
			window->SetFullscreenMode(0, 0);
			LogMessage(std::string("Window: full screen, the desktop's size") + " (" + windowName + " window)");
		}
		window->ShowFullscreen();
	}
	else
	{
		LogMessage(std::string("Window: " + std::to_string(width) + "x" + std::to_string(height) + ", in a window") + " (" + windowName + " window)");
		window->ShowNormal();
	}

	// The client's config saved as the original's viewport saves it at its
	// first resize, as its window opens (bugs.md ENG.30)
	SaveClientConfig();
	FlushConfig();
}

void Engine::CloseWindow()
{
	window.reset();
}

void Engine::TickWindow()
{
	if (window)
	{
		if (viewport->bShowWindowsMouse() && viewport->bWindowsMouseAvailable())
			window->UnlockCursor();
		else
			window->LockCursor();

		// Text typed only where the game takes it: its UI holding the keyboard
		// (every modal up grabs it -- the menus, conversations, computers and
		// keypads) or the console typing. In play the keyboard is the game's alone, so a
		// desktop's input method does not hold back held keys.
		if ((dxRootWindow && dxRootWindow->IsKeyboardGrabbed()) || (console && console->bTyping()))
			window->UnlockKeyboard();
		else
			window->LockKeyboard();
	}

	GameWindow::ProcessEvents();

	// The window's size kept the tick after it last changed (KeepWindowedSize)
	if (windowSettleTicks > 0 && --windowSettleTicks == 0)
		KeepWindowedSize();

	if (MouseMoveX != 0 || MouseMoveY != 0)
	{
		int dx = MouseMoveX;
		int dy = MouseMoveY;
		MouseMoveX = 0;
		MouseMoveY = 0;

		// The tick's move, an axis event for each axis it moved on, none
		// for no move, as the original's window sends its batch of moves
		// (WinDrv.dll 0x1110afe4-0x1110b01e)
		if (dx)
			InputEvent(IK_MouseX, IST_Axis, dx);
		if (dy)
			InputEvent(IK_MouseY, IST_Axis, -dy);
	}
}

void Engine::OnWindowPaint()
{
}

void Engine::OnWindowMouseMove(const Point& pos)
{
	if (engine->dxRootWindow && engine->dxRootWindow->OnWindowMouseMove(pos))
		return;

	float scale = (float)window->GetDpiScale() * window->GetRenderDevice()->GetRenderScale();
	viewport->WindowsMouseX() = pos.x * scale;
	viewport->WindowsMouseY() = pos.y * scale;
}

void Engine::OnWindowMouseDown(const Point& pos, EInputKey key)
{
	if (engine->dxRootWindow && engine->dxRootWindow->OnWindowMouseDown(pos, key))
	{
		ReleaseFireButtons();
		return;
	}

	InputEvent(key, IST_Press);
}

void Engine::OnWindowMouseDoubleclick(const Point& pos, EInputKey key)
{
	if (engine->dxRootWindow && engine->dxRootWindow->OnWindowMouseDoubleclick(pos, key))
	{
		ReleaseFireButtons();
		return;
	}

	// Double click event sequence is: mouse down, up, doubleclick, up
	// Since we don't have double click events in the UE1 event system, the second click
	// should be treated as a normal mouse down event.
	InputEvent(key, IST_Press);
}

void Engine::OnWindowMouseUp(const Point& pos, EInputKey key)
{
	if (engine->dxRootWindow && engine->dxRootWindow->OnWindowMouseUp(pos, key))
	{
		ReleaseFireButtons();
		return;
	}

	InputEvent(key, IST_Release);
}

void Engine::OnWindowMouseWheel(const Point& pos, EInputKey key)
{
	if (engine->dxRootWindow && engine->dxRootWindow->OnWindowMouseWheel(pos, key))
		return;

	// The wheel's delta to the MouseW axis, a notch's 120 (Windows'
	// WHEEL_DELTA), then its press and release, as the original's window
	// sends a WM_MOUSEWHEEL (WinDrv.dll 0x1110b2bd; bugs.md ENG.38)
	InputEvent(IK_MouseW, IST_Axis, key == IK_MouseWheelUp ? 120 : -120);
	InputEvent(key, IST_Press);
	InputEvent(key, IST_Release);
}

void Engine::OnWindowRawMouseMove(int dx, int dy)
{
	if (engine->dxRootWindow && engine->dxRootWindow->OnWindowRawMouseMove(dx, dy))
		return;

	MouseMoveX += dx;
	MouseMoveY += dy;
}

// Typed text comes as UTF-8; it goes on a 16-bit character at a time, as
// Windows sends the original's WM_CHAR (WinDrv.dll UWindowsViewport::
// ViewportWndProc 0x11109b30, to the engine's Key, a carriage return passed
// over): one past 16 bits as its surrogate pair, two. The console takes each
// first, its KeyType given the character's low byte (UEngine::Key, Engine.dll
// 0x103813d0); one it leaves goes to the root window whole, to the window
// with the keyboard's focus (XGameEngineExt::Key, Extension.dll 0x10026650).
void Engine::OnWindowKeyChar(std::string chars)
{
	if (Timeline::LogKeys)
		LogMessage("DXKEY: char " + chars);
	for (char16_t c : UTF8ToScript(chars))
	{
		if (c == '\r')
			continue;
		if (Key(c))
			continue;
		if (engine->dxRootWindow)
			engine->dxRootWindow->OnWindowKeyChar(ScriptString(1, c));
	}
}

// Logged as the original's toggle logs it (WinDrv.dll's ToggleFullscreen
// 0x1110d500): EndFullscreen as full screen ends (0x1110d380), else
// AttemptFullscreen.
void Engine::ToggleFullscreen()
{
	bool isFullscreen = window->IsFullscreen();
	LogMessage(isFullscreen ? "EndFullscreen" : "AttemptFullscreen");

	// Get the resolution to SWITCH TO
	int width = isFullscreen ? client->WindowedViewportX : client->FullscreenViewportX;
	int height = isFullscreen ? client->WindowedViewportY : client->FullscreenViewportY;

	Size resolution;
	resolution.width = width;
	resolution.height = height;

	window->ToggleWindowFullscreen(resolution);
	viewport->SetViewportRect(0, 0, width, height);
	ModeSetAgain();
	if (isFullscreen)
		AskWindowSize(width, height);
	// The client's config saved as each resize of the original's viewport
	// saves it (ResizeViewport, WinDrv.dll 0x1110e2f3-0x1110e35b), the
	// config cache flushed (bugs.md ENG.27)
	SaveClientConfig();
	FlushConfig();
}

// The console's ENDFULLSCREEN, which the original's window takes in any
// state, after the exec functions and the engine's words
// (UWindowsViewport::Exec, WinDrv.dll 0x11106dcf-0x11106dea): its
// EndFullscreen (0x1110d380) logs EndFullscreen and sizes the viewport in a
// window at the client's windowed size (bugs.md WIN.17). Full screen, that is
// the toggle's way back, as its toggle calls it; in a window it is the
// window's own size, which the original's client keeps as the window is
// sized (WM_SIZE, 0x11109f82), so the window stays as it is.
void Engine::EndFullscreen()
{
	if (window->IsFullscreen())
	{
		ToggleFullscreen();
	}
	else
	{
		LogMessage("EndFullscreen");
		ModeSetAgain();
		SaveClientConfig();
		FlushConfig();
	}
}

void Engine::OnWindowKeyDown(EInputKey key)
{
	if (Timeline::LogKeys)
		LogMessage(std::string("DXKEY: ") + ((unsigned)key < 256 && keynames[key] ? keynames[key] : "?") + " down");
	// Enter with Alt held toggles full screen and presses nothing, as the
	// original's window takes it before the game sees a key (WinDrv.dll
	// 0x1110a28e: GetKeyState of Alt, then ToggleFullscreen)
	if (key == IK_Enter && window && window->GetKeyState(IK_Alt))
	{
		ToggleFullscreen();
		return;
	}
	if (engine->dxRootWindow && engine->dxRootWindow->OnWindowKeyDown(key))
	{
		ReleaseHeldInputKeys();
		return;
	}

	InputEvent(key, IST_Press);
}

void Engine::OnWindowKeyUp(EInputKey key)
{
	if (Timeline::LogKeys)
		LogMessage(std::string("DXKEY: ") + ((unsigned)key < 256 && keynames[key] ? keynames[key] : "?") + " up");
	if (engine->dxRootWindow && engine->dxRootWindow->OnWindowKeyUp(key))
	{
		ReleaseHeldInputKeys();
		return;
	}

	InputEvent(key, IST_Release);
}

void Engine::OnWindowGeometryChanged()
{
	windowSettleTicks = 2;
}

static double SteadySeconds()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void Engine::AskWindowSize(int width, int height)
{
	askedWindowWidth = width;
	askedWindowHeight = height;
	askedWindowTime = SteadySeconds();
}

// The original's WM_SIZE in a window (WinDrv.dll 0x1110a033) gives the
// render device the client area's new size, and the viewport's resize
// (ResizeViewport, 0x1110d640) makes the width even, one more where it is
// odd (0x1110d9a8), sizes the window to it (MoveWindow, 0x1110e29f), keeps
// the size as the client's WindowedViewportX and Y and the desktop's depth
// as its WindowedColorBits, and saves the client's config, the config cache
// flushed (0x1110e2f3-0x1110e35b): the ini written at once, so the way back
// from full screen and the next start come at the size. A size of nothing,
// a window minimized, is not kept. Full screen the window keeps its mode's
// size, not this.
void Engine::KeepWindowedSize()
{
	if (!window || window->IsFullscreen())
		return;
	int width = window->GetNativePixelWidth();
	int height = window->GetNativePixelHeight();
	if (width <= 0 || height <= 0)
		return;
	if (askedWindowWidth > 0)
	{
		if ((width != askedWindowWidth || height != askedWindowHeight) && SteadySeconds() - askedWindowTime < 1.0)
			return;
		askedWindowWidth = 0;
		askedWindowHeight = 0;
	}
	if (width & 1)
	{
		width++;
		double dpi = window->GetDpiScale();
		window->SetFrameSize(Size(width / dpi, height / dpi));
		AskWindowSize(width, height);
	}
	client->WindowedViewportX = width;
	client->WindowedViewportY = height;
	client->WindowedColorBits = 32;
	ModeSetAgain();
	SaveClientConfig();
	FlushConfig();
}

// The render device's mode set again -- full screen or back, SETRES, the
// window's new size --, as the original's D3DDrv's SetRes sets its texture
// stages again (InitTextureStageState, D3DDrv.dll 0x10009e20): a LODBIAS's
// bias gone, the launcher's again
void Engine::ModeSetAgain()
{
	if (render && render->Device)
		render->Device->SetLODBias(render->Device->ConfiguredLODBias);
}

// The original's UEngine::Flush(1) (Engine.dll 0x10380fed), the console's
// FLUSH and BRIGHTNESS's: the memory cache emptied, the light maps kept in
// it with it (0x10380d27), and the render device flushed, a precache
// allowed; no PRECACHING frame after it, as none comes after the original's
// (RenderWordsConsole with DXCAP_PRECACHE=1: the map's load alone shows it)
void Engine::FlushRender()
{
	if (Level)
		Level->Light.Flush();
	render->Device->Flush(true);
}

void Engine::OnWindowClose()
{
	quit = true;
}

void Engine::OnWindowActivated()
{
	//SetPause(false);
}

void Engine::OnWindowDeactivated()
{
	//SetPause(true);
	if (dxRootWindow)
		dxRootWindow->ForgetKeys();
}

void Engine::OnWindowDpiScaleChanged()
{
}

// Maximize asked of the game's window -- its title bar's button, a double
// click on the title bar, the window menu's -- toggles full screen as
// Alt+Enter does, as the original's window takes the command: SC_MAXIMIZE,
// its low four bits masked off, goes to ToggleFullscreen and no further, so
// the window is never maximized (WinDrv.dll 0x1110b149-0x1110b177)
void Engine::OnWindowMaximize()
{
	ToggleFullscreen();
}

// The screen saver and the display's power-down, which the game's window
// refuses in a window or full screen, logged as the original's window logs
// them (WinDrv.dll 0x1110b1d9, 0x1110b20a)
void Engine::OnWindowScreenSaver(bool displayPower)
{
	LogMessage(displayPower ? "Received SC_MONITORPOWER" : "Received SC_SCREENSAVE");
}

void Engine::LockCursor()
{
	if (window)
		window->LockCursor();
}

void Engine::UnlockCursor()
{
	if (window)
		window->UnlockCursor();
}

bool Engine::Key(char16_t key)
{
	if (Frame::Halted)
		return false;

	if (key == '~' && window->GetKeyState(IK_Shift))
	{
		// Did they REALLY hack UE1 to do something as lame as this??
		console->GotoState("Typing", {});
	}

	return CallEvent(console, EventName::KeyType, { ExpressionValue::ByteValue((uint8_t)key) }).ToBool();
}

// When the UI takes a key, every key the input holds down is released, so
// nothing stays held under a menu (extension-dll.md, the engine and the
// input). The original's XInputExt runs each held key's release binding; the
// buttons and axes tracked here are DXEngine's equivalent of those bindings.
void Engine::ReleaseHeldInputKeys()
{
	if (Frame::Halted || !viewport || !viewport->Actor())
		return;

	// Each held key's release binding, as the original's input runs it
	std::set<EInputKey> held;
	held.swap(heldInputKeys);
	for (EInputKey key : held)
		ExecInputLine(keybindings[keynames[key]], key, EInputType::IST_Release, 0);
	activeInputButtons.clear();

	// The axes are made at each read from the keys held, none now
	activeInputAxes.clear();
}

// When the UI takes a mouse button, the original's root window clears only
// the player's bFire and bAltFire (extension-dll.md, the root window).
void Engine::ReleaseFireButtons()
{
	if (Frame::Halted || !viewport || !viewport->Actor())
		return;

	for (auto it = activeInputButtons.begin(); it != activeInputButtons.end();)
	{
		if (NameString(it->first) == "bFire" || NameString(it->first) == "bAltFire")
		{
			SetInputButton(viewport->Actor(), it->first, false);
			it = activeInputButtons.erase(it);
		}
		else
		{
			++it;
		}
	}
}

// The mouse's axes, whose moves the original's window and DirectInput send
// (WinDrv.dll 0x1110afe4-0x1110b01e, 0x1110b2bd)
static bool IsMouseAxis(EInputKey key)
{
	return key == IK_MouseX || key == IK_MouseY || key == IK_MouseZ || key == IK_MouseW;
}

void Engine::InputEvent(EInputKey key, EInputType type, int delta)
{
	if (Frame::Halted)
		return;

	bool handled = CallEvent(console, EventName::KeyEvent, { ExpressionValue::ByteValue(key), ExpressionValue::ByteValue(type), ExpressionValue::FloatValue((float)delta) }).ToBool();
	if (handled || key < 0 || (int)key >= 256)
		return;

	// The key's binding run as the original's input runs it (XInputExt,
	// extension-dll.md, the engine and the input): a press once per hold, a
	// release only after its press
	if (type == EInputType::IST_Press)
	{
		if (!heldInputKeys.insert(key).second)
			return;
		ExecInputLine(keybindings[keynames[key]], key, type, 20);
	}
	else if (type == EInputType::IST_Axis)
	{
		// A mouse's move as it comes, which its Axis adds at once; a pad's
		// stick x 16, held until it moves again or is let go (InputExec)
		ExecInputLine(keybindings[keynames[key]], key, type, IsMouseAxis(key) ? delta : delta * 16);
	}
	else if (type == EInputType::IST_Release)
	{
		if (heldInputKeys.erase(key) != 0)
			ExecInputLine(keybindings[keynames[key]], key, type, 0);

		// The axes it added to -- a held key's, or a pad's stick, which a
		// release with no press before it centres -- go on with the keys
		// still held on them at the next read: W's walk again once S,
		// pressed over it, is let go
		for (auto it = activeInputAxes.begin(); it != activeInputAxes.end();)
		{
			if (it->first.second == key)
				it = activeInputAxes.erase(it);
			else
				++it;
		}
		for (auto it = activeInputButtons.begin(); it != activeInputButtons.end();)
		{
			if (it->second == key)
				it = activeInputButtons.erase(it);
			else
				++it;
		}
	}
}

// The original's ParseNext (Core.dll 0x10147b50) from pos: blanks, tabs, CRs
// and LFs passed, and a ';' comment with them to the line's end
static size_t ParseNext(const std::string& line, size_t pos)
{
	while (true)
	{
		while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\r' || line[pos] == '\n'))
			pos++;
		if (pos >= line.size() || line[pos] != ';')
			return pos;
		while (pos < line.size() && line[pos] != '\r' && line[pos] != '\n')
			pos++;
	}
}

namespace
{
	// An exec function's parameters in a block of its own layout, zeroed and
	// each made, then each destroyed, as the original's ScriptConsoleExec has
	// them (ParmsSize on the stack, zeroed): two bools in a row share a word
	// there, each its own bit
	struct ExecFunctionParms
	{
		ExecFunctionParms(UFunction* func) : Parms(Frame::CallParms(func)), Size(std::max(func->StructSize, (size_t)1))
		{
			Data = static_cast<uint8_t*>(AlignedAlloc(func->StructAlignment, Size));
			memset(Data, 0, Size);
			for (UProperty* prop : Parms)
				prop->ConstructArray(Data + prop->DataOffset.DataOffset);
		}
		~ExecFunctionParms()
		{
			for (UProperty* prop : Parms)
				prop->DestructArray(Data + prop->DataOffset.DataOffset);
			AlignedFree(Data);
		}
		const Array<UProperty*>& Parms;
		size_t Size;
		uint8_t* Data;
	};
}

// One object's exec functions, as the original's ScriptConsoleExec runs them
// (Core.dll 0x10150860; bugs.md ENG.17): the function of the line's first
// token's name, if it is an exec function. Its parameters as the original
// takes them: a first one that is an object the executor is, the executor;
// each other in turn, past blanks and a ';' comment (ParseNext), by its
// property's ImportText from where the last one stopped: a string the rest
// of the line, its position left where it was; a float to the next ',' or
// ')'; an object one character past its name. A refused import ends them:
// for an optional parameter the call is made, the rest zero; for any other
// Core's BadProperty is the command's output and no call is made. The
// command is taken either way. The callers ask each object in the
// original's order (ViewportExec's console, PlayerExec's game, HUD, player
// and inventory, EngineExec's game with no executor).
bool Engine::ScriptExec(UObject* target, UObject* executor, const std::string& line, ExecOutput& out)
{
	if (!target)
		return false;
	Array<std::string> args = GetArgs(line);
	if (args.empty())
		return false;
	UFunction* func = FindEventFunction(target, args[0]);
	if (!func || !AllFlags(func->FuncFlags, FunctionFlags::Exec))
		return false;

	// The parameters' text, past the command's word as ParseToken leaves it
	size_t start = line.find_first_not_of(" \t");
	if (start != std::string::npos)
		start = line.find_first_of(" \t", start);
	if (start == std::string::npos)
		start = line.size();

	ExecFunctionParms parms(func);
	size_t pos = start;
	bool refused = false;
	for (size_t i = 0; i < parms.Parms.size() && !AllFlags(parms.Parms[i]->PropFlags, PropertyFlags::ReturnParm); i++)
	{
		UProperty* prop = parms.Parms[i];
		void* data = parms.Data + prop->DataOffset.DataOffset;
		UObjectProperty* objprop = UObject::TryCast<UObjectProperty>(prop);
		// The executor for a first object parameter of its class, the
		// class itself or one it extends (UObject::ScriptConsoleExec,
		// Core.dll 0x10150860)
		if (i == 0 && executor && objprop && objprop->ObjectClass && executor->IsA(objprop->ObjectClass))
		{
			*static_cast<UObject**>(data) = executor;
			continue;
		}
		pos = ParseNext(line, pos);
		size_t next = prop->ImportText(line, pos, data);
		if (next == UProperty::ImportFailed)
		{
			if (!AllFlags(prop->PropFlags, PropertyFlags::OptionalParm))
			{
				out.Log(LocalizeMessage("Core", "Errors", "BadProperty", "'%s': Bad or missing property '%s'", { func->Name.ToString(), prop->Name.ToString() }));
				refused = true;
			}
			break;
		}
		pos = next;
	}
	if (!refused)
	{
		Array<ExpressionValue> vmArgs;
		for (UProperty* prop : parms.Parms)
		{
			if (AllFlags(prop->PropFlags, PropertyFlags::ReturnParm))
				break;
			ExpressionValue value = ExpressionValue::Variable(parms.Data, prop);
			value.Load();
			vmArgs.push_back(std::move(value));
		}
		CallEvent(target, func->Name, std::move(vmArgs));
	}
	return true;
}

// A Button binding's property on the pawn: an `input byte` (Pawn's bFire
// and the rest), or a bool; an Axis binding's a float. None, or another
// type, is the original's "Bad Button command" or "Bad Axis command"
// (UInput::Exec, Engine.dll 0x10393e10), logged and left alone.
static UProperty* InputProperty(UObject* pawn, const std::string& name, bool axis)
{
	if (!pawn || !pawn->HasProperty(name))
		return nullptr;
	UProperty* prop = pawn->GetMemberProperty(name);
	if (axis)
		return prop->ValueType == ExpressionValueType::ValueFloat ? prop : nullptr;
	return prop->ValueType == ExpressionValueType::ValueByte || prop->ValueType == ExpressionValueType::ValueBool ? prop : nullptr;
}

// A button's state into its property: a byte 1 or 0, as the original writes
// the byte, a bool set or cleared
static void SetInputButton(UObject* pawn, const std::string& name, bool pressed)
{
	UProperty* prop = InputProperty(pawn, name, false);
	if (!prop)
		return;
	if (prop->ValueType == ExpressionValueType::ValueByte)
		*static_cast<uint8_t*>(pawn->GetProperty(prop)) = pressed ? 1 : 0;
	else
		static_cast<UBoolProperty*>(prop)->SetBool(pawn->GetProperty(prop), pressed);
}

// The original's UInput::ExecInputCommands (Engine.dll 0x10393bf0), a key's
// binding (or an alias's command) line by line as ParseLine takes them into
// 256 characters (Core.dll 0x10147e60; StrTools::ue1_parse_line) -- a |
// outside quotes the next command, a // outside quotes the rest of its line
// dropped, a line past 255 characters its rest the next --, where each | cut
// one whatever the quotes: a press's to the viewport's commands -- the
// input's own first (InputExec), then the viewport's (RunCommand) --, as a
// release's that begins with OnRelease does, the rest of it, OnRelease as
// ParseCommand takes the word; anything else -- an axis, any other release
// -- to the input's own alone. OnRelease's line on the press, which no
// command takes, is passed over (bugs.md PKG.27).
void Engine::ExecInputLine(const std::string& line, EInputKey key, EInputType action, int value)
{
	size_t at = 0;
	std::string command;
	while (StrTools::ue1_parse_line(line, at, command, 256))
	{
		if (GetArgs(command).empty())
			continue;
		size_t pos = 0;
		bool onRelease = StrTools::ue1_parse_command(command, pos, "OnRelease");
		if (action == EInputType::IST_Press)
		{
			if (!onRelease && !InputExec(command, key, action, value))
				RunCommand(command);
		}
		else if (action == EInputType::IST_Release && onRelease)
		{
			std::string rest = command.substr(pos);
			if (!rest.empty() && !InputExec(rest, key, action, value))
				RunCommand(rest);
		}
		else
		{
			InputExec(command, key, action, value);
		}
	}
}

// A button's byte, or a bool's 1 or 0
static uint8_t InputButtonValue(UObject* pawn, UProperty* prop)
{
	if (prop->ValueType == ExpressionValueType::ValueByte)
		return *static_cast<uint8_t*>(pawn->GetProperty(prop));
	return static_cast<UBoolProperty*>(prop)->GetBool(pawn->GetProperty(prop)) ? 1 : 0;
}

static void SetInputButtonValue(UObject* pawn, UProperty* prop, uint8_t value)
{
	if (prop->ValueType == ExpressionValueType::ValueByte)
		*static_cast<uint8_t*>(pawn->GetProperty(prop)) = value;
	else
		static_cast<UBoolProperty*>(prop)->SetBool(pawn->GetProperty(prop), value != 0);
}

// The original's UInput::Exec (Engine.dll 0x10393e10), the input's own
// words: BUTTON sets its byte to 1 on a press and to 0 on a release, PULSE
// sets it on a press alone, TOGGLE flips its top bit on a press; AXIS adds at
// each read while its key is held, its speed x 20 (the original's hold adds
// the speed x the tick, which the read scales by 20 over the tick), a pad's
// stick the speed x its move x 16 at each read, and a mouse's move at once,
// the speed x the move x 0.01, as the original's adds an axis's, which the
// read scales by 20 over the tick: 1.2 a count for the stock MouseX=Axis
// aMouseX Speed=6.0, the original's a window pixel (bugs.md ENG.36); an
// alias's name runs the alias's line as the key's, with its action, an
// alias inside it not expanded again. False for any other word, which the
// viewport's commands take.
bool Engine::InputExec(const std::string& command, EInputKey key, EInputType action, int value, ExecOutput* out)
{
	if (GetArgs(command).empty())
		return false;
	// Its words as ParseCommand matches them (CommandWord), their arguments
	// from past the word: button"bFire" is BUTTON
	std::string rest;
	std::string word = CommandWord(command, rest);
	Array<std::string> args;
	args.push_back(word);
	for (const std::string& arg : GetArgs(rest))
		args.push_back(arg);
	UActor* actor = viewport->Actor();

	if (word == "button" || word == "pulse" || word == "toggle")
	{
		UProperty* prop = actor && args.size() >= 2 ? InputProperty(actor, args[1], false) : nullptr;
		if (!prop)
		{
			LogMessage(word == "toggle" ? "Bad Toggle command" : "Bad Button command");
			return true;
		}
		if (word == "button")
		{
			if (action == EInputType::IST_Press)
			{
				SetInputButtonValue(actor, prop, 1);
				if (key != IK_None)
					activeInputButtons[args[1]] = key;
			}
			else if (action == EInputType::IST_Release && InputButtonValue(actor, prop) != 0)
			{
				SetInputButtonValue(actor, prop, 0);
			}
		}
		else if (word == "pulse")
		{
			if (action == EInputType::IST_Press)
				SetInputButtonValue(actor, prop, 1);
		}
		else if (action == EInputType::IST_Press)
		{
			SetInputButtonValue(actor, prop, InputButtonValue(actor, prop) ^ 0x80);
		}
		return true;
	}

	if (word == "axis")
	{
		UProperty* prop = actor && args.size() >= 2 ? InputProperty(actor, args[1], true) : nullptr;
		if (!prop)
		{
			LogMessage("Bad Axis command");
			return true;
		}
		// The speed from a SPEED= word in any case, 1 without one, as the
		// original's parses it: the game's DefUser.ini and DXLauncher's pad
		// preset write speed=.
		float speed = 1.0f;
		for (size_t i = 2; i < args.size(); i++)
		{
			if (args[i].size() > 6 && StrTools::equals_ignore_case(args[i].substr(0, 6), "speed="))
				speed = (float)std::atof(args[i].substr(6).c_str());
		}
		if (action == EInputType::IST_Axis && IsMouseAxis(key))
			*static_cast<float*>(actor->GetProperty(prop)) += speed * value * 0.01f;
		else if (key != IK_None && (action == EInputType::IST_Press || action == EInputType::IST_Axis))
			activeInputAxes[{ args[1], key }] = speed * value;
		return true;
	}

	// A key's name by its number, the number past the word (appAtoi): ""
	// past the keys; and a key's binding by its name, in any case, the rest of
	// the line whole: nothing said for a name that is no key's (0x103940a0,
	// 0x103940eb). The input's alone: a remote player's are no one's (bugs.md
	// WIN.21).
	if (word == "keyname")
	{
		int index = std::atoi(rest.c_str());
		if (out)
			out->Log(index >= 0 && index < 256 ? keynames[index] : "");
		return true;
	}
	if (word == "keybinding")
	{
		for (int index = 0; index < 255; index++)
		{
			if (*keynames[index] && StrTools::equals_ignore_case(keynames[index], rest))
			{
				auto it = keybindings.find(keynames[index]);
				if (out)
					out->Log(it != keybindings.end() ? it->second : std::string());
				break;
			}
		}
		return true;
	}

	// An alias by its name, the line's first token as ParseToken takes it
	// (Engine.dll 0x10393e10's): a quoted name whole
	std::string aliasName;
	size_t aliasPos = 0;
	if (!inputAliasRunning && StrTools::ue1_parse_token(command, aliasPos, aliasName, 256))
	{
		auto it = inputAliases.find(aliasName);
		if (it != inputAliases.end())
		{
			inputAliasRunning = true;
			ExecInputLine(it->second, key, action, value);
			inputAliasRunning = false;
			return true;
		}
	}
	return false;
}

void Engine::SetPause(bool value)
{
	m_GamePaused = value;
}

void Engine::LogGamePackageSHA1Sums() const
{
	auto systemPath = packages->GetSystemFolderPath();

	LogMessage("SHA1Sums of some system files:");
	LogMessage("Core.u: " + SHA1Sum::of_file(systemPath / "Core.u"));
	LogMessage("Engine.u: " + SHA1Sum::of_file(systemPath / "Engine.u"));
}

void Engine::GetLevelInfoObject()
{
	LevelInfo = UObject::Cast<ULevelInfo>(LevelPackage->GetUObject("LevelInfo", "LevelInfo0"));
	if (!LevelInfo)
		Exception::Throw("Could not find the LevelInfo object for " + LevelPackage->GetPackageName().ToString() + "!");
}

void Engine::GetLevelObject()
{
	Level = UObject::Cast<ULevel>(LevelPackage->GetUObject("Level", "MyLevel"));
	if (!Level)
		Exception::Throw("Could not find the Level object for" + LevelPackage->GetPackageName().ToString() + "!");

	// Also try to find DeusExLevelInfo
	DeusExLevelInfo = UObject::Cast<UDeusExLevelInfo>(LevelPackage->GetUObject("DeusExLevelInfo", "DeusExLevelInfo0"));

	// Didn't find it. Keep searching.
	for (int grr = 1; !DeusExLevelInfo && grr < 20; grr++)
		DeusExLevelInfo = UObject::Cast<UDeusExLevelInfo>(LevelPackage->GetUObject("DeusExLevelInfo", "DeusExLevelInfo" + std::to_string(grr)));

	// Entry.dx does not have a DeusExLevelInfo
	/*
	if (!DeusExLevelInfo)
		Exception::Throw("Could not find the DeusExLevelInfo object for " + url.Map + "!");
	*/

	// The conversations' comment events stay: the script passes over
	// them, and 11 jumps land on labels only a comment carries. Each
	// event's item classes load when the conversation's events bind
	// (UConversation::BindEventsToActor), as the original's do.
}

void Engine::LinkActorsToLevel()
{
	// Link actors to the level
	for (UActor* actor : Level->Actors)
	{
		if (actor)
		{
			actor->XLevel() = Level;
			Level->Collision.AddToCollision(actor);
		}
	}

	// BasedActors (who is standing on me, so I carry them when I move) is native, runtime-only
	// state - never serialized - while ActorBase() (what am I standing on) is a normal property
	// that does round-trip through a package/save. Without this, an actor that starts a level (or
	// a load) already resting on a mover has its own ActorBase() pointer intact, but the mover's
	// own BasedActors list is empty, so the mover has no way to carry it along once it moves.
	for (UActor* actor : Level->Actors)
	{
		if (actor)
			actor->RelinkBasedActor();
	}
}

const char* Engine::keynames[256] =
{
	/*00*/ "None", "LeftMouse", "RightMouse", "Cancel",
	/*04*/ "MiddleMouse", "Unknown05", "Unknown06", "Unknown07",
	/*08*/ "Backspace", "Tab", "Unknown0A", "Unknown0B",
	/*0C*/ "Unknown0C", "Enter", "Unknown0E", "Unknown0F",
	/*10*/ "Shift", "Ctrl", "Alt", "Pause",
	/*14*/ "CapsLock", "Unknown15", "Unknown16", "Unknown17",
	/*18*/ "Unknown18", "Unknown19", "Unknown1A", "Escape",
	/*1C*/ "Unknown1C", "Unknown1D", "Unknown1E", "Unknown1F",
	/*20*/ "Space", "PageUp", "PageDown", "End",
	/*24*/ "Home", "Left", "Up", "Right",
	/*28*/ "Down", "Select", "Print", "Execute",
	/*2C*/ "PrintScrn", "Insert", "Delete", "Help",
	/*30*/ "0", "1", "2", "3",
	/*34*/ "4", "5", "6", "7",
	/*38*/ "8", "9", "Unknown3A", "Unknown3B",
	/*3C*/ "Unknown3C", "Unknown3D", "Unknown3E", "Unknown3F",
	/*40*/ "Unknown40", "A", "B", "C",
	/*44*/ "D", "E", "F", "G",
	/*48*/ "H", "I", "J", "K",
	/*4C*/ "L", "M", "N", "O",
	/*50*/ "P", "Q", "R", "S",
	/*54*/ "T", "U", "V", "W",
	/*58*/ "X", "Y", "Z", "Unknown5B",
	/*5C*/ "Unknown5C", "Unknown5D", "Unknown5E", "Unknown5F",
	/*60*/ "NumPad0", "NumPad1", "NumPad2", "NumPad3",
	/*64*/ "NumPad4", "NumPad5", "NumPad6", "NumPad7",
	/*68*/ "NumPad8", "NumPad9", "GreyStar", "GreyPlus",
	/*6C*/ "Separator", "GreyMinus", "NumPadPeriod", "GreySlash",
	/*70*/ "F1", "F2", "F3", "F4",
	/*74*/ "F5", "F6", "F7", "F8",
	/*78*/ "F9", "F10", "F11", "F12",
	/*7C*/ "F13", "F14", "F15", "F16",
	/*80*/ "F17", "F18", "F19", "F20",
	/*84*/ "F21", "F22", "F23", "F24",
	/*88*/ "Unknown88", "Unknown89", "Unknown8A", "Unknown8B",
	/*8C*/ "Unknown8C", "Unknown8D", "Unknown8E", "Unknown8F",
	/*90*/ "NumLock", "ScrollLock", "Unknown92", "Unknown93",
	/*94*/ "Unknown94", "Unknown95", "Unknown96", "Unknown97",
	/*98*/ "Unknown98", "Unknown99", "Unknown9A", "Unknown9B",
	/*9C*/ "Unknown9C", "Unknown9D", "Unknown9E", "Unknown9F",
	/*A0*/ "LShift", "RShift", "LControl", "RControl",
	/*A4*/ "UnknownA4", "UnknownA5", "UnknownA6", "UnknownA7",
	/*A8*/ "UnknownA8", "UnknownA9", "UnknownAA", "UnknownAB",
	/*AC*/ "UnknownAC", "UnknownAD", "UnknownAE", "UnknownAF",
	/*B0*/ "UnknownB0", "UnknownB1", "UnknownB2", "UnknownB3",
	/*B4*/ "UnknownB4", "UnknownB5", "UnknownB6", "UnknownB7",
	/*B8*/ "UnknownB8", "UnknownB9", "Semicolon", "Equals",
	/*BC*/ "Comma", "Minus", "Period", "Slash",
	/*C0*/ "Tilde", "UnknownC1", "UnknownC2", "UnknownC3",
	/*C4*/ "UnknownC4", "UnknownC5", "UnknownC6", "UnknownC7",
	/*C8*/ "Joy1", "Joy2", "Joy3", "Joy4",
	/*CC*/ "Joy5", "Joy6", "Joy7", "Joy8",
	/*D0*/ "Joy9", "Joy10", "Joy11", "Joy12",
	/*D4*/ "Joy13", "Joy14", "Joy15", "Joy16",
	/*D8*/ "UnknownD8", "UnknownD9", "UnknownDA", "LeftBracket",
	/*DC*/ "Backslash", "RightBracket", "SingleQuote", "UnknownDF",
	/*E0*/ "JoyX", "JoyY", "JoyZ", "JoyR",
	/*E4*/ "MouseX", "MouseY", "MouseZ", "MouseW",
	/*E8*/ "JoyU", "JoyV", "UnknownEA", "UnknownEB",
	/*EC*/ "MouseWheelUp", "MouseWheelDown", "Unknown10E", "Unknown10F",
	/*F0*/ "JoyPovUp", "JoyPovDown", "JoyPovLeft", "JoyPovRight",
	/*F4*/ "UnknownF4", "UnknownF5", "Attn", "CrSel",
	/*F8*/ "ExSel", "ErEof", "Play", "Zoom",
	/*FC*/ "NoName", "PA1", "OEMClear", ""
};
