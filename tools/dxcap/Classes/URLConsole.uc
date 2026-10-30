//=============================================================================
// URLConsole: the level's URL as Level.GetLocalURL gives it, in both engines
// (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, saving, loading and
// travel). From the menu map: Liberty Island opened, then a map exit's
// travel to UNATCO HQ (ClientTravel, TRAVEL_Relative, with the items), then
// Liberty Island opened again, which comes back from Current, then the
// player's RestartLevel there, then a map exit's travel to UNATCO HQ again.
// 3 s into each level the URL is logged with "DXURL:" in front, and the
// player's inventory count; then an exit. A new level is told by its
// LevelInfo's name as text, or for the restart's same map by its clock
// starting again: the console holds no actor of a level, which would keep
// the level loaded past the next one's collection, and the original's game
// engine takes the first DeusExLevelInfo it finds for the current level's
// (its mission number, the name it saves the level under).
//=============================================================================
class URLConsole extends Console;

var float LevelTime;
var float LastSeconds;
var string LastLevel;
var int Step;

function int InventoryCount(PlayerPawn P)
{
	local Inventory I;
	local int n;

	for (I = P.Inventory; I != None; I = I.Inventory)
		n++;
	return n;
}

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

	Log("DXURL: step " $ Step $ ": " $ P.Level.GetLocalURL() $ ", " $ InventoryCount(P) $ " items");
	if (Step == 0)
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
	else if (Step == 1)
		P.ClientTravel("01_NYC_UNATCOHQ", TRAVEL_Relative, true);
	else if (Step == 2)
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
	else if (Step == 3)
		P.ConsoleCommand("RestartLevel");
	else if (Step == 4)
		P.ClientTravel("01_NYC_UNATCOHQ", TRAVEL_Relative, true);
	else
	{
		Log("DXURL: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	}
	Step++;
}
