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

class Engine : public GameWindowHost
{
public:
	Engine(GameLaunchInfo launchinfo);
	~Engine();

	void Run();
	void ClientTravel(const std::string& URL, ETravelType travelType, bool transferItems);
	UnrealURL GetDefaultURL(const std::string& map);
	// The player's options in the URL: Deus Ex's every key of the user ini's
	// [DefaultPlayer] as the ini spells them, in its order, a key given twice
	// its last value, as the original's FURL::LoadURLConfig adds the section
	// (Engine.dll 0x103f9bc0).
	Array<std::string> DefaultPlayerOptionKeys();
	void SetMultiplayerName(UnrealURL& url);
	void LoadEntryMap();
	// The Entry level made the level played, a new player spawned in it, as
	// the original's browse to ?failed or ?entry: a lost server, or one that
	// refused the player after the join, sends a client there.
	void ReturnToEntry(const UnrealURL& url);
	void LoadMap(const UnrealURL& url, const std::map<std::string, std::string>& travelInfo = {}, bool asClient = false);
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
	// Whether a travel's map is there to load; where it is not, the browse
	// fails as the original's does, the level playing on.
	bool BrowseFinds(const UnrealURL& url);
	// A travel to a map as the original's Browse makes it: the pre-travel,
	// then the load, or a missing map's fallback; false when no level loads.
	bool BrowseMap(const UnrealURL& url, const std::map<std::string, std::string>& travelInfo);
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
	// and map, for a minute at a time (the original's each frame).
	void UpdateConnectingMessage();
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

	std::string ConsoleCommand(UObject* context, const std::string& command, BitfieldBool& found);
	// A command the engine runs itself -- a key's line, an exec file's, the
	// launcher's Open --, a failure in it logged and the game going on.
	void RunCommand(UObject* context, const std::string& command);

	void UpdateInput(float timeElapsed);
	void ExecInputLine(const std::string& line, EInputKey key, EInputType action, int value);
	bool InputExec(const std::string& command, EInputKey key, EInputType action, int value);

	void LockCursor();
	void UnlockCursor();

	bool ExecCommand(const Array<std::string>& args, const std::string& line = {});
	Array<std::string> GetArgs(const std::string& commandline);
	Array<std::string> GetSubcommands(const std::string& commandline);

	void UpdateAudio();

	void OpenWindow();
	// Between the window and full screen, each at the ini's size for it:
	// the console's togglefullscreen, and Enter with Alt held.
	void ToggleFullscreen();
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

	void SetPause(bool value);

	UZoneInfo* GetZoneActor(int zoneIndex);

	std::string ParseClassName(std::string className);

	UGameEngine* gameengine = nullptr;
	UDXEngineRenderDevice* renderdev = nullptr;
	UDXEngineAudioDevice* audiodev = nullptr;
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
		ETravelType TravelType = ETravelType::TRAVEL_Absolute;
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
	std::map<std::string, std::string> CreateTravelInfo(bool transferItems);

	// What the original's command line changes for this run alone
	// (OriginalCommandLine): -defaultres's 640x480 and the window -noddraw
	// leaves the software device. What each replaced goes back before the
	// client's settings are saved, unless the player changed it since.
	void ApplyRunOnlySettings();
	void RestoreRunOnlySettings();
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

	// The console's EXEC: each line of a file run as a console command.
	void ExecFile(UObject* context, const std::string& filename);
	// What the launcher sends: its TakeFocus and Open (LauncherLine).
	void OnLauncherLine(const std::string& line);
	void Relaunch(const std::string& commandline);

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
