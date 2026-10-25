//=============================================================================
// DeleteSaveConsole: DELETEGAME on the quick save -- Liberty Island opened from
// wherever the run starts; 2 s into its time the quick save, as the
// player's QuickSave makes it, by a SaveHelper on the actors' tick, 4 s later "DeleteGame -1", as the Save Game screen's Delete
// sends on the quick save's row, then an exit 2 s on. The quick save's
// folder after the run is the proof (Save/QuickSave): the original's
// DeleteGame removes Save%04d alone, so the quick save stays; DXEngine's
// once removed the quick save for -1, and Current for -2 (bugs.md
// E15b.11). Each line starts "DXDELSAVE:".
//=============================================================================
class DeleteSaveConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	// The name after the last / or \: a level from Current names its file
	// in the original (..\Save\Current\<map>.dxs).
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
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXDELSAVE: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && MapTime > 2.0)
	{
		// The quick save as the player's QuickSave makes it, from an actor's
		// tick (SaveHelper): the original crashes on a save from its
		// console's tick.
		Log("DXDELSAVE: the quick save on " $ CurrentMap);
		S = P.Spawn(class'SaveHelper', P);
		S.Slot = -1;
		S.Description = DeusExPlayer(P).QuickSaveGameTitle;
		Step = 1;
	}
	else if (Step == 1 && MapTime > 6.0)
	{
		Log("DXDELSAVE: DeleteGame -1");
		P.ConsoleCommand("DeleteGame -1");
		Step = 2;
	}
	else if (Step == 2 && MapTime > 8.0)
	{
		Log("DXDELSAVE: exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
