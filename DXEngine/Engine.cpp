
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
#include "Package/ObjectStream.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/USubsystem.h"
#include "Packages/Core/Properties/UObjectProperty.h"
#include "Packages/Core/Properties/UStringProperty.h"
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
#include <chrono>
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
		std::string useSound = packages->GetIniValue("system", gameEngineClass, "UseSound", "True");
		bool soundOff = useSound == "0" || StrTools::equals_ignore_case(useSound, "False");
		if (OriginalCommandLine::Get().Param("nosound") || soundOff)
		{
			LogMessage(soundOff ? "UseSound off: no sound" : "-nosound: no sound");
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

	bool launchServer = !LaunchInfo.url.empty() && !UnrealURL(LaunchInfo.url).Host.empty() && !dedicated;
	// A save to start in, ?loadgame=N and no map: the slot loaded before any
	// map, as the original's first browse loads it (DeusExGameEngine's
	// Browse, dx-reverse-info's deusex-dll.md); the menu's map, as with no
	// URL, for a slot with no save.
	UnrealURL startURL(LaunchInfo.url);
	bool launchSave = !LaunchInfo.url.empty() && startURL.Host.empty() && startURL.Map.empty() &&
		startURL.HasOption("loadgame") && !dedicated;
	if (!launchSave || !LoadSavedGame(startURL))
	{
		if (LaunchInfo.url.empty() || launchServer || launchSave)
			LoadMap(GetDefaultURL(packages->GetIniValue("system", "URL", "LocalMap")));
		else
			LoadMap(UnrealURL(GetDefaultURL(packages->GetIniValue("system", "URL", "LocalMap")), LaunchInfo.url));

		if (!dedicated)
			LoginPlayer();
	}

	// A server's address to start with: joined from the menu's map, as the
	// original's first browse does.
	if (launchServer)
		ClientTravel(LaunchInfo.url, ETravelType::TRAVEL_Absolute, false);

	// The original's EXEC=, read into an FString's 4,096 characters
	// (DeusEx.exe WinMain 0x1090947a, Core.dll 0x10146e0e), run as the
	// command "exec <value>" on the client's viewport once the engine is up
	// (0x109097e1): the console's exec, its file the value's first token;
	// then the launcher's splash can go.
	std::string execFile;
	if (!dedicated && viewport->Actor() && OriginalCommandLine::Get().Value("EXEC", execFile, 4096))
		RunCommand(viewport->Actor(), "exec " + execFile);
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

		// A frame, as the original's game engine has it (dx-reverse-info
		// engine-dll.md, the game engine's frame): the windows' tick, the
		// levels' tick, a save asked for, a travel -- the frame ending
		// there --, a join under way, the draw.
		float realTimeElapsed = CalcTimeElapsed();

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
		// after, as the original's level tick has them.
		if (LevelNetDriver)
			LevelNetDriver->TickDispatch(levelElapsed);

		Level->Tick(levelElapsed, m_GamePaused);

		// A server replicates to its clients before the packets go; a client
		// its own player's pawn to the server.
		if (LevelNetDriver && !LevelNetDriver->ServerConnection && ServerLevel)
			ServerLevel->TickNetServer(levelElapsed);
		else if (LevelNetDriver && LevelNetDriver->ServerConnection && ClientLevel)
			ClientLevel->TickNetClient(*LevelNetDriver);
		if (LevelNetDriver)
			LevelNetDriver->TickFlush();

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

		Draw(realTimeElapsed, levelElapsed, PendingLevel.get());

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
	if (window)
		window->UnlockCursor();

	LogMessage("Saving configurations...");
	RestoreRunOnlySettings();
	if (packages->MissingDXESystemIni())
	{
		// Add the missing Subsystem entries
		client->SaveConfig();
		audiodev->SaveConfig();
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

void Engine::ExecFile(UObject* context, const std::string& filename)
{
	// As the original's EXEC: a relative file from the game's System folder
	// (the original's working directory, which nothing sets here), each line
	// a command.
	std::string text;
	try
	{
		fs::path file = filename;
		if (file.is_relative())
			file = packages->GetSystemFolderPath() / file;
		text = File::read_all_text(file.string());
	}
	catch (...)
	{
		LogMessage("Can't find file '" + filename + "'");
		return;
	}
	LogMessage("Execing " + filename);
	size_t start = 0;
	while (start < text.size())
	{
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();
		std::string line = text.substr(start, end - start);
		start = end + 1;
		while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
			line.pop_back();
		if (line.find_first_not_of(" \t") == std::string::npos)
			continue;
		RunCommand(context, line);
	}
}

// A failure (a SHOT whose file cannot be written) logged and the game going
// on, as a console line's is under its native (Frame::CallNative) and the
// original's SHOT carries on, writing nothing: it left Run through the
// window's events and ended the game. A runaway script still stops it.
void Engine::RunCommand(UObject* context, const std::string& command)
{
	uint32_t foundBits = 0;
	BitfieldBool found = { &foundBits, 1 };
	try
	{
		ConsoleCommand(context, command, found);
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

void Engine::Relaunch(const std::string& commandline)
{
	// The original's RELAUNCH (Core.dll's USystem::Exec) logs the rest of the
	// line, saves the config, starts DeusEx.exe with it and exits. The
	// Display screen's Rendering Device sends -changevideo, the launcher's
	// renderer page; the Host screen's dedicated server, its map URL with
	// -server in it. The inis are saved as the engine shuts down.
	size_t start = commandline.find_first_not_of(" \t");
	size_t rest = commandline.find_first_of(" \t", start);
	rest = commandline.find_first_not_of(" \t", rest == std::string::npos ? commandline.size() : rest);
	std::string line = rest == std::string::npos ? std::string() : commandline.substr(rest);
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
		RunCommand(viewport ? viewport->Actor() : nullptr, "open " + line.substr(5));
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
		ClientTravelInfo.TravelType = travelType;
		ClientTravelInfo.TransferItems = transferItems;
		SendPreTravel(Level);
		return;
	}

	UnrealURL url(newURL);

	// The seven keys the URL has are written to the ini, as the original's
	// LoadMap saves them (SaveURLConfig).
	const char* playerKeys[] = { "Name", "Class", "team", "skin", "Face", "Voice", "OverrideClass" };
	for (const char* optionKey : playerKeys)
	{
		if (url.HasOption(optionKey))
			engine->packages->SetIniValue("User", "DefaultPlayer", optionKey, url.GetOption(optionKey));
	}

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

	// The last URL's options but PUSH, POP, PEER and LOAD, as the original's
	// FURL(Base, URL, TRAVEL_Partial or TRAVEL_Relative) (Engine.dll
	// 0x103f7b60) copies them, the new URL's over them.
	auto carried = [&]()
	{
		Array<std::string> options;
		for (const std::string& option : LastURL.Options)
		{
			if (!StrTools::equals_ignore_case(option, "push") && !StrTools::equals_ignore_case(option, "pop") &&
				!StrTools::equals_ignore_case(option, "peer") && !StrTools::equals_ignore_case(option, "load"))
				options.push_back(option);
		}
		return options;
	};

	if (travelType == ETravelType::TRAVEL_Absolute)
	{
		// With the last URL's player options it lacks or has empty, after
		// its own, as the original's FURL(Base, URL, TRAVEL_Absolute) adds
		// them to a URL of the game's protocol and logs each: a hosted
		// game's start keeps the player's name and class.
		ClientTravelInfo.URL = url;
		if (url.Protocol == UnrealURL().Protocol)
		{
			const std::string playerOptions[] = { "Name", "Team", "Class", "Skin", "Face", "Voice", "OverrideClass" };
			for (const std::string& option : LastURL.Options)
			{
				for (const std::string& name : playerOptions)
				{
					if (option.size() > name.size() && option[name.size()] == '=' && StrTools::equals_ignore_case(option.substr(0, name.size()), name))
					{
						if (ClientTravelInfo.URL.GetOption(name).empty())
						{
							LogMessage("URL: Adding default option " + option);
							ClientTravelInfo.URL.Options.push_back(option);
						}
						break;
					}
				}
			}
		}
	}
	else if (travelType == ETravelType::TRAVEL_Partial)
	{
		// On the last URL's options (the player's name among them) alone,
		// none of its server, map or portal: "carry name, reset server"
		// (Engine.u), so a net client's 'open <map>' leaves the server for
		// the local map
		UnrealURL base;
		base.Options = carried();
		ClientTravelInfo.URL = UnrealURL(base, url.ToString());
	}
	else if (travelType == ETravelType::TRAVEL_Relative)
	{
		// On the last URL whole, its server, map and options -- a net
		// client's is its server's, so a server travelling sends its clients
		// its next map this way --, and no option added: DXEngine added the
		// game's ?Difficulty, which the original's never has.
		UnrealURL base = LastURL;
		base.Options = carried();
		ClientTravelInfo.URL = UnrealURL(base, url);
	}

	// A URL that names no map goes to the default one, [URL] Map, as the
	// original's starts from it, a relative one to the last URL's: a bare
	// ?loadgame opened from the menu browses Index.dx.
	if (url.Map.empty())
		ClientTravelInfo.URL.Map = travelType == ETravelType::TRAVEL_Relative ? LastURL.Map : packages->GetIniValue("system", "URL", "Map");

	ClientTravelInfo.TravelType = travelType;
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
	for (const std::string& optionKey : DefaultPlayerOptionKeys())
	{
		std::string value;
		if (packages->FindIniValue("user", "DefaultPlayer", optionKey, value))
			url.Options.push_back(optionKey + "=" + value);
	}
	return url;
}

Array<std::string> Engine::DefaultPlayerOptionKeys()
{
	Array<std::string> keys;
	for (const NameString& key : packages->GetIniKeysFromSection("user", "DefaultPlayer"))
		keys.push_back(key.ToString());
	return keys;
}

void Engine::LoadEntryMap()
{
	// The entry map is the map you see in the game when no other map is playing. For example when disconnected from a server. It is always loaded and running.
	// Loaded as the original's Init loads it, entry.dx with no option: its
	// game begins with none, the player's options coming with a return to it.
	LoadMap(UnrealURL("entry." + packages->GetMapExtension()));
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
	CloseNetDriver();

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

void Engine::LoadMap(const UnrealURL& url, const std::map<std::string, std::string>& travelInfo, bool asClient)
{
	ClientTravelInfo.URL.Clear();

	// A map's load counts its own jumps, as the original's (UGameEngine::LoadMap)
	Frame::InitRunaway();

	if (url.HasOption("entry") || url.HasOption("failed"))
	{
		ReturnToEntry(url);
		return;
	}

	// The loading screen. None at the start, with no level yet: the
	// original's Init loads its first map before it has a viewport.
	if (Level && !LaunchInfo.dedicatedServer)
		PaintLoading(asClient);

	if (Level)
		CallEvent(console, EventName::NotifyLevelChange);

	UnloadMap();

	// Load map objects

	bool loadedFromCurrent = false;
	LevelPackage = nullptr;
	if (dxTravelUsesCurrent)
	{
		dxTravelUsesCurrent = false;
		// Within a mission, a map saved into Current is revisited as the
		// player left it: the file SaveCurrentLevel named by the package,
		// found whatever case the URL spells the map in (a stock exit names
		// 12_vandenberg_cmd), with or without its .dx, as the original's
		// Browse cuts and Windows' file system finds it.
		const std::string currentMap = packages->FindCurrentSave(url.Map);
		if (!currentMap.empty())
		{
			LogMessage("Loading " + url.Map + " from Current");
			LevelPackage = packages->LoadSaveFile(currentMap);
			loadedFromCurrent = LevelPackage != nullptr;
		}
	}
	if (!LevelPackage)
	{
		// A client's map may be a download, in the cache under its GUID.
		std::string file = asClient ? packages->FindPackageFile(url.Map, {}) : std::string();
		LevelPackage = !file.empty() ? packages->LoadMapFile(url.Map, file) : packages->LoadMap(url.Map);
	}
	LevelPackage->SetIsLevel();

	// A level from Current is entered by its file, as the original's Browse
	// loads it (DeusEx.dll 0x10010b90, FURL(URL, file, TRAVEL_Relative)):
	// the map the save path's Current\<map>.<save extension>, the map named
	// cut at its first .dx -- ..\Save\Current\01_NYC_UNATCOIsland.dxs --,
	// with the URL's options but PUSH, POP, PEER and LOAD, and its portal.
	UnrealURL enteredURL = url;
	if (loadedFromCurrent)
	{
		std::string map = url.Map;
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

	GetLevelInfoObject();

	LevelInfo->ComputerName() = UTF8ToScript(ComputerName());
	LevelInfo->HubStackLevel() = 0; // To do: handle level hubs
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
	// the one entered by, which the game and the player log in with.
	LevelInfo->URL = HasLoadOption(enteredURL) ? LevelHeldURL() : enteredURL;
	LastURL = enteredURL;

	Level->TravelInfo = travelInfo; // Initially used travel info for level restart

	// Remove the actors meant for the editor (to do: should we do this at the package manager level?)
	for (UActor*& actor : Level->Actors)
	{
		if (actor && AllFlags(actor->Flags, ObjectFlags::NotForServer))
		{
			actor->bDeleteMe() = true;
			actor = nullptr;
		}
	}

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
		return;
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
}

void Engine::BeginPlay(const UnrealURL& url)
{
	if (!LevelInfo->bBegunPlay())
	{
		LevelInfo->TimeSeconds() = 0.0f;
		LevelInfo->bBegunPlay() = true;

		std::string options = url.GetOptions();

		auto stringProp = GC::Alloc<UStringProperty>("", nullptr, ObjectFlags::NoFlags);
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
	ClientTravelInfo.URL.Clear();

	if (url.HasOption("entry")) // Not sure what the purpose of this kind of travel is - do nothing for now.
		return false;

	Package* savefilePackage = nullptr;
	uint32_t slotNum = 0;
	std::string deusExMapName;

	if (url.HasOption("load"))
	{
		slotNum = (uint32_t)SlotNumber(url.GetOption("load"));
		savefilePackage = packages->LoadSaveSlot(slotNum);
	}

	if (!savefilePackage && url.HasOption("loadgame"))
	{
		// The original's Browse ?loadgame=N (dx-reverse-info/deusex-dll.md, travel
		// and saving): read the slot's SaveInfo, empty Current, copy the
		// slot into it, and load the info's map from Current. -1 is the
		// quick save.
		const int32_t slot = SlotNumber(url.GetOption("loadgame"));
		const std::string folder = SaveSlotFolderName(slot);
		packages->ScanSaveInfos();
		if (Package* infoPkg = packages->GetSaveInfoPackage(folder))
		{
			if (auto info = UObject::Cast<UDXSaveInfo>(infoPkg->GetUObject("DeusExSaveInfo", "MyDeusExSaveInfo")))
			{
				deusExMapName = ScriptToUTF8(info->MapName());
				DeleteSaveGameFiles("Current");
				CopySaveGameFiles(folder, "Current");
				savefilePackage = packages->LoadSaveFile("Current/" + deusExMapName + "." + packages->GetSaveExtension());
			}
		}
		if (!savefilePackage)
			LogMessage("LoadGame: no save in slot " + std::to_string(slot));
	}

	if (!savefilePackage)
		return false;

	// The loading screen, as the original's load of a save is a map's
	// load; none for an empty slot.
	if (Level && !LaunchInfo.dedicatedServer)
		PaintLoading(false);

	if (Level)
		CallEvent(console, EventName::NotifyLevelChange);

	UnloadMap();

	LevelPackage = savefilePackage;
	LevelPackage->SetIsLevel();

	GetLevelInfoObject();

	// Same as LoadMap: these are session/engine identity, not save data, and must be
	// re-established on every load regardless of what the package/save file contains.
	LevelInfo->ComputerName() = UTF8ToScript(ComputerName());
	LevelInfo->HubStackLevel() = 0; // To do: handle level hubs
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
	// First and foremost ensure the Save folder exists
	const auto saveFolderPath = packages->GetSaveFolderPath();
	if (!fs::exists(saveFolderPath))
		fs::create_directory(saveFolderPath);

	// The original's SaveGame (dx-reverse-info/deusex-dll.md, travel and saving):
	// slot 0 is a new slot, the highest SaveNNNN plus one; -1 the quick
	// save; any other number a Save%04d, a negative one too (the console's
	// `savegame -3` writes Save-003, and `savegame -2` writes its info to
	// Save-002 while SaveCurrentLevel saves the level into Current). The
	// slot is emptied, Current copied in, the SaveInfo filled and written
	// to the slot (and to Current, which keeps one of its own), and the
	// level saved on top of the copy.
	if (slotNum == 0)
		slotNum = NextSaveSlot();
	const auto slotFolderName = slotNum == -1 ? SaveSlotFolderName(slotNum) : SaveNumberedFolderName(slotNum);
	DeleteSaveGameFiles(slotFolderName);
	const auto saveSlotFolder = saveFolderPath / slotFolderName;
	if (!fs::exists(saveSlotFolder))
		fs::create_directory(saveSlotFolder);
	CopySaveGameFiles("Current", slotFolderName);

	dxSaveInfo->DirectoryIndex() = slotNum;
	dxSaveInfo->Description() = !saveDescription.empty() ? UTF8ToScript(saveDescription) : LevelInfo->Title();
	dxSaveInfo->MissionLocation() = DeusExLevelInfo ? DeusExLevelInfo->MissionLocation() : ScriptString();
	dxSaveInfo->MapName() = UTF8ToScript(Level->package->GetPackageName().ToString());
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
	fs::remove(saveSlotFolder / saveInfoName);
	dxSaveInfo->package->Save(dxSaveInfo, (saveSlotFolder / saveInfoName).string());
	const auto currentFolder = saveFolderPath / "Current";
	if (fs::exists(currentFolder))
	{
		fs::remove(currentFolder / saveInfoName);
		dxSaveInfo->package->Save(dxSaveInfo, (currentFolder / saveInfoName).string());
	}

	SaveCurrentLevel(slotNum);

	packages->RemoveSaveInfoPackage(slotFolderName);
	packages->RemoveSaveInfoPackage("Current");
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

void Engine::CopySaveGameFiles(const std::string& fromFolder, const std::string& toFolder) const
{
	const auto saveFolderPath = packages->GetSaveFolderPath();
	const auto from = saveFolderPath / fromFolder;
	const auto to = saveFolderPath / toFolder;
	if (!fs::exists(from) || !fs::is_directory(from))
		return;
	if (!fs::exists(to))
		fs::create_directories(to);
	for (const auto& entry : fs::directory_iterator(from))
	{
		if (entry.is_regular_file() && entry.path().extension() != ".old")
			fs::copy_file(entry.path(), to / entry.path().filename(), fs::copy_options::overwrite_existing);
	}
}

// The folder's files, the folder itself kept; "" means Current, as the
// original's DeleteSaveGameFiles.
void Engine::DeleteSaveGameFiles(const std::string& folder) const
{
	// The package streams closed first, as the original resets its loaders
	// before it deletes: Windows refuses to delete a file held open for
	// reading, and the save scan and a load keep up to ten open.
	packages->CloseStreams();
	const std::string name = folder.empty() ? "Current" : folder;
	const auto dir = packages->GetSaveFolderPath() / name;
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
// numbered slot alone, as the original's DeleteGame (DeusEx.dll 0x10010710)
// removes Save%04d, so the Save Game screen's Delete on the quick save's row
// (it sends -1) removes nothing, and -2 is not Current.
void Engine::DeleteGame(int32_t slot) const
{
	if (slot < 0)
		return;
	packages->CloseStreams();
	const std::string name = SaveSlotFolderName(slot);
	fs::remove_all(packages->GetSaveFolderPath() / name);
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

// The original's LoadMap checks the new map's package (LOAD_Verify) before
// anything of the old level goes, and a map that is not there fails the
// browse: Core's UrlFailed over the error for 6 s, the level playing on
// (Engine.dll 0x1038c1f0). Checked before the pre-travel too, where the
// original's Browse has already pruned the player's augmentations, skills and
// flags, and the level played on without them (NATIVES.md, saving).
bool Engine::BrowseFinds(const UnrealURL& url)
{
	if (!Level || packages->MapExists(url.Map) || !packages->FindCurrentSave(url.Map).empty())
		return true;
	std::string error = LocalizeMessage("Core", "Errors", "FileNotFound", "Can't find file '%s'", { url.Map });
	LogMessage("Failed to load '" + url.Map + "': " + error);
	SetProgress(LocalizeMessage("Core", "Errors", "UrlFailed", "Failed launching URL"), error, 6.0f);
	return false;
}

// The original's Browse of a map (DeusEx.dll 0x10010b90): the pre-travel
// first, then the load. A map that is not there leaves the level playing --
// within the mission pruned and saved into Current, as the pre-travel left
// it --, and otherwise, Current emptied, loads the level's own map again on
// the travel's URL, with its travel info: the level begins again from Maps,
// the failure's message gone with the pawn that showed it.
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
	UnrealURL loadURL = url;
	if (!BrowseFinds(url))
	{
		const bool withinMission = dxTravelUsesCurrent;
		dxTravelUsesCurrent = false;
		loadURL.Map = LevelInfo->URL.Map;
		if (withinMission || !BrowseFinds(loadURL))
			return false;
	}
	LoadMap(loadURL, travelInfo);
	if (!LaunchInfo.dedicatedServer)
		LoginPlayer();
	return true;
}

// The original's (DeusEx.dll's Browse, 0x10010b90): the DeusExLevelInfo's
// MapName -- there the first among all objects (GetDeusExLevelInfo), the
// level's own once those of the levels left are collected -- with the last
// URL's portal, ?loadonly and no other option, so the player logs in with no
// name; with none, the last URL as it is.
UnrealURL Engine::RestartURL()
{
	if (!DeusExLevelInfo)
		return LastURL;
	UnrealURL url(ScriptToUTF8(DeusExLevelInfo->MapName()));
	url.Portal = LastURL.Portal;
	url.Options.clear();
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
		HasValueOption(url, "load") || HasValueOption(url, "loadgame"))
		return;

	// For a map numbered within a mission and no ?loadonly: within the same
	// mission, for a player not starting a new game, the level is pruned and
	// saved into Current; otherwise the player's bStartingNewGame is cleared.
	// Every other way empties Current, a map outside any mission or a
	// ?loadonly with no mission's test.
	const int32_t next = DeusExMissionNumber(url.Map);
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
	// SAVING on the screen first, at every save, the quick save's too, as
	// the original's SaveCurrentLevel (DeusEx.dll 0x1000fac0) marks the
	// level saving and paints it; the action none again at the end.
	LevelInfo->LevelAction() = LEVACT_Saving;
	PaintProgress();

	const auto folder = packages->GetSaveFolderPath() / SaveSlotFolderName(slot);
	if (!fs::exists(folder))
		fs::create_directories(folder);
	const auto levelName = Level->package->GetPackageName().ToString() + "." + packages->GetSaveExtension();
	packages->CloseStreams();
	fs::remove(folder / levelName);
	// The destroyed actors let go first, as the original's save does: the
	// file holds none of them.
	Level->CleanupDestroyed(true);
	SetLevelURLForSave();
	LevelPackage->Save(Level, (folder / levelName).string());
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
	Level->Unknown = 1;
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

void Engine::LoginPlayer(const UnrealURL& loginURL)
{
	// A listen server's host is a player of the net game too.
	UnrealURL url = loginURL;
	if (LevelInfo && LevelInfo->NetMode() == NM_ListenServer)
		SetMultiplayerName(url);

	std::map<std::string, std::string> travelInfo = Level->TravelInfo;

	auto stringProp = GC::Alloc<UStringProperty>("", nullptr, ObjectFlags::NoFlags);
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

	pawn->LoadProperties();

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

	// Transfer travel actors to the new map

	CallEvent(pawn, EventName::TravelPreAccept);

	// The travel is taken whether or not Login spawned a pawn, as the
	// original's SpawnPlayActor takes it (its one test is ?loadonly, Engine.dll
	// 0x10396bd0): a Login that reuses a pawn keeps its travelling items.
	// The travel info is keyed by the player's PlayerName, which the stock
	// Login cuts to 20 characters: the URL's name is looked up whole, then
	// cut, and standalone with none found the first entry is taken, as the
	// original's does.
	Array<UObject*> acceptedObjects;
	if (ClientTravelInfo.TravelType == ETravelType::TRAVEL_Relative)
	{
		std::string playerName = url.GetOption("Name");
		if (playerName.empty())
			playerName = packages->GetIniValue("system", "URL", "Name");

		auto it = travelInfo.find(playerName);
		if (it == travelInfo.end() && playerName.size() > 20)
			it = travelInfo.find(playerName.substr(0, 20));
		if (it == travelInfo.end() && !travelInfo.empty() && LevelInfo->NetMode() == NM_Standalone)
			it = travelInfo.begin();
		if (!playerName.empty() && it != travelInfo.end())
		{
			acceptedObjects = ActorTravelInfo::Accept(pawn, it->second);
		}
		else
		{
			if (travelInfo.empty())
				LogMessage("Skipping travel transfer. No travel data");
			else if (playerName.empty())
				LogMessage("Skipping travel transfer. Player name is empty");
			else
				LogMessage("Skipping travel transfer. Player '" + playerName + "' not found in travel info");
		}
	}

	// The travelled items' TravelPreAccept, the game's AcceptInventory, the
	// items' TravelPostAccept, each run over the items last to first and the
	// player's own pair round them, as the original's SpawnPlayActor runs
	// them (Engine.dll 0x10396bd0): an item's GiveTo then links the chain in
	// the original's order, and the player, whose travel entry is among the
	// accepted, gets each of its events once
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
		SetProgress(LocalizeMessage("Engine", "Errors", "ConnectionFailed", "Connection failed"), error, 4.0f);
		LogMessage("Net: " + LocalizeMessage("Engine", "Errors", "Pending", "Pending connect to '%s' failed; %s", { PendingLevel->URL.ToString(), error }));
		CancelPending();
	}
	else if (PendingLevel->Success && PendingLevel->FilesNeeded == 0)
	{
		std::unique_ptr<NetPendingLevel> pending = std::move(PendingLevel);
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
	std::string line1 = LocalizeMessage("Engine", "Progress", "ConnectingText", "Connecting (F10 Cancels):");
	std::string line2 = LocalizeMessage("Engine", "Progress", "ConnectingURL", "unreal://%s/%s", { pending.URL.Host, pending.URL.Map });
	SetProgress(line1, line2, 60.0f);
}

void Engine::LoadClientMap(NetPendingLevel* pending)
{
	UnrealURL url = pending->URL;
	LogMessage("Net: loading " + url.Map + " as a client");
	LoadMap(url, {}, true);

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
	pawn->LoadProperties();

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
	auto stringProp = GC::Alloc<UStringProperty>("", nullptr, ObjectFlags::NoFlags);
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
	auto stringProp = GC::Alloc<UStringProperty>("", nullptr, ObjectFlags::NoFlags);
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

	CallEvent(pawn, EventName::TravelPreAccept);
	CallEvent(LevelInfo->Game(), EventName::AcceptInventory, { ExpressionValue::ObjectValue(pawn) });
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

float Engine::CalcTimeElapsed()
{
	using namespace std::chrono;

	// A client runs no more frames a second than its connection's speed over
	// 64, as the original's GetMaxTickRate has it: 40 at the default 2,600. A
	// dedicated server runs [IpDrv.TcpNetDriver]'s rate, 10 to 120: 20 on the
	// internet, 35 on a LAN (--lanplay). A listen server runs no more than 144,
	// where the original's runs as fast as it draws: a remote client's queue
	// drains its rate over a tick, and the faster the ticks the less room an
	// unreliable call finds (dx-reverse-info/network.md, packets) -- the
	// sounds a remote player hears go so.
	int maxTickRate = (LevelNetDriver && LevelNetDriver->ServerConnection) ? LevelNetDriver->ServerConnection->CurrentNetSpeed / 64 : 0;
	if (LevelNetDriver && !LevelNetDriver->ServerConnection && LaunchInfo.dedicatedServer)
		maxTickRate = std::clamp(LaunchInfo.lanPlay ? LevelNetDriver->LanServerMaxTickRate : LevelNetDriver->NetServerMaxTickRate, 10, 120);
	else if (LevelNetDriver && !LevelNetDriver->ServerConnection)
		maxTickRate = 144;
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
			// LoginPlayer only transfers travel actors when ClientTravelInfo.TravelType is
			// TRAVEL_Relative, and TravelType is otherwise assigned only in ClientTravel. These
			// NextURL routes never go through ClientTravel, so without setting it here they
			// inherit whatever the last ClientTravel left behind (or TRAVEL_Absolute, the
			// default) - which silently discarded the inventory the bNextItems branch exists
			// specifically to carry. State the intent explicitly on each branch instead of
			// depending on leftover state.
			if (UnrealURL(ScriptToUTF8(LevelInfo->NextURL())).HasOption("restart"))
			{
				DeleteSaveGameFiles("Current"); // the original's ?restart empties Current
				// Carries nothing: the original's Browse loads the map with no
				// travel info and ?loadonly, whose login takes none of it, the
				// player's own or its items' (Engine.dll's SpawnPlayActor,
				// 0x10396bd0)
				ClientTravelInfo.TravelType = ETravelType::TRAVEL_Absolute;
				UnrealURL restartURL = RestartURL();
				LoadMap(restartURL, {});
				if (!dedicated)
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
				ClientTravelInfo.TravelType = LevelInfo->bNextItems() ? ETravelType::TRAVEL_Relative : ETravelType::TRAVEL_Absolute;
				auto travelInfo = LevelInfo->bNextItems() ? CreateServerTravelInfo() : std::map<std::string, std::string>();
				if (!BrowseMap(UnrealURL(LastURL, ScriptToUTF8(LevelInfo->NextURL())), travelInfo))
					LevelInfo->NextURL().clear();
			}
			return true;
		}
	}

	if (ClientTravelInfo.URL.HasOption("restart"))
	{
		DeleteSaveGameFiles("Current"); // the original's ?restart empties Current
		// Carries nothing, as the NextURL's ?restart above
		ClientTravelInfo.TravelType = ETravelType::TRAVEL_Absolute;
		UnrealURL restartURL = RestartURL();
		LoadMap(restartURL, {});
		if (!dedicated)
			LoginPlayer();
		return true;
	}

	// A slot's load is ?loadgame=N or ?load=N, as the original's Browse
	// asks for "loadgame=" and "load=": a bare ?loadgame is a map's
	// browse, its map the default one.
	if (HasValueOption(ClientTravelInfo.URL, "load") || HasValueOption(ClientTravelInfo.URL, "loadgame"))
	{
		UnrealURL url(ClientTravelInfo.URL);
		LoadSavedGame(url);
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

	if (!ClientTravelInfo.URL.Host.empty())
	{
		UnrealURL url(ClientTravelInfo.URL);
		ClientTravelInfo.URL.Clear();
		BeginConnect(url);
		return true;
	}
	else if (!ClientTravelInfo.URL.Map.empty())
	{
		UnrealURL url(ClientTravelInfo.URL);
		LogMessage("Client travel to " + url.ToString());
		// Captured before the pre-travel prune deletes what crosses in
		// the pawn's graph.
		auto travelInfo = CreateClientTravelInfo(ClientTravelInfo.TransferItems);
		if (!BrowseMap(url, travelInfo))
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

std::string Engine::ConsoleCommand(UObject* context, const std::string& commandline, BitfieldBool& found)
{
	found = false;

	Array<std::string> args = GetArgs(commandline);
	if (args.empty())
	{
		return {};
	}

	std::string command = args[0];
	for (char& c : command) c = std::tolower(c);

	found = true;
	// A dedicated server has no window and no renderer: the commands that
	// take them do nothing there (a logged-in admin's "admin getres" or
	// "admin flush" came through PlayerPawn.Admin)
	if ((!window || !render) && (command == "getres" || command == "getcurrentres" || command == "setres" || command == "togglefullscreen" || command == "flush" || command == "stat" || command == "shot"))
		return {};

	// The original's order (UViewport::Exec, Engine.dll 0x10373a60;
	// engine-dll.md, a key's binding): the input's words and aliases, the
	// render device's and the viewport's own, then the exec functions
	// (ExecCommand), then the engine's words. So a word an exec function
	// shares is the viewport's (SHOT) or the exec function's (TIMEDEMO, the
	// console's).
	static const std::set<std::string> viewportWords = {
		"shot", "exec", "getping", "getloss", "netspeed", "lanspeed", "keyname", "keybinding",
		"getres", "getcolordepths", "getcurrentres", "getcurrentcolordepth", "setres", "togglefullscreen",
		"showlog", "dxwindowdebug"
	};
	if (InputExec(commandline, IK_None, EInputType::IST_Press, 20))
		return {};
	bool viewportWord = viewportWords.count(command) != 0;
	// An exec function's refusal, Core's BadProperty, is the command's output,
	// as the original writes it to the command's output device: the console's
	// message, or what ConsoleCommand returns (Engine.dll 0x103ba200's string)
	std::string output;
	if (!viewportWord && ExecCommand(args, commandline, output))
		return output;

	if (command == "exit" || command == "quit")
	{
		quit = true;
	}
	else if (command == "relaunch")
	{
		Relaunch(commandline);
	}
	else if (command == "shot")
	{
		TakeScreenshot();
	}
	else if (command == "exec")
	{
		// UViewport::Exec's EXEC (Engine.dll 0x10374241): the file the first
		// token past the word, by ParseToken into 64 characters -- a quoted
		// one whole, 63 characters at most --; with none nothing runs, the
		// command taken all the same
		size_t pos = commandline.find_first_of(" \t", commandline.find_first_not_of(" \t"));
		std::string filename;
		if (pos != std::string::npos && StrTools::ue1_parse_token(commandline, pos, filename, 64))
			ExecFile(context, filename);
	}
	else if (command == "stat" && args.size() == 2)
	{
		render->ShowRenderStats = 0;

		if (args[1] == "render")
			render->ShowRenderStats = 1;
	}
	else if (command == "dxwindowdebug")
	{
		m_DrawDebugDXWindowHierarchy = !m_DrawDebugDXWindowHierarchy;
	}
	else if (command == "showlog")
	{
		// The original's shows its log window, which DXEngine has not
		LogMessage("showlog: DXEngine has no log window");
	}
	/*else if (command == "playsong")
	{
		auto music = LevelInfo->Song();
		if (music)
			audio->PlayMusic(AudioSource::CreateMod(music->Data, true, 0, LevelInfo->SongSection()));
	}
	else if (command == "stopsong")
	{
		audio->PlayMusic(nullptr);
	}*/
	else if (command == "getres")
	{
		return window->GetMenuResolutions();
	}
	else if (command == "getcolordepths")
	{
		return "32 16";
	}
	else if (command == "getcurrentres")
	{
		// The window's own size, as GETRES lists it, not the render size
		int width = window->GetNativePixelWidth();
		int height = window->GetNativePixelHeight();

		return std::to_string(width) + "x" + std::to_string(height);
	}
	else if (command == "getcurrentcolordepth")
	{
		return "32";
	}
	else if (command == "obj" && args.size() >= 2 && StrTools::equals_ignore_case(args[1], "garbage"))
	{
		// The original's OBJ GARBAGE, at the end of this frame.
		RequestGarbageCollection("obj garbage");
	}
	else if (command == "getping")
	{
		// The original's UNetConnection::Exec (0x10401c50): the connection's
		// best lag of the last stats period in ms, which
		// PlayerReplicationInfo shows as the ping; 0 with no server
		NetConnection* server = LevelNetDriver ? LevelNetDriver->ServerConnection.get() : nullptr;
		return std::to_string(server && server->BestLag < 9999.0f ? (int)(server->BestLag * 1000.0f) : 0);
	}
	else if (command == "getloss")
	{
		// Its incoming packet loss of the period, in percent
		NetConnection* server = LevelNetDriver ? LevelNetDriver->ServerConnection.get() : nullptr;
		return std::to_string(server ? (int)server->InLoss : 0);
	}
	else if (command == "keyname" && args.size() == 2)
	{
		uint8_t index = Convert::to_uint8(args[1]);
		return keynames[index];
	}
	else if (command == "keybinding" && args.size() == 2)
	{
		const std::string& name = args[1];
		return keybindings[name];
	}
	else if (command == "open" || command == "start")
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
		// original's.
		size_t word = commandline.find_first_not_of(" \t");
		size_t rest = commandline.find_first_not_of(" \t", commandline.find_first_of(" \t", word));
		ClientTravel(rest == std::string::npos ? std::string() : commandline.substr(rest), command == "open" ? ETravelType::TRAVEL_Partial : ETravelType::TRAVEL_Absolute, false);
	}
	else if ((command == "netspeed" || command == "lanspeed") && args.size() == 2)
	{
		// The rate asked of servers, kept for the next join; on a server of
		// that kind (?LAN or not), asked now (the original's viewport's).
		// Both keep it as the internet speed, as the original's both do.
		int rate = std::atoi(args[1].c_str());
		bool lan = command == "lanspeed";
		LogMessage("Net: " + command + " " + std::to_string(rate));
		packages->SetIniValue("system", "Engine.Player", "ConfiguredInternetSpeed", std::to_string(rate));
		NetConnection* connection = LevelNetDriver ? LevelNetDriver->ServerConnection.get() : nullptr;
		if (rate >= 500 && connection && LevelInfo->URL.HasOption("LAN") == lan)
		{
			connection->CurrentNetSpeed = std::min(rate, LevelNetDriver->MaxClientRate);
			viewport->CurrentNetSpeed() = connection->CurrentNetSpeed;
			connection->SendText("NETSPEED " + std::to_string(rate));
		}
	}
	else if (command == "servertravel" && args.size() >= 2)
	{
		// The level's own event: its game tells the clients, and the server
		// follows once NextSwitchCountdown runs out (the original's).
		size_t word = commandline.find_first_not_of(" \t");
		size_t blank = commandline.find_first_of(" \t", word);
		std::string url = commandline.substr(commandline.find_first_not_of(" \t", blank));
		CallEvent(LevelInfo, "ServerTravel", { ExpressionValue::StringValue(url), ExpressionValue::BoolValue(false) });
	}
	else if (command == "cancel")
	{
		// A join given up (the progress window's F10).
		if (PendingLevel)
			SetProgress(LocalizeMessage("Engine", "Progress", "CancelledConnect", "Cancelled Connect Attempt"), "", 2.0f);
		else
			SetProgress("", "", 0.0f);
		CancelPending();
	}
	else if (command == "disconnect")
	{
		// Back to the menu's map, the server told as the level goes (Deus
		// Ex's own goes to dx.dx).
		ClientTravel("dx.dx", ETravelType::TRAVEL_Absolute, false);
	}
	else if (command == "reconnect")
	{
		// The last URL again: a client's is its server's.
		if (LevelInfo)
			ClientTravel(LastURL.ToString(), ETravelType::TRAVEL_Absolute, false);
	}
	else if (command == "savegame" && (args.size() == 2 || args.size() == 3))
	{

		int32_t slotNum;
		// slotNum not being parsable shouldn't cause a crash
		try
		{
			slotNum = Convert::to_int32(args[1]);
		}
		catch (...)
		{
			return {};
		}

		SaveGameInfo.SaveGameSlot = slotNum;
		TakeSaveSnapshot();

		if (args.size() == 3)
			SaveGameInfo.SaveGameDescription = args[2];

		// SaveGameToSlot(slotNum, "");

		//LogMessage("SaveGame command not fully implemented yet!");
		return {};
	}
	else if (command == "deletegame" && args.size() == 2)
	{
		// The Load and Save Game screens delete a save with DELETEGAME n.
		try
		{
			DeleteGame(Convert::to_int32(args[1]));
		}
		catch (...)
		{
		}
		return {};
	}
	else if (command == "get" && args.size() == 3)
	{
		NameString className = ParseClassName(args[1]);
		NameString propertyName = args[2];

		UClass* cls = packages->FindClassAnyPackage(className);
		if (!cls)
		{
			LogMessage("Could not find class '" + className.ToString() + "': " + commandline);
			return {};
		}

		if (IsClassNamed(className, renderdev->Class))
		{
			return renderdev->GetPropertyAsString(propertyName);
		}
		else if (IsClassNamed(className, audiodev->Class))
		{
			return audiodev->GetPropertyAsString(propertyName);
		}
		else if (IsClassNamed(className, netdev->Class))
		{
			return netdev->GetPropertyAsString(propertyName);
		}
		else if (IsClassNamed(className, client->Class))
		{
			return client->GetPropertyAsString(propertyName);
		}
		else
		{
			try
			{
				return ExportForGet(cls, propertyName);
			}
			catch (const std::exception&)
			{
				LogMessage("Could not get property '" + propertyName.ToString() + "': " + commandline);
				return {};
			}
		}
	}
	else if (command == "set" && args.size() >= 3)
	{
		NameString className = ParseClassName(args[1]);
		NameString propertyName = args[2];
		// The value is the rest of the line, as the original's SET takes it
		// (Core.dll's UObject::StaticExec, 0x101531b0): a server's name has
		// spaces.
		std::string value = ArgsFrom(commandline, 3);

		// Special input setting handling. Deus Ex's input class is Extension's
		// InputExt, so the multiplayer key SETs land here too instead of
		// failing on every map (extension-dll.md, the engine and the input).
		if (className == "input" || className == "inputext")
		{
			keybindings[propertyName.ToString()] = value;
			packages->SetIniValue("user", "Engine.Input", propertyName, value);
			return {};
		}

		UClass* cls = packages->FindClassAnyPackage(className);
		if (!cls)
		{
			LogMessage("Could not find class '" + className.ToString() + "': " + commandline);
			return {};
		}

		if (IsClassNamed(className, renderdev->Class))
		{
			renderdev->SetPropertyFromString(propertyName, value);
		}
		else if (IsClassNamed(className, audiodev->Class))
		{
			audiodev->SetPropertyFromString(propertyName, value);
		}
		else if (IsClassNamed(className, netdev->Class))
		{
			netdev->SetPropertyFromString(propertyName, value);
		}
		else if (IsClassNamed(className, client->Class))
		{
			client->SetPropertyFromString(propertyName, value);
			client->PostEditChange();
		}
		else
		{
			try
			{
				// As the original's GlobalSetProperty (Core.dll 0x101523a0):
				// the property the class's field walk finds, its own first,
				// set in every object of the class or a subclass, each then
				// told of it (PostEditChange), then in the class's defaults,
				// and the class's config is saved. The devices and the
				// client keep their values in members of their own, set by
				// name.
				UProperty* prop = cls->FindProperty(propertyName);
				if (!prop)
					Exception::Throw("Class Property '" + cls->Name.ToString() + "." + propertyName.ToString() + "' not found");
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
			}
			catch (const std::exception&)
			{
				LogMessage("Could not set property '" + propertyName.ToString() + "': " + commandline);
			}
			return {};
		}
	}
	else if (command == "setres" && args.size() == 2)
	{
		window->SetResolution(args[1]);
		// The new size is kept as the windowed size, or full screen the
		// mode taken, which the ini saves, as the original's SETRES keeps it.
		int width = 0, height = 0;
		if (window->IsFullscreen())
		{
			Size mode = window->GetClosestResolution(Size(window->GetPixelWidth(), window->GetPixelHeight()));
			if (sscanf(args[1].c_str(), "%dx%d", &width, &height) == 2 && width >= 640 && height >= 480)
				mode = window->GetClosestResolution(Size(width, height));
			client->SetPropertyFromString("FullscreenViewportX", std::to_string((int)mode.width));
			client->SetPropertyFromString("FullscreenViewportY", std::to_string((int)mode.height));
		}
		else if (sscanf(args[1].c_str(), "%dx%d", &width, &height) == 2 && width >= 640 && height >= 480)
		{
			client->SetPropertyFromString("WindowedViewportX", std::to_string(width));
			client->SetPropertyFromString("WindowedViewportY", std::to_string(height));
		}
	}
	else if (command == "togglefullscreen")
	{
		ToggleFullscreen();
		return {};
	}
	else if (command == "flush")
	{
		engine->render->Device->Flush(1);
		return {};
	}
	else if (!viewportWord || !ExecCommand(args, commandline, output))
	{
		LogMessage("Unknown command: " + commandline);
		found = false;
	}
	return output;
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

Array<std::string> Engine::GetSubcommands(const std::string& command)
{
	Array<std::string> subcommands;
	size_t pos = 0;
	while (pos < command.size())
	{
		size_t endpos = command.find('|', pos);
		if (endpos == std::string::npos)
			endpos = command.size();

		std::string subcommand = command.substr(pos, endpos - pos);
		if (!subcommand.empty())
			subcommands.push_back(subcommand);
		pos = endpos + 1;
	}
	return subcommands;
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
		RunCommand(nullptr, "set Engine.Player ConfiguredInternetSpeed 20000");

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
// binding): every input float a script left since the last read is scaled
// by 20 over the tick, as the original scales each, then the keys held and
// the mouse add theirs, already in those units (a held key's axis its speed
// x 20, the original's speed x the tick x 20 over the tick). The buttons are
// set and cleared by their keys' press and release alone (InputExec); each
// held key's line runs again as held, its input words alone, as the
// original's read runs it, its axes counted here.
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
		{
			float add = (axis == "aMouseX" || axis == "aMouseY") ? value / (elapsed * 150.0f) : value;
			*static_cast<float*>(player->GetProperty(prop)) += add;
		}
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

	InputEvent(IK_MouseX, IST_Axis, 0);
	InputEvent(IK_MouseY, IST_Axis, 0);

	GameWindow::ProcessEvents();

	if (MouseMoveX != 0 || MouseMoveY != 0)
	{
		int dx = MouseMoveX;
		int dy = MouseMoveY;
		MouseMoveX = 0;
		MouseMoveY = 0;

		// Send to input subsystem.
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

void Engine::ToggleFullscreen()
{
	bool isFullscreen = window->IsFullscreen();

	// Get the resolution to SWITCH TO
	int width = isFullscreen ? client->WindowedViewportX : client->FullscreenViewportX;
	int height = isFullscreen ? client->WindowedViewportY : client->FullscreenViewportY;

	Size resolution;
	resolution.width = width;
	resolution.height = height;

	window->ToggleWindowFullscreen(resolution);
	viewport->SetViewportRect(0, 0, width, height);
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
		ExecInputLine(keybindings[keynames[key]], key, type, delta * 16);
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

// The rest of the line from its (word)th word on, the words blank-separated
// as GetArgs splits them
static std::string LineFromWord(const std::string& line, size_t word)
{
	size_t i = 0;
	for (size_t w = 0; w < word; w++)
	{
		i = line.find_first_not_of(" \t", i);
		if (i == std::string::npos)
			return {};
		i = line.find_first_of(" \t", i);
		if (i == std::string::npos)
			return {};
	}
	i = line.find_first_not_of(" \t", i);
	return i == std::string::npos ? std::string() : line.substr(i);
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

// The exec functions in the original's order: the console's (UViewport::
// Exec), then the game's, the HUD's, the player pawn's and each item of its
// inventory in turn (UPlayer::Exec, Engine.dll 0x103d7ed0), each with the
// viewport's actor as the command's executor. The parameters as the
// original's ScriptConsoleExec takes them (Core.dll 0x10150860; bugs.md
// ENG.17): a first one that is an object the executor is, the executor; each
// other in turn, past blanks and a ';' comment (ParseNext), by its
// property's ImportText from where the last one stopped: a string the rest of
// the line, its position left where it was; a float to the next ',' or ')';
// an object one character past its name. A refused import ends them: for an
// optional parameter the call is made, the rest zero; for any other Core's
// BadProperty is the command's output and no call is made. The command is
// taken either way.
bool Engine::ExecCommand(const Array<std::string>& args, const std::string& line, std::string& output)
{
	Array<UObject*> targets;
	targets.push_back(console);
	UPlayerPawn* player = UObject::TryCast<UPlayerPawn>(viewport->Actor());
	if (LevelInfo && LevelInfo->Game())
		targets.push_back(LevelInfo->Game());
	if (player && player->myHUD())
		targets.push_back(player->myHUD());
	targets.push_back(viewport->Actor());
	if (player)
	{
		for (UInventory* item = player->Inventory(); item; item = item->Inventory())
			targets.push_back(item);
	}

	// The parameters' text, past the command's word as ParseToken leaves it
	size_t start = line.find_first_not_of(" \t");
	if (start != std::string::npos)
		start = line.find_first_of(" \t", start);
	if (start == std::string::npos)
		start = line.size();

	UObject* executor = viewport->Actor();
	for (UObject* target : targets)
	{
		if (!target)
			continue;

		UFunction* func = FindEventFunction(target, args[0]);
		if (!func || !AllFlags(func->FuncFlags, FunctionFlags::Exec))
			continue;

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
					output = LocalizeMessage("Core", "Errors", "BadProperty", "'%s': Bad or missing property '%s'", { func->Name.ToString(), prop->Name.ToString() });
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

	return false;
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
// binding line by line: a press's to the viewport's commands -- the input's
// own first (InputExec), then the console's (RunCommand) --, as a release's
// that starts with OnRelease does, the rest of it; anything else -- an
// axis, any other release -- to the input's own alone. OnRelease's line on
// the press, which no command takes, is passed over.
void Engine::ExecInputLine(const std::string& line, EInputKey key, EInputType action, int value)
{
	for (const std::string& command : GetSubcommands(line))
	{
		Array<std::string> args = GetArgs(command);
		if (args.empty())
			continue;
		bool onRelease = StrTools::equals_ignore_case(args[0], "onrelease");
		if (action == EInputType::IST_Press)
		{
			if (!onRelease && !InputExec(command, key, action, value))
				RunCommand(viewport->Actor(), command);
		}
		else if (action == EInputType::IST_Release && onRelease)
		{
			std::string rest = LineFromWord(command, 1);
			if (!rest.empty() && !InputExec(rest, key, action, value))
				RunCommand(viewport->Actor(), rest);
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
// the speed x the tick, which the read scales by 20 over the tick), or an
// axis's move, the speed x the move x 16; an alias's name runs the alias's
// line as the key's, with its action, an alias inside it not expanded again.
// False for any other word, which the viewport's commands take.
bool Engine::InputExec(const std::string& command, EInputKey key, EInputType action, int value)
{
	Array<std::string> args = GetArgs(command);
	if (args.empty())
		return false;
	std::string word = args[0];
	for (char& c : word) c = std::tolower(c);
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
		if (!actor || args.size() < 2 || !InputProperty(actor, args[1], true))
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
		if (key != IK_None && (action == EInputType::IST_Press || action == EInputType::IST_Axis))
			activeInputAxes[{ args[1], key }] = speed * value;
		return true;
	}

	if (!inputAliasRunning)
	{
		auto it = inputAliases.find(args[0]);
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
