//=============================================================================
// SaveConsole: a save for the other engine to load -- Liberty Island opened
// from wherever the run starts; 8 s in, a LoadMarker is spawned, the player
// is given a goal, two notes (the second's text past Latin-1: U+0100, U+03A9
// and U+2122, saved as 16-bit characters) and a nano key as the game gives them (AddGoal,
// AddNote, the key ring's GiveKey: each new(Self) on the player) and two HUD
// messages (a log object each, CreateLogObject), and what the level holds
// is logged (the player, its place and inventory, its goals, notes, keys
// and logs, the game, how many actors, pawns and inventory items, the
// mission script, the marker's PostPostBeginPlay calls, and the game's
// replication info's own int Region and the Region it has as an Actor, a
// struct, which the original's load skips: Core.dll 0x10126730 finds the
// class's own Region first, another type, and logs "Type mismatch in Region
// of GameReplicationInfo: file 10, class 2" (bugs.md PKG.26)), then the game
// is saved to slot 9 (by a SaveHelper, on the next actors' tick) and the run
// exits 3 s later. LoadConsole loads that slot and logs the same: no save
// holds the mission script, and the loaded player's TravelPostAccept spawns
// it again. With Via (DXCAP_CONFIG "Via=<map>", for LoadConsole too) the
// island is left as a map exit leaves it, with the items, and the marker,
// the census and the save are made 8 s into that map, where all three
// arrived by travel. With Fresh (DXCAP_CONFIG "Fresh=True") the island is a new
// game's, the game's own StartNewGame sending the player there from the menu
// map, and it is saved 8 s in as it is, nothing given or spawned: the start
// CensusConsole loads, saved without cheats.
//=============================================================================
class SaveConsole extends Console;

var config string Via;
var config bool Fresh;
var float MapTime;
var int Step;
var string CurrentMap;


// A string's characters as their codes, the log's being 8-bit: its length,
// then each one's code
function string Codes(string S)
{
	local string R;
	local int i;

	R = Len(S) $ ":";
	for (i = 0; i < Len(S); i++)
	{
		if (i > 0)
			R = R $ ",";
		R = R $ Asc(Mid(S, i, 1));
	}
	return R;
}

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	// The options first: the original's level keeps the URL it was saved
	// with, a path for one loaded from a save (..\Save\Current\<map>.dxs).
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
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

// What the level holds, logged with "DXSAVE:" in front.
function Census(PlayerPawn P, string Label)
{
	local Actor A;
	local Inventory Inv;
	local LoadMarker M;
	local MissionScript S;
	local DeusExPlayer DP;
	local DeusExGoal Goal;
	local DeusExNote Note;
	local NanoKeyInfo Key;
	local DeusExLog LogEntry;
	local GameReplicationInfo GRI;
	local Actor GRIActor;
	local int Actors, Pawns, Items;
	local string Carried;

	foreach P.AllActors(class'Actor', A)
	{
		Actors++;
		if (Pawn(A) != None)
			Pawns++;
		if (Inventory(A) != None)
			Items++;
	}
	for (Inv = P.Inventory; Inv != None; Inv = Inv.Inventory)
		Carried = Carried $ " " $ Inv.Class.Name;
	Log("DXSAVE: " $ Label $ " in " $ MapOf(P) $ ": player " $ P.Class.Name $ " at " $ P.Location $ " health " $ P.Health);
	Log("DXSAVE: " $ Label $ " carries" $ Carried);
	DP = DeusExPlayer(P);
	if (DP != None)
	{
		for (Goal = DP.FirstGoal; Goal != None; Goal = Goal.next)
			Log("DXSAVE: " $ Label $ " goal " $ string(Goal) $ " " $ Goal.goalName $ " primary " $ Goal.bPrimaryGoal
				$ " completed " $ Goal.bCompleted $ ": " $ Goal.text);
		for (Note = DP.FirstNote; Note != None; Note = Note.next)
			Log("DXSAVE: " $ Label $ " note " $ string(Note) $ " user " $ Note.bUserNote $ ": " $ Note.text $ " (codes " $ Codes(Note.text) $ ")");
		for (Key = DP.KeyList; Key != None; Key = Key.NextKey)
			Log("DXSAVE: " $ Label $ " key " $ string(Key) $ " " $ Key.KeyID $ ": " $ Key.Description);
		for (LogEntry = DP.FirstLog; LogEntry != None; LogEntry = LogEntry.next)
			Log("DXSAVE: " $ Label $ " log " $ string(LogEntry) $ ": " $ LogEntry.text);
	}
	Log("DXSAVE: " $ Label $ " game " $ P.Level.Game $ ", " $ Actors $ " actors, " $ Pawns $ " pawns, " $ Items $ " inventory items");
	foreach P.AllActors(class'MissionScript', S)
		Log("DXSAVE: " $ Label $ " mission script " $ S.Name $ " (" $ S.Class.Name $ ")");
	foreach P.AllActors(class'LoadMarker', M)
		Log("DXSAVE: " $ Label $ " " $ M.Name $ " PostPostBeginPlay " $ M.Begun $ " times");
	foreach P.AllActors(class'GameReplicationInfo', GRI)
	{
		GRIActor = GRI;
		Log("DXSAVE: " $ Label $ " " $ GRI.Name $ " Region " $ GRI.Region $ ", as an Actor zone " $ GRIActor.Region.Zone
			$ " leaf " $ GRIActor.Region.iLeaf $ " number " $ GRIActor.Region.ZoneNumber);
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local SaveHelper S;
	local DeusExGoal Goal;

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
			if (Fresh)
			{
				Log("DXSAVE: a new game, StartNewGame 01_NYC_UNATCOIsland");
				P.ConsoleCommand("StartNewGame 01_NYC_UNATCOIsland");
			}
			else
			{
				Log("DXSAVE: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			}
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && MapTime > 8.0 && !Fresh)
	{
		if (DeusExPlayer(P) != None)
		{
			Goal = DeusExPlayer(P).AddGoal('DXSaveGoal', True);
			Goal.SetText("DXCapture goal");
			DeusExPlayer(P).AddNote("DXCapture note", False, False);
			DeusExPlayer(P).AddNote("DXCapture wide " $ Chr(256) $ Chr(937) $ Chr(8482), False, False);
			if (DeusExPlayer(P).KeyRing != None)
				DeusExPlayer(P).KeyRing.GiveKey('DXSaveKey', "DXCapture key");
			// Two HUD messages: a log object each (DeusExPlayer.AddLog),
			// each named apart in the save
			P.ClientMessage("DXCapture message one");
			P.ClientMessage("DXCapture message two");
		}
		if (Via != "")
		{
			Log("DXSAVE: travelling to " $ Via $ " with the items");
			P.ClientTravel(Via, TRAVEL_Relative, True);
			Step = 3;
			return;
		}
	}
	if ((Step == 0 && MapTime > 8.0) || (Step == 3 && CurrentMap ~= Via && MapTime > 8.0))
	{
		if (!Fresh)
			P.Spawn(class'LoadMarker');
		Census(P, "saved");
		S = P.Spawn(class'SaveHelper', P);
		S.Slot = 9;
		S.Description = "DXCapture save";
		Step = 1;
	}
	else if (Step == 1 && MapTime > 11.0)
	{
		Log("DXSAVE: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
