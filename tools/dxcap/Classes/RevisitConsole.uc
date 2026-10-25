//=============================================================================
// RevisitConsole: a map saved into Current found again under another case --
// Liberty Island opened from wherever the run starts; 2 s into its time a
// crate tagged 'RevisitMark' spawned ahead of the player and UNATCO HQ
// opened (the same mission, so the island is saved into Current); 3 s into
// HQ the island opened as "01_nyc_unatcoisland", in lower case, as the stock
// exits of Vandenberg and the training map spell theirs; 2 s in, whether the
// marker is there (the level as left, from Current) or not (loaded fresh
// from Maps), then an exit. The original finds the saved level whatever the
// case, Windows' file system being blind to it; DXEngine once looked it up
// by the URL's own spelling and loaded the map fresh on a case-sensitive
// file system (bugs.md E15b.2). Each line starts "DXREVISIT:".
//=============================================================================
class RevisitConsole extends Console;

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

function int Markers(PlayerPawn P)
{
	local DeusExDecoration D;
	local int N;

	foreach P.AllActors(class'DeusExDecoration', D, 'RevisitMark')
		N++;
	return N;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExDecoration Crate;

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
			Log("DXREVISIT: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && MapTime > 2.0)
	{
		Crate = P.Spawn(class'CrateUnbreakableSmall',, 'RevisitMark', P.Location + Vector(P.Rotation) * 160 + vect(0,0,20));
		Log("DXREVISIT: marker " $ Crate $ " on " $ CurrentMap $ ", markers " $ Markers(P) $ ", opening 01_NYC_UNATCOHQ");
		P.ConsoleCommand("open 01_NYC_UNATCOHQ");
		Step = 1;
	}
	else if (Step == 1 && (CurrentMap ~= "01_NYC_UNATCOHQ") && MapTime > 3.0)
	{
		Log("DXREVISIT: on " $ CurrentMap $ ", opening 01_nyc_unatcoisland");
		P.ConsoleCommand("open 01_nyc_unatcoisland");
		Step = 2;
	}
	else if (Step == 2 && (CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 2.0)
	{
		if (Markers(P) > 0)
			Log("DXREVISIT: back on " $ CurrentMap $ " as left: markers " $ Markers(P));
		else
			Log("DXREVISIT: back on " $ CurrentMap $ " loaded fresh: markers 0");
		Step = 3;
	}
	else if (Step == 3 && MapTime > 3.0)
	{
		Log("DXREVISIT: exiting");
		P.ConsoleCommand("exit");
		Step = 4;
	}
}
