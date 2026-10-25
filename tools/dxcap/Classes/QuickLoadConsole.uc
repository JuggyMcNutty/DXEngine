//=============================================================================
// QuickLoadConsole: a quick save after a map exit, then a quick load --
// Liberty Island opened from wherever the run starts; 2 s into its time
// UNATCO HQ opened (the same mission: the island saved into Current); 3 s
// into HQ the quick save, by a SaveHelper on the actors' tick; 3 s later
// the player's LoadGame -1 (QuickLoad's, confirmed); on the loaded level, 2 s in, a line that it
// loaded, and an exit. A Windows build once stopped at the load with
// Current half emptied: the save scan and the load kept the SaveInfo files
// open, sharing reads alone, and the load's deletes failed on them (bugs.md
// E2.13). Each line starts "DXQL:".
//=============================================================================
class QuickLoadConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;
var bool bLoaded;
var PlayerPawn Before; // the player before the load: the loaded level has its own, and the load's collection makes this None
var bool bBefore;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, "\\");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "\\");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local SaveHelper S;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	// The quick load gives the same map, the same level name, even the same
	// pawn name again (the saved level carries its player): what tells the
	// load is the player before it, gone -- the collection makes the
	// reference None.
	if (Step == 3 && !bLoaded && bBefore && Before == None)
	{
		bLoaded = true;
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXQL: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && MapTime > 2.0)
	{
		Log("DXQL: on " $ CurrentMap $ ", opening 01_NYC_UNATCOHQ");
		P.ConsoleCommand("open 01_NYC_UNATCOHQ");
		Step = 1;
	}
	else if (Step == 1 && (CurrentMap ~= "01_NYC_UNATCOHQ") && MapTime > 3.0)
	{
		Log("DXQL: the quick save on " $ CurrentMap);
		S = P.Spawn(class'SaveHelper', P);
		S.Slot = -1;
		S.Description = DeusExPlayer(P).QuickSaveGameTitle;
		Step = 2;
	}
	else if (Step == 2 && MapTime > 6.0)
	{
		Log("DXQL: LoadGame -1, the quick load confirmed");
		Before = P;
		bBefore = true;
		P.ConsoleCommand("LoadGame -1");
		Step = 3;
	}
	else if (Step == 3 && bLoaded && MapTime > 2.0)
	{
		Log("DXQL: loaded, on " $ CurrentMap $ " as " $ P $ ": the player before the load is gone");
		Step = 4;
	}
	else if (Step == 4 && MapTime > 3.0)
	{
		Log("DXQL: exiting");
		P.ConsoleCommand("exit");
		Step = 5;
	}
}
