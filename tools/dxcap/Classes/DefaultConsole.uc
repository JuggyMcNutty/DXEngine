//=============================================================================
// DefaultConsole: what a map exit writes of a travelled item's travel
// properties, logged with "DXDEFAULT:" (docs/DEVELOPMENT.md, scripted runs).
// On Liberty Island, opened from wherever the run starts, 3 s in: a
// CapTravelItem given to the player, its spawn setting its two travel
// properties from their defaults, 1, to 2; then Kept set back to its
// default and Changed to 3, logged, and the island left for UNATCO HQ as a
// map exit leaves it (ClientTravel, TRAVEL_Relative, with the items). 3 s
// into UNATCO HQ the item the player carries is logged and the run exits.
// The original's export writes a value only where it differs from its
// class's default (Engine.dll 0x10391894): Kept arrives as the new item's
// spawn set it, 2, and Changed as written, 3.
//=============================================================================
class DefaultConsole extends Console;

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

function Carried(PlayerPawn P, string Label)
{
	local CapTravelItem Item;
	local Inventory Inv;
	local int Count;

	for (Inv = P.Inventory; Inv != None; Inv = Inv.Inventory)
	{
		Item = CapTravelItem(Inv);
		if (Item != None)
		{
			Count++;
			Log("DXDEFAULT: " $ Label $ ": the player carries " $ Item.Name $ ", Kept " $ Item.Kept $ ", Changed " $ Item.Changed);
		}
	}
	if (Count == 0)
		Log("DXDEFAULT: " $ Label $ ": the player carries no CapTravelItem");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local CapTravelItem Item;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
		Log("DXDEFAULT: in " $ CurrentMap);
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 3.0)
	{
		Log("DXDEFAULT: opening 01_NYC_UNATCOIsland");
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		Step = 1;
	}
	else if (Step <= 1 && (CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 3.0)
	{
		Item = P.Spawn(class'CapTravelItem', P);
		if (Item == None)
		{
			Log("DXDEFAULT: no item spawned, exiting");
			P.ConsoleCommand("exit");
			Step = 9;
			return;
		}
		Item.GiveTo(P);
		Item.Kept = 1;
		Item.Changed = 3;
		Carried(P, "island");
		Log("DXDEFAULT: travelling to 01_NYC_UNATCOHQ with the items");
		P.ClientTravel("01_NYC_UNATCOHQ", TRAVEL_Relative, True);
		Step = 2;
	}
	else if (Step == 2 && (CurrentMap ~= "01_NYC_UNATCOHQ") && MapTime > 3.0)
	{
		Carried(P, "HQ");
		Log("DXDEFAULT: exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
