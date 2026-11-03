//=============================================================================
// CaseTravelConsole: a travel string
// that differs from its class's default only in case, across a map exit,
// logged with "DXCASE:". On Liberty Island, opened from wherever the run
// starts, 3 s in: the player's TruePlayerName (a travel string of
// DeusExPlayer) set to its default in capitals, logged, and the island left
// for UNATCO HQ as a map exit leaves it (ClientTravel, TRAVEL_Relative,
// with the items). 3 s into UNATCO HQ TruePlayerName is logged and the run
// exits. The original's export writes a value only where it is not
// Identical to the default, and UStrProperty::Identical is appStricmp
// (Core.dll 0x101676f0): the name arrives as the new player's default.
// DXEngine's compare is exact (ObjectTravelInfo.cpp, CompareElement).
//=============================================================================
class CaseTravelConsole extends Console;

var int Step;
var float MapTime;
var string CurrentMap;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
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
	return U;
}

event Tick(float Delta)
{
	local DeusExPlayer P;

	Super.Tick(Delta);
	if (Viewport == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = DeusExPlayer(Viewport.Actor);
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
		Log("DXCASE: in " $ CurrentMap);
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 3.0)
	{
		Log("DXCASE: opening 01_NYC_UNATCOIsland");
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		Step = 1;
	}
	else if (Step <= 1 && (CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 3.0)
	{
		Log("DXCASE: island: TruePlayerName [" $ P.TruePlayerName $ "], its default [" $ P.default.TruePlayerName $ "]");
		P.TruePlayerName = Caps(P.default.TruePlayerName);
		Log("DXCASE: island: set to [" $ P.TruePlayerName $ "], travelling to 01_NYC_UNATCOHQ");
		P.ClientTravel("01_NYC_UNATCOHQ", TRAVEL_Relative, True);
		Step = 2;
	}
	else if (Step == 2 && (CurrentMap ~= "01_NYC_UNATCOHQ") && MapTime > 3.0)
	{
		Log("DXCASE: HQ: TruePlayerName [" $ P.TruePlayerName $ "]");
		Log("DXCASE: exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
