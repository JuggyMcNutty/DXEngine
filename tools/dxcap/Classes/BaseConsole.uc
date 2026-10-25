//=============================================================================
// BaseConsole: what the map's actors stand on at its start -- Liberty Island
// opened from wherever the run starts; 3 s in, every decoration, inventory
// item and pawn with PHYS_None or PHYS_Rotating, in the level's order: its
// base and how many stand on it; then an exit. The original's map load finds
// each one's base by a short trace down (movers and actors included);
// DXEngine once took the first actor it overlapped, in heap order, itself
// among them (bugs.md E6.5, E3E7.19). Each line starts "DXBASE:".
//=============================================================================
class BaseConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;

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

function string BaseOf(Actor A)
{
	if (A.Base == None)
		return "None";
	if (A.Base == A.Level)
		return "the level";
	return string(A.Base.Name);
}

function Census(PlayerPawn P)
{
	local Actor A;
	local int N;

	foreach P.AllActors(class'Actor', A)
	{
		if (A == P)
			continue;
		if (Decoration(A) == None && Inventory(A) == None && Pawn(A) == None)
			continue;
		if (A.Physics != PHYS_None && A.Physics != PHYS_Rotating)
			continue;
		Log("DXBASE: " $ A.Name $ " on " $ BaseOf(A) $ ", standing on it " $ A.StandingCount $ ", at " $ A.Location);
		N++;
	}
	Log("DXBASE: " $ N $ " actors listed");
}

event Tick(float Delta)
{
	local PlayerPawn P;

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
			Log("DXBASE: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && MapTime > 3.0)
	{
		Census(P);
		Step = 1;
	}
	else if (Step == 1 && MapTime > 4.0)
	{
		Log("DXBASE: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
