//=============================================================================
// HubConsole: the hub stack's travels, ?push, ?peer and ?pop, logged with
// "DXHUB:" (docs/DEVELOPMENT.md, scripted runs; bugs.md PKG.39).
//
// From the menu map: Liberty Island opened, then, 3 s into each level, the
// level's URL and its LevelInfo's HubStackLevel logged and the next travel
// asked by the console's open:
//   1 the island            open 01_NYC_UNATCOHQ?push       (same mission)
//   2 UNATCO HQ             open 02_NYC_BatteryPark?peer    (a new mission)
//   3 Battery Park          open 02_NYC_Street?push         (same mission)
//   4 the street            open 02_NYC_Bar?pop             (same mission)
//   5 the bar               open 02_NYC_BatteryPark?pop     (from Current)
//   6 Battery Park again    exit
// The original's UGameEngine::LoadMap (Engine.dll 0x1038c1f0) saves the
// level left on a ?push into <SavePath>\Game%04i.dxs, numbered by the level
// left's HubStackLevel (0x1038c825-0x1038c88c), and gives the new level
// that level plus one on ?push, less one (not under 0) on ?pop, the same on
// ?peer, its file's own with an option starting "load", and 0 for any other
// travel (0x1038e5b0 on); a level that comes from Current is entered by a
// URL without PUSH, POP, PEER or LOAD (FURL 0x103f7f31-0x103f7faf), so 0.
// Expected: the original's HubStackLevel 0, 1, 1, 2, 1, 0 at steps 1 to 6,
// and gamefiles/Save/Game0000.dxs (the island) and Game0001.dxs (Battery
// Park) after the run; DXEngine's 0 throughout (Engine.cpp:1237), no file.
// Remove the two Game*.dxs files and Current after the run.
//=============================================================================
class HubConsole extends Console;

var float LevelTime;
var float LastSeconds;
var string LastLevel;
var int Step;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Step == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (string(P.Level) != LastLevel || P.Level.TimeSeconds < LastSeconds)
		LevelTime = 0;
	LastLevel = string(P.Level);
	LastSeconds = P.Level.TimeSeconds;
	LevelTime += Delta;
	if (LevelTime < 3.0)
		return;
	LevelTime = -1000;

	Log("DXHUB: step " $ Step $ ": " $ P.Level.GetLocalURL() $ ", HubStackLevel " $ P.Level.HubStackLevel);
	if (Step == 0)
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
	else if (Step == 1)
		P.ConsoleCommand("open 01_NYC_UNATCOHQ?push");
	else if (Step == 2)
		P.ConsoleCommand("open 02_NYC_BatteryPark?peer");
	else if (Step == 3)
		P.ConsoleCommand("open 02_NYC_Street?push");
	else if (Step == 4)
		P.ConsoleCommand("open 02_NYC_Bar?pop");
	else if (Step == 5)
		P.ConsoleCommand("open 02_NYC_BatteryPark?pop");
	else
	{
		Log("DXHUB: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	}
	Step++;
}
