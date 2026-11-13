#pragma once

#include "Utils/Logger.h"
#include "Math/vec.h"
#include "Math/mat.h"
#include "Math/floating.h"
#include "RenderDevice/RenderDevice.h"
#include "GameWindow.h"
#include "Packages/Engine/Actors/UActor.h"
#include "UnrealURL.h"
#include "GameFolder.h"
#include "GamepadInput.h"
#include <set>
#include <list>

class RenderSubsystem;
class PackageManager;
class UObject;
class ULevel;
class UModel;
class GameWindow;
class UTexture;
class UActor;
class UFont;
class UMesh;
class ULodMesh;
class ULevelSummary;
class ULevelInfo;
class UZoneInfo;
class UClient;
class UViewport;
class UCanvas;
class UConsole;
class UPlayerPawn;
class UGameInfo;
class UGameReplicationInfo;
class UPlayerReplicationInfo;
class UGameEngine;
class UDXEngineRenderDevice;
class UDXEngineAudioDevice;
class UDXEngineNetworkDevice;
class UDXEngineClient;
class BspSurface;
class BspNode;
class LightMapIndex;
class FrustumPlanes;
class AudioSubsystem;
class Rotator;
class ExpressionValue;
class UnrealURL;
class UFloatProperty;
class UObjectProperty;
class UStructProperty;
class GCMarker;
class UDXSaveInfo;
class UDeusExLevelInfo;
class URootWindow;
class UGC;
class NetPendingLevel;
class NetDriver;
class NetClientLevel;
class NetServerLevel;
class NetConnection;
struct TextureInfo;
struct SceneNode;
struct SurfaceFacet;
struct MeshFace;

static constexpr int32_t DONT_SAVE_GAME = INT32_MIN; // No slot: -1 is the quick save's, -2 Current's, and the original's SaveGame takes any other number

// What a command writes, a line at a time, as the original's output devices
// take each Log: a ConsoleCommand's string runs them together (a string
// device, Engine.dll 0x10374500, adds no line break), the console shows each
// as a message (UConsole::Serialize 0x10379740), and a dedicated server's
// typed line logs each.
struct ExecOutput
{
	Array<std::string> Lines;
	void Log(const std::string& line) { Lines.push_back(line); }
	std::string Joined() const
	{
		std::string text;
		for (const std::string& line : Lines)
			text += line;
		return text;
	}
};

class Engine : public GameWindowHost
{
public:
	Engine(GameLaunchInfo launchinfo);
	~Engine();

	void Run();
	// Whether a SIGTERM, SIGINT or SIGHUP has come since the engine took
	// them (InstallStopSignal), which the frame loop and the Fatal Error
	// window quit on; never on Windows, which has none
	static bool IsStopSignalled();
	void ClientTravel(const std::string& URL, ETravelType travelType, bool transferItems);
	UnrealURL GetDefaultURL(const std::string& map);
	// The player's options in the URL, each Key=value: Deus Ex's every key of
	// the user ini's [DefaultPlayer] as the ini spells them, in its order, a
	// key given twice its last value, and a line with no name (=x) the option
	// =x, as the original's FURL::LoadURLConfig adds the section's lines
	// (Engine.dll 0x103f9bc0), each in place of one of its name before it
	// (AddOption 0x103f9960): =x after =y one, a nameless one's place.
	Array<std::string> DefaultPlayerOptions();
	void SetMultiplayerName(UnrealURL& url);
	void LoadEntryMap();
	// The Entry level made the level played, a new player spawned in it, as
	// the original's browse to ?failed or ?entry: a lost server, or one that
	// refused the player after the join, sends a client there.
	void ReturnToEntry(const UnrealURL& url);
	// A map's load, as the original's LoadMap: false, the error given, where
	// the new map fails its check, the old level playing on.
	bool LoadMap(const UnrealURL& url, const std::map<std::string, std::string>& travelInfo = {}, bool asClient = false, std::string* error = nullptr);
	// A ?push's save of the level left into the save path's Game%04i.dxs.
	void SaveHubLevel(int32_t hubStackLevel);
	// A save in place of the level; false when there is none to load.
	bool LoadFromSaveFile(const UnrealURL& url);
	// The same, its player logged in as the original's load logs it in.
	bool LoadSavedGame(const UnrealURL& url);
	// The original's loading screen (dx-reverse-info/engine-dll.md, the
	// loading screen): one frame painted before a map loads or a level is
	// saved, up until the next. PaintLoading is the map load's: LOADING, or
	// CONNECTING for a join's map.
	void PaintProgress();
	void PaintLoading(bool connecting);
	// A frame drawn (the original's UGameEngine::Draw): a pending join's
	// message first, then the view, the console's tick and the frame. The
	// frame's own, and the one a join's map load draws before its JOIN.
	void Draw(float realTimeElapsed, float levelElapsed, const NetPendingLevel* pending);
	void SaveGameToSlot(int32_t slotNum, const std::string& saveDescription);
	// Deus Ex's save picture, taken when the save is asked for: of the frame
	// the player last saw, where the save itself waits for the next one.
	void TakeSaveSnapshot();
	void TakeScreenshot();
	// Deus Ex's save directories (dx-reverse-info/deusex-dll.md, travel and saving):
	// Save%04d a slot, QuickSave (-1) the quick save, Current (-2) the
	// mission in progress.
	std::string SaveSlotFolderName(int32_t slot) const;
	std::string SaveNumberedFolderName(int32_t slot) const;
	int32_t NextSaveSlot() const;
	void CopySaveGameFiles(const std::string& fromFolder, const std::string& toFolder) const;
	void DeleteSaveGameFiles(const std::string& folder) const;
	void DeleteGame(int32_t slot) const;
	// Deus Ex travel: within a mission the departing level is pruned and
	// saved into Current, and a map saved there is revisited as the player
	// left it; a new mission, or a player starting a new game, empties
	// Current (dx-reverse-info/deusex-dll.md, travel and saving).
	void DeusExPreTravel(const UnrealURL& url);
	// A travel to a map as the original's Browse makes it: the pre-travel,
	// then the load, or a failed load's fallback; false when no level loads.
	bool BrowseMap(const UnrealURL& url, const std::map<std::string, std::string>& travelInfo);
	// The start's browse of a map: Current emptied, the load, or Dx.Dx for
	// a map that fails its check; false, the error's text given, when no
	// level loads.
	bool BrowseStartMap(const UnrealURL& url, std::string& error);
	// A map's load that fails its check: FailedLoad logged, UrlFailed shown
	// over the reason for 6 s, the reason given as the error.
	void FailLoad(const std::string& map, const std::string& reason, std::string& error);
	// The check of a package's file that is there: false, the load failed,
	// for one that is not a package.
	bool VerifyPackageFile(const std::string& map, const std::string& file, std::string& error);
	// The check of a level's package before its load, as the original's
	// LoadMap checks it: the package made of the file, or none, the load
	// failed, for a file not there, not a package, without its MyLevel or
	// importing a package that is not there.
	Package* CheckLevelPackage(const std::string& map, const std::string& name, const std::string& file, std::string& error);
	// Whether the packages a package imports, and theirs, are all there.
	bool ImportsFound(Package* package, std::set<std::string>& seen, Array<std::string>& failed, std::string& error);
	// The map a ?restart reloads, as the original's Browse makes it.
	UnrealURL RestartURL();
	void PruneTravelActors() const;
	void SaveCurrentLevel(int32_t slot);
	void SetLevelURLForSave() const;
	UnrealURL LevelHeldURL() const;
	int32_t DeusExMissionNumber(const std::string& mapName) const;
	void UnloadMap();
	void LoginPlayer();
	// The local player logged in with a URL's portal and options, as the
	// original's SpawnPlayActor: the level's own URL, or a save load's.
	void LoginPlayer(const UnrealURL& url);
	// A joining client's player while it waits for its pawn: logged in to
	// the Entry level with the join's URL, as the original's map load logs a
	// net client's in there (dx-reverse-info/engine-dll.md, a map's load).
	void LoginEntryPlayer(const UnrealURL& url);

	// Joining a server (dx-reverse-info/network.md, joining): the handshake while the
	// current level plays on, then the server's map loaded as a client, and
	// the player the server spawns for it.
	void BeginConnect(const UnrealURL& url);
	void TickPendingLevel(float realTimeElapsed);
	// The join given up (the CANCEL command's): the server told, the level
	// playing on.
	void CancelPending();
	// Two lines the game shows the player for the seconds given; -1 seconds
	// is the game's upgrade menu (the original's SetProgress).
	void SetProgress(const std::string& line1, const std::string& line2, float seconds);
	// While joining, what shows when nothing else does: the server's address
	// and map, for a minute at a time (the original's each frame drawn).
	void UpdateConnectingMessage(const NetPendingLevel& pending);
	void SetEngineVersion();
	void LoadClientMap(NetPendingLevel* pending);
	void HandleClientPlayer(NetConnection* connection, UPlayerPawn* pawn);
	void CloseNetDriver();

	// A server's (dx-reverse-info/network.md): listening on a map opened with
	// ?listen, the game's PreLogin, and a player spawned and possessed as the
	// original's SpawnPlayActor does.
	void Listen(const UnrealURL& url);
	bool PreLogin(const std::string& options, const std::string& address, std::string& error, std::string& failcode);
	UPlayerPawn* SpawnPlayActor(UObject* player, uint8_t remoteRole, const UnrealURL& url, std::string& error);


	// The collector (docs/ENGINE.md, objects and memory). A request is
	// carried out at the end of the frame, with no script running.
	// A map change's also takes the new level's actors that are not in its
	// Actors (the original's orphans).
	void RequestGarbageCollection(const std::string& reason, bool mapChange = false);
	void CollectGarbage();
	// The engine's roots: its subsystems, levels and objects, the net
	// drivers', the natives' and the package manager's.
	void MarkRoots(GCMarker& marker);
	std::string GarbageRequest;
	bool GarbageMapChange = false;
	// A texture freed by the collection under way (UTexture::OnGCDestroy).
	bool GarbageTextureDied = false;
	// Actors freed by no collection yet since ULevel::CleanupDestroyed let go
	// of them: past the threshold a collection is asked for. A collection
	// is long on a slow CPU (some 250 ms on a Cortex-A53), so the backlog is
	// let grow.
	int GarbageDestroyedBacklog = 0;
	static constexpr int GarbageDestroyedThreshold = 2048;
	// DXE_GC_STRESS=<frames>: a collection every that many frames.
	int GarbageStressFrames = 0;
	int GarbageStressCount = 0;

	// The original's command layers, each true when one of its words or exec
	// functions took the line, the first taker winning (dx-reverse-info
	// engine-dll.md, a key's binding; network.md, the game engine's
	// commands): the local viewport's -- the console's line, a key's, an exec
	// file's, the local player's ConsoleCommand -- (UWindowsViewport::Exec);
	// a player's own (UPlayer::Exec), a remote one's through its connection
	// first (UNetConnection::Exec); and the game engine's -- an actor's
	// ConsoleCommand, a dedicated server's typed line, the end of a player's
	// -- (DDeusExGameEngine::Exec, UGameEngine::Exec, UEngine::Exec).
	bool ViewportExec(const std::string& line, ExecOutput& out);
	bool PlayerExec(UPlayerPawn* pawn, const std::string& line, ExecOutput& out);
	bool ConnectionExec(NetConnection* connection, const std::string& line, ExecOutput& out);
	bool EngineExec(const std::string& line, ExecOutput& out);
	// A pawn's ConsoleCommand (APlayerPawn::execConsoleCommand, Engine.dll
	// 0x103ba200): its own player's -- the viewport's, or a remote player's
	// connection's --, nothing for a pawn with none.
	std::string PawnConsoleCommand(UPlayerPawn* pawn, const std::string& line);
	// A command the engine runs itself on the viewport -- a key's line, an
	// exec file's, EXEC= --, or on the game engine -- the launcher's Open, as
	// the original's log window runs it, and any where there is no viewport
	// --, a failure in it logged and the game going on.
	void RunCommand(const std::string& command, ExecOutput* out = nullptr, bool onEngine = false);
	// A dedicated server's typed line (bugs.md D.65): on the game engine as
	// the original's log window runs one, its output logged.
	void RunTypedLine(const std::string& typed);
	// The desktop's clipboard through the window; with none, a dedicated
	// server's, text the engine holds, by choice.
	void SetClipboard(const std::string& text);
	std::string GetClipboard();
	std::string ServerClipboard;
	// The frames a second the main loop last counted, once a second, as the
	// original's MainLoop counts them into the engine (GETCURRENTTICKRATE).
	float CurrentTickRate = 0.0f;
	// The frames a second the main loop holds itself to, 0 for none
	// (CalcTimeElapsed, GETMAXTICKRATE).
	float GetMaxTickRate();
	// STAT GLOBAL's frame figures, in seconds, as the original's game engine
	// clocks them (UGameEngine::Tick, Engine.dll 0x1038fe90): the frame's
	// whole tick (0x103911ea), its game level's tick with its net
	// (0x1039006a-0x10390204), and the client's draw (0x10391198), each the
	// last frame's
	double TickStatTime = 0.0;
	double GameStatTime = 0.0;
	double ClientStatTime = 0.0;

	// The config cache flushed to disk, as the original's every SaveConfig
	// ends (Core.dll 0x10151540; bugs.md ENG.27): each ini changed since it
	// was last written, written now (PackageManager::FlushIniFiles). Where
	// the run began with no engine ini, the client's and the devices'
	// settings go into the system ini first, as at the exit, so that a run
	// that ends unclean leaves the next one an engine ini that has them.
	void FlushConfig();

	void UpdateInput(float timeElapsed);
	void ExecInputLine(const std::string& line, EInputKey key, EInputType action, int value);
	bool InputExec(const std::string& command, EInputKey key, EInputType action, int value, ExecOutput* out = nullptr);

	void LockCursor();
	void UnlockCursor();

	// An object's exec functions (the original's ScriptConsoleExec, Core.dll
	// 0x10150860), the executor the given player or none
	bool ScriptExec(UObject* target, UObject* executor, const std::string& line, ExecOutput& out);
	Array<std::string> GetArgs(const std::string& commandline);

	void UpdateAudio();

	void OpenWindow();
	// Between the window and full screen, each at the ini's size for it:
	// the console's togglefullscreen, and Enter with Alt held.
	void ToggleFullscreen();
	// The console's endfullscreen: back in a window as the toggle comes
	// back, in any state, EndFullscreen logged.
	void EndFullscreen();
	// The window's size in a window kept as the client's windowed size, its
	// width made even, and saved at once, as the original's client keeps
	// each size its window is given (bugs.md WIN.18): the tick after the
	// window's last change of size, so a drag is kept where it stops and a
	// toggle's way through its sizes, full screen by then, not at all
	void KeepWindowedSize();
	void ModeSetAgain();
	void FlushRender();
	// A size the engine asks of the window, which the window system may
	// take a moment to give: another size the window has meanwhile, the
	// full screen it is leaving among them, is not kept, for a second at
	// most
	void AskWindowSize(int width, int height);
	int windowSettleTicks = 0;
	int askedWindowWidth = 0;
	int askedWindowHeight = 0;
	double askedWindowTime = 0.0;
	void CloseWindow();
	void TickWindow();

	// The render device the run draws with (ENGINE.md, rendering): -nohard's
	// software device, as the original's; else Settings.json's Type where
	// it names one; else the device the game's ini names (GameRenderDevice):
	// the software device for SoftDrv's, the Vulkan device for any other
	RenderDeviceType ChooseRenderDevice();
	RenderDeviceType chosenRenderDevice = RenderDeviceType::Vulkan;
	// The window for it; where its GPU device does not start, the software
	// device's, the error logged
	std::unique_ptr<GameWindow> CreateGameWindow(RenderDeviceType type);

	bool Key(char16_t key);
	void InputEvent(EInputKey key, EInputType type, int delta = 0);
	void ReleaseHeldInputKeys();
	void ReleaseFireButtons();

	void OnWindowPaint() override;
	void OnWindowMouseMove(const Point& pos) override;
	void OnWindowMouseDown(const Point& pos, EInputKey key) override;
	void OnWindowMouseDoubleclick(const Point& pos, EInputKey key) override;
	void OnWindowMouseUp(const Point& pos, EInputKey key) override;
	void OnWindowMouseWheel(const Point& pos, EInputKey key) override;
	void OnWindowRawMouseMove(int dx, int dy) override;
	void OnWindowKeyChar(std::string chars) override;
	void OnWindowKeyDown(EInputKey key) override;
	void OnWindowKeyUp(EInputKey key) override;
	void OnWindowGeometryChanged() override;
	void OnWindowClose() override;
	void OnWindowActivated() override;
	void OnWindowDeactivated() override;
	void OnWindowDpiScaleChanged() override;
	void OnWindowMaximize() override;
	void OnWindowScreenSaver(bool displayPower) override;

	void SetPause(bool value);

	UZoneInfo* GetZoneActor(int zoneIndex);

	std::string ParseClassName(std::string className);

	UGameEngine* gameengine = nullptr;
	UDXEngineRenderDevice* renderdev = nullptr;
	UDXEngineAudioDevice* audiodev = nullptr;
	// -nosound or UseSound off: the original's engine makes no audio
	// subsystem then (UEngine::InitAudio, Engine.dll 0x1037fe50), so no
	// word or overlay of Galaxy's
	bool NoAudioSubsystem = false;
	UDXEngineNetworkDevice* netdev = nullptr;
	UDXEngineClient* client = nullptr;
	UViewport* viewport = nullptr;
	UCanvas* canvas = nullptr;
	UGC* dxgc = nullptr;
	UDXSaveInfo* dxSaveInfo = nullptr;
	UConsole* console = nullptr;
	URootWindow* dxRootWindow = nullptr;

	UFloatProperty* floatprop = nullptr;
	// PlayerCalcView's three out parameters, as properties (Run).
	UObjectProperty* objprop = nullptr;
	UStructProperty* vecprop = nullptr;
	UStructProperty* colorprop = nullptr; // a Color out argument's (CursorRequested); its Struct from the event's own
	UStructProperty* rotprop = nullptr;

	ULevelInfo* EntryLevelInfo = nullptr;
	ULevel* EntryLevel = nullptr;
	UGameInfo* EntryGameInfo = nullptr;
	Package* EntryLevelPackage = nullptr;
	UDeusExLevelInfo* EntryDeusExLevelInfo = nullptr;

	double TotalTime = 0.0;

	ULevelInfo* LevelInfo = nullptr;
	ULevel* Level = nullptr;
	Package* LevelPackage = nullptr;
	UGameInfo* GameInfo = nullptr;
	UTexture* DefaultTexture = nullptr;

	Package* deusExPackage = nullptr;
	UDeusExLevelInfo* DeusExLevelInfo = nullptr;
	bool dxTravelUsesCurrent = false; // set by DeusExPreTravel, read once by LoadMap
	bool levelFromCurrent = false; // set by LoadMap: its level came from Current
	// The render device's PrecacheOnFlip: set by a map's load with its
	// UsePrecache on, cleared by the frame that shows PRECACHING
	bool PrecacheOnFlip = false;

	std::unique_ptr<NetPendingLevel> PendingLevel;
	std::unique_ptr<NetDriver> LevelNetDriver;
	std::unique_ptr<NetClientLevel> ClientLevel;
	std::unique_ptr<NetServerLevel> ServerLevel;
	std::string NetFailure;
	// The URL the level was loaded with, as given (the original's
	// UGameEngine::LastURL): what a restart, a server's travel and reconnect
	// go by, where the level's own URL also gets a listen server's address.
	UnrealURL LastURL;
	struct
	{
		UnrealURL URL;
		bool TransferItems = false;
	} ClientTravelInfo;

	struct
	{
		int32_t SaveGameSlot = DONT_SAVE_GAME;
		std::string SaveGameDescription;
	} SaveGameInfo;

	GameLaunchInfo LaunchInfo;
	std::unique_ptr<PackageManager> packages;
	std::unique_ptr<GameWindow> window;
	std::unique_ptr<RenderSubsystem> render;

	int MouseMoveX = 0;
	int MouseMoveY = 0;

	float CalcTimeElapsed();

	// A save asked for, written: after the levels' tick, before a travel
	// takes the level, and after the draw.
	void WritePendingSave();
	// The game engine's travels, after the levels' tick as the original's
	// (UGameEngine::Tick, Engine.dll 0x1038fe90): a server's next map, then
	// a client's travel; true when one was taken, loaded or not.
	bool TakeTravel(float realTimeElapsed);

	// The console's Tick, by the time since the viewport was last drawn, as
	// the original's UConsole::PreRender gives it at each frame's draw, the
	// loading screen's too; the first, by the frame's time.
	void TickConsole(float firstElapsed);
	uint64_t consoleLastTick = 0;

	UActor* CameraActor = nullptr;
	vec3 CameraLocation = vec3(0.0f);
	Rotator CameraRotation = Rotator(0,0,0);
	float CameraFovAngle = 95.0f;


	bool quit = false;

	// The command line a RELAUNCH named, for GameApp to start the engine
	// again with once it has shut down; empty when there is none.
	std::string RelaunchLine;

	uint64_t lastTime = 0;

	void LoadEngineSettings();

	void LoadKeybindings();
	// Key and alias names in any case, as the original's, FNames
	struct NameLess
	{
		bool operator()(const std::string& a, const std::string& b) const;
	};
	std::map<std::string, std::string, NameLess> keybindings;
	std::map<std::string, std::string, NameLess> inputAliases;
	static const char* keynames[256];

	std::map<std::string, EInputKey> activeInputButtons;
	// The keys the input holds down, each pressed once per hold and released
	// only after its press, as the original's XInputExt runs them
	std::set<EInputKey> heldInputKeys;
	// An alias's lines running: an alias inside one is not expanded again
	bool inputAliasRunning = false;
	// Each held key's own value of an axis, by the axis and the key: the
	// axis is their sum each tick, as the original's held bindings add into
	// it (engine-dll.md, a key's binding)
	std::map<std::pair<std::string, EInputKey>, float> activeInputAxes;
	// The player's input read in its tick, before PlayerInput (ReadInput)
	void ReadInput(UPlayerPawn* player, float elapsed);

	GamepadInput gamepad;

	bool getEditorMode() const { return m_EditorMode; }
	void setEditorMode(const bool value) { m_EditorMode = value; }

	bool getDXWindowDebugMode() const { return m_DrawDebugDXWindowHierarchy; }

private:
	std::map<std::string, std::string> CreateServerTravelInfo();
	std::map<std::string, std::string> CreateClientTravelInfo(bool transferItems);
	// An arriving player's travel, a local or a remote one's, between its
	// own TravelPreAccept and TravelPostAccept.
	void AcceptTravel(UPlayerPawn* pawn, const UnrealURL& url);

	// What the original's command line changes for this run alone
	// (OriginalCommandLine): -defaultres's 640x480 and the window -noddraw
	// leaves the software device. What each replaced goes back before the
	// client's settings are saved, unless the player changed it since.
	void ApplyRunOnlySettings();
	void RestoreRunOnlySettings();
	// The client's config saved, what the original's command line set for
	// the run saved as it was configured, the run keeping its own
	void SaveClientConfig(bool asResized = true);
	// The client's and the devices' settings read (LoadEngineSettings): no
	// config flushed before
	bool engineSettingsLoaded = false;
	// -noddraw's window, for the software device chosen or drawing in the
	// place of a GPU device that did not start
	void ApplyNoDDraw();
	struct RunOnlySetting
	{
		bool Active = false;
		int Configured = 0;
		int ForTheRun = 0;
	};
	RunOnlySetting runWindowedX, runWindowedY, runFullscreenX, runFullscreenY, runStartupFullscreen;

	// The console's EXEC: each line of a file run on the viewport, its output
	// the command's.
	void ExecFile(const std::string& filename, ExecOutput& out);
	// What the launcher sends: its TakeFocus and Open (LauncherLine).
	void OnLauncherLine(const std::string& line);
	// RELAUNCH: line the text past the word
	void Relaunch(const std::string& line);

	// The command layers' parts, in their order (ViewportExec, PlayerExec,
	// EngineExec): the render device's words, the viewport's own, the
	// window's; a level's; Deus Ex's game engine's and the game engine's own;
	// then the engine's: the system's, the objects', the launcher's, the
	// client's and the renderer's.
	bool RenderDeviceExec(const std::string& line, ExecOutput& out);
	bool ViewportWordExec(UPlayerPawn* actor, const std::string& line, ExecOutput& out);
	bool WindowExec(const std::string& line, ExecOutput& out);
	bool LevelExec(ULevel* level, const std::string& line, ExecOutput& out);
	bool GameEngineExec(const std::string& line, ExecOutput& out);
	bool EngineWordExec(const std::string& line, ExecOutput& out);
	bool SystemExec(const std::string& line, ExecOutput& out);
	bool ObjectExec(const std::string& line, ExecOutput& out);
	bool LauncherExec(const std::string& line, ExecOutput& out);
	bool RendererExec(const std::string& line, ExecOutput& out);
	// A dedicated server's OPEN, START, DISCONNECT and RECONNECT: with no
	// viewport, the original's game engine browses at once, no PreTravel
	// sent (UGameEngine::Exec, Engine.dll 0x1038a030); here the frame's
	// travel takes the URL, where no script is running; false, the error
	// given, for one it refuses at once.
	bool ServerBrowse(const std::string& text, ETravelType travelType, std::string& error);
	// OBJ MARK's objects, which OBJ MARKCHECK leaves out
	std::set<UObject*> markedObjects;

	void LogGamePackageSHA1Sums() const;
	void GetLevelInfoObject();
	void GetLevelObject();
	void LinkActorsToLevel();
	void BeginPlay(const UnrealURL& url);
	void CallPostPostBeginPlay();

	bool m_EditorMode = false; // Set this to true to allow rendering of invisible polys.
	bool m_GamePaused = false;

	bool m_DrawDebugDXWindowHierarchy = false; // If set to true, engine will show the Deus Ex window hierarchy
};

extern Engine* engine;
