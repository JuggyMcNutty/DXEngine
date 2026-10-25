//=============================================================================
// TravelConsole: whom a travel's PreTravel reaches -- Liberty Island opened
// from wherever the run starts; 2 s into its time a CapTravelMarker spawned
// (an Info with a PreTravel function) and UNATCO HQ opened; on HQ, an exit.
// The original's SetClientTravel sends PreTravel to every actor of the level
// that has the function, the mission scripts among them, so the marker
// logs; DXEngine once sent it to the travelling player alone (bugs.md
// E4a.20). Each line starts "DXTRAVEL:".
//=============================================================================
class TravelConsole extends Console;

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

event Tick(float Delta)
{
	local PlayerPawn P;
	local CapTravelMarker M;

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
			Log("DXTRAVEL: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && MapTime > 2.0)
	{
		M = P.Spawn(class'CapTravelMarker');
		Log("DXTRAVEL: marker " $ M $ " spawned, opening 01_NYC_UNATCOHQ");
		P.ConsoleCommand("open 01_NYC_UNATCOHQ");
		Step = 1;
	}
	else if (Step == 1 && (CurrentMap ~= "01_NYC_UNATCOHQ") && MapTime > 2.0)
	{
		Log("DXTRAVEL: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
