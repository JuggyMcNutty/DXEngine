//=============================================================================
// ConSaveConsole: a level left while a conversation plays, and entered again
// from Current, logged with "DXCONSAVE:" (bugs.md PKG.38).
//
// From the menu map: Battery Park opened; 3 s in, each ConPlay there logged
// (its conversation and whether it is going), then the street opened, and
// from it Battery Park again, by plain travels, the level left saved into
// Current. Battery Park's second entry logs its time, which goes on from
// the first's where Current's save was loaded, and its ConPlays again. A
// ConPlay holds the conversation's camera, which ConSys makes in package
// DeusEx (ConSys.dll 0x100106f0): a save holding one names it as an import.
// Remove Current after the run.
//=============================================================================
class ConSaveConsole extends Console;

var float LevelTime;
var float LastSeconds;
var string LastLevel;
var int Step;

function LogConPlays(PlayerPawn P)
{
	local ConPlay C;
	local int Count;

	foreach P.AllActors(class'ConPlay', C)
	{
		Log("DXCONSAVE: " $ C $ " conversation " $ C.con $ " deleted " $ C.bDeleteMe $ " state " $ C.GetStateName());
		Count++;
	}
	Log("DXCONSAVE: " $ Count $ " ConPlay");
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

	Log("DXCONSAVE: step " $ Step $ ": " $ P.Level.GetLocalURL() $ " at " $ P.Level.TimeSeconds);
	if (Step == 0)
		P.ConsoleCommand("open 02_NYC_BatteryPark");
	else if (Step == 1)
	{
		LogConPlays(P);
		P.ConsoleCommand("open 02_NYC_Street");
	}
	else if (Step == 2)
		P.ConsoleCommand("open 02_NYC_BatteryPark");
	else
	{
		LogConPlays(P);
		Log("DXCONSAVE: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	}
	Step++;
}
