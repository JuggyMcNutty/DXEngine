//=============================================================================
// LightStartConsole: the light the AI sees by before a map's first frame --
// Liberty Island opened from wherever the run starts; 3 s in, a CapLightItem
// given to the player and the island left for UNATCO HQ as a map exit
// leaves it (ClientTravel, TRAVEL_Relative, with the items). The item logs
// AIGetLightLevel at the player's place and at four Light actors' places in
// its TravelPostAccept and at its first six ticks on HQ ("DXAILIGHT:"): in
// the original every line of a place alike, the player standing still; in
// DXEngine the TravelPostAccept's and the first tick's lines the zone's
// ambient alone, the tree of lights not yet built, the later ones the
// original's. 3 s into HQ the console's own probe of the player's place,
// then an exit.
//=============================================================================
class LightStartConsole extends Console;

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
	local CapLightItem Item;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
		Log("DXAILIGHT: in " $ CurrentMap);
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 3.0)
	{
		Log("DXAILIGHT: opening 01_NYC_UNATCOIsland");
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		Step = 1;
	}
	else if (Step <= 1 && (CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 3.0)
	{
		Item = P.Spawn(class'CapLightItem', P);
		if (Item == None)
		{
			Log("DXAILIGHT: no item spawned, exiting");
			P.ConsoleCommand("exit");
			Step = 9;
			return;
		}
		Item.GiveTo(P);
		Log("DXAILIGHT: travelling to 01_NYC_UNATCOHQ with the item; island light " $ P.AIGetLightLevel(P.Location));
		P.ClientTravel("01_NYC_UNATCOHQ", TRAVEL_Relative, True);
		Step = 2;
	}
	else if (Step == 2 && (CurrentMap ~= "01_NYC_UNATCOHQ") && MapTime > 3.0)
	{
		Log("DXAILIGHT: console " $ P.Level.TimeSeconds $ " player " $ P.AIGetLightLevel(P.Location));
		Log("DXAILIGHT: exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
