//=============================================================================
// ReadOnlyConsole: settings saved into inis the run cannot write, then an
// exit (bugs.md ENG.25), logged with "DXREADONLY:". Run with
// DXCAP_READONLY=1, the run's two inis read-only. From wherever the run
// starts, 2 s in: the system ini's [Engine.Engine] CacheSizeMegs as the
// config cache holds it (GetConfig) and MouseSensitivity, PlayerPawn's
// default and the player's own; then `set Engine CacheSizeMegs 5`, a save
// of the system ini, and the player's MouseSensitivity 4.25 saved
// (SaveConfig), a save of the user ini, and the three again; a second later
// the three a last time, then an exit. The original's SaveConfig ends with
// the config cache's Flush (Core.dll 0x10151540), whose write of a file it
// cannot open fails quietly (GOG DeusEx.exe 0x10915760, 0x109067b0; Core.dll
// 0x10148c40), the values kept in memory: after the saves CacheSizeMegs is
// 5 and the default MouseSensitivity 4.25, the config read back from the
// cache, and neither file changes. Compared: the lines, each engine's exit
// and the run's readonly-before.txt and readonly-after.txt
// (docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class ReadOnlyConsole extends Console;

var float RunTime;
var int Phase;

function LogValues(string When)
{
	Log("DXREADONLY: " $ When $ ": CacheSizeMegs |" $ GetConfig("Engine.Engine", "CacheSizeMegs")
		$ "| MouseSensitivity " $ class'PlayerPawn'.default.MouseSensitivity
		$ ", the player's " $ Viewport.Actor.MouseSensitivity);
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Phase == 2 || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		LogValues("before");
		Viewport.Actor.ConsoleCommand("set Engine CacheSizeMegs 5");
		Viewport.Actor.MouseSensitivity = 4.25;
		Viewport.Actor.SaveConfig();
		LogValues("saved");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		LogValues("exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 2;
	}
}
