//=============================================================================
// StatueConsole: the terrorists Liberty Island's script kills as the statue's
// mission ends, and the carcasses they leave, logged with "DXSTATUE:", the
// ticks since the flag and the console's clock (docs/DEVELOPMENT.md, scripted
// runs).
//
// On Liberty Island, 6 s in: each Terrorist's state, health and items. Then
// the flag StatueMissionComplete set, as the statue's ending sets it:
// Mission01.Timer, within its second, kills each terrorist but the commander
// (TakeDamage at no health) and deletes its inventory. From then on, each
// tick for 5 s: a terrorist's state, health, items, hidden and physics
// logged when they change, and each TerroristCarcass as it appears, with its
// items; then every carcass's items once more. Then UNATCO HQ opened (the
// same mission, so the island is saved into Current), and 3 s in the island
// opened again from there: 3 s in, every carcass's items, and an exit.
//=============================================================================
class StatueConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var int Ticks;
var bool bTravelled;
var Terrorist Watched[64];
var string LastLine[64];
var int NumWatched;
var TerroristCarcass Seen[64];
var int NumSeen;

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;
	// The name after the last / or \: a level from Current names its file
	// in the original (..\Save\Current\<map>.dxs).
	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, "\\");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "\\");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

// A holder's items by name, in chain order; a chain longer than 40 does not
// end.
function string Items(Actor Holder)
{
	local Inventory item;
	local string S;
	local int n;

	for (item = Holder.Inventory; item != None && n < 40; item = item.Inventory)
	{
		S = S $ " " $ item.Name;
		n++;
	}
	if (n >= 40)
		S = S $ " (the chain does not end)";
	return n $ " items" $ S;
}

function string Describe(Terrorist T)
{
	if (T == None || T.bDeleteMe)
		return "gone";
	return string(T.GetStateName()) $ " health " $ T.Health $ " hidden " $ T.bHidden
	    $ " physics " $ T.Physics $ " " $ Items(T);
}

function Note(string S)
{
	Log("DXSTATUE: tick " $ Ticks $ " at " $ MapTime $ ": " $ S);
}

function Watch(PlayerPawn P)
{
	local Terrorist T;

	foreach P.AllActors(class'Terrorist', T)
	{
		if (NumWatched >= 64)
			break;
		Watched[NumWatched] = T;
		LastLine[NumWatched] = Describe(T);
		Note(T.Name $ " (" $ T.BindName $ ") " $ LastLine[NumWatched]);
		NumWatched++;
	}
}

// Each terrorist whose line changed, and each carcass not seen before.
function Check(PlayerPawn P)
{
	local TerroristCarcass C;
	local string Line;
	local int i;
	local bool bKnown;

	for (i = 0; i < NumWatched; i++)
	{
		if (LastLine[i] == "gone")
			continue;
		Line = Describe(Watched[i]);
		if (Line != LastLine[i])
		{
			Note(string(i) $ " " $ Line);
			LastLine[i] = Line;
		}
	}
	foreach P.AllActors(class'TerroristCarcass', C)
	{
		bKnown = false;
		for (i = 0; i < NumSeen; i++)
			if (Seen[i] == C)
				bKnown = true;
		if (!bKnown && NumSeen < 64)
		{
			Seen[NumSeen++] = C;
			Note("carcass " $ C.Name $ " of " $ C.KillerBindName $ " " $ C.itemName $ ": " $ Items(C));
		}
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local TerroristCarcass C;
	local string M;
	local int i;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	CapTime += Delta;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXCAP: now in " $ M $ " at " $ CapTime);
		CurrentMap = M;
		MapTime = 0;
		StepTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	// Only the island needs Deus Ex's player; the menu map's travels.
	if (DXP == None && M ~= "01_NYC_UNATCOIsland")
		return;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0 && !bTravelled)
			{
				Log("DXCAP: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				bTravelled = true;
			}
		}
		else if (MapTime > 6.0)
		{
			Watch(P);
			foreach P.AllActors(class'TerroristCarcass', C)
				Seen[NumSeen++] = C;
			Note(NumSeen $ " carcasses at the start");
			DXP.flagBase.SetBool('StatueMissionComplete', True);
			Note("StatueMissionComplete set");
			StepTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == 1)
	{
		Ticks++;
		Check(P);
		if (StepTime > 5.0)
		{
			foreach P.AllActors(class'TerroristCarcass', C)
				Note("at the end, carcass " $ C.Name $ ": " $ Items(C));
			// The original's garbage collection stops the game on a console
			// that still holds the level's actors as it is left.
			for (i = 0; i < 64; i++)
			{
				Watched[i] = None;
				Seen[i] = None;
			}
			Log("DXCAP: opening 01_NYC_UNATCOHQ");
			P.ConsoleCommand("open 01_NYC_UNATCOHQ");
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		if (M ~= "01_NYC_UNATCOHQ" && MapTime > 3.0)
		{
			Log("DXCAP: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		if (M ~= "01_NYC_UNATCOIsland" && MapTime > 3.0)
		{
			foreach P.AllActors(class'TerroristCarcass', C)
				Note("after the return, carcass " $ C.Name $ ": " $ Items(C));
			Log("DXSTATUE: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 4;
		}
	}
}
