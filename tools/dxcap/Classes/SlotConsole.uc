//=============================================================================
// SlotConsole: the game's SaveGame with a negative slot, logged with
// "DXSLOT:" (docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island, 2 s in: the player's SaveGame(-3), as the console's
// `savegame -3` saves, made from an actor's tick (SaveHelper: the original
// crashes on a save from its console's tick), 3 s later SaveGame(-2), 3 s
// later an exit, each call logged as it is made. What is compared
// between the engines: the game's Save folder after each run -- Save-003
// holding a SaveInfo and the level, Save-002 a SaveInfo with the level
// saved into Current, as the original's SaveGame and SaveCurrentLevel put
// them; the run ends at its own exit. Remove the two folders after.
//=============================================================================
class SlotConsole extends Console;

var float RunTime;
var int Phase;

function Say(string S)
{
	Log("DXSLOT: " $ S);
}

event Tick(float Delta)
{
	local SaveHelper S;

	Super.Tick(Delta);
	if (Phase == 3 || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Say("SaveGame(-3)");
		S = Viewport.Actor.Spawn(class'SaveHelper', Viewport.Actor);
		S.Slot = -3;
		S.Description = "slot -3";
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 5.0)
	{
		Say("SaveGame(-2)");
		S = Viewport.Actor.Spawn(class'SaveHelper', Viewport.Actor);
		S.Slot = -2;
		S.Description = "slot -2";
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 8.0)
	{
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 3;
	}
}
