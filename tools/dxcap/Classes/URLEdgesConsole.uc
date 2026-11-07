//=============================================================================
// URLEdgesConsole: the URL and travel edges NET.13's fix left, and a URL's
// ?load= (bugs.md NET.24; dx-reverse-info network.md, addresses, and
// deusex-dll.md, travel and saving), logged with "DXEDGES:". From the menu
// map, the console's open of each URL in turn, each from the level the one
// before reached:
//   DX.dx#<1,007 p's>?Name=Cut1234, 1,026 characters: the original's parse
//     keeps the text's first 1,023 (Engine.dll 0x103f7d00), so the menu map
//     is loaded again, its portal the p's, the player named Cut1; the
//     original's same map loaded again makes its objects anew in place, so
//     the LevelInfo held from before the load is the level's again. With
//     Short, DX.dx?Name=Cut1234 alone: the original's process ends at the
//     long one's open. The LevelInfo is let go once logged: the original's
//     browse takes the level's mission from the first DeusExLevelInfo in its
//     object list (DeusEx.dll 0x100120a0), the menu map's -2 while anything
//     holds that level. With Hold it is held to the end, and the original's
//     browses from the island and HQ read -2, each a new mission's;
//   01_NYC_UNATCOIsland, mission 1, the menu map's -2: Current emptied;
//   01_NYC_UNATCOHQ: the island saved into Current, HQ from Maps;
//   01_NYC_UNATCOHQ?load=01_NYC_UNATCOIsland: the ?load= value is the map
//     the browse tests for its mission and looks for in Current, the URL's
//     map the one it loads from Maps (DeusEx.dll 0x100115ce,
//     0x10011736-0x10011825): HQ saved into Current, the island back from
//     there with its crate, HQ passed over, the level keeping the URL its
//     file holds, which becomes the last URL;
//   01_NYC_UNATCOHQ?load=9: 9 in no mission, so Current emptied and HQ from
//     Maps, its URL its file's Index.dx, as for any URL with an option
//     starting load, the last URL the browse's, ?load=9 on it;
//   01_NYC_UNATCOIsland: the ?load=9 carried on, so Current emptied again,
//     HQ not saved, the island from Maps, its URL Index.dx;
//   01_NYC_UNATCOIsland?load=01_NYC_UNATCOHQ: HQ's file not in Current, so
//     the island, saved into Current, comes again from Maps, not from there;
//   01_NYC_UNATCOHQ?load=01_NYC_UNATCOIsland: the island saved into Current
//     and found there, HQ passed over, its URL its file's Index.dx, the last
//     URL that one.
// A crate tagged EdgesMark is left in each level from the island on before
// the next open: one comes back only with a level from Current.
// A level reached is told by the console's NotifyLevelChange, which both
// engines' map load sends. 3 s into each, logged: its package's name, its
// map as GetURLMap gives it (the game engine's last URL's), the length of
// its own URL (GetLocalURL), the player's name, the net mode, the map loads
// so far and the crates tagged EdgesMark in it; after the first, whether the
// LevelInfo held from before is the level's and whether it is None. An open
// that reaches none in 15 s says so, and the next goes from where the player
// stands. Then an exit.
//=============================================================================
class URLEdgesConsole extends Console;

var float LevelTime, WaitTime;
var int Step, Loads, LoadsBefore;
var LevelInfo Before;
var config bool Short, Hold;

event NotifyLevelChange()
{
	Super.NotifyLevelChange();
	Loads++;
}

function string NextURL()
{
	local string Pad;
	local int i;

	switch (Step)
	{
	case 0:
		// DX.dx# (6), 1,007 p's and ?Name=Cut1234 (13): 1,026 characters,
		// cut after Cut1; with Short the menu map again with a short URL
		// alone, as the original's run takes it: its open of the long one
		// ends the original's process 4 s in, its log unflushed
		if (Short)
			return "DX.dx?Name=Cut1234";
		for (i = 0; i < 1007; i++)
			Pad = Pad $ "p";
		return "DX.dx#" $ Pad $ "?Name=Cut1234";
	case 1:
		return "01_NYC_UNATCOIsland";
	case 2:
		return "01_NYC_UNATCOHQ";
	case 3:
		return "01_NYC_UNATCOHQ?load=01_NYC_UNATCOIsland";
	case 4:
		return "01_NYC_UNATCOHQ?load=9";
	case 5:
		return "01_NYC_UNATCOIsland";
	case 6:
		return "01_NYC_UNATCOIsland?load=01_NYC_UNATCOHQ";
	case 7:
		return "01_NYC_UNATCOHQ?load=01_NYC_UNATCOIsland";
	}
	return "";
}

// The crates tagged EdgesMark in the level: one left in a level saved into
// Current is there again when the level comes back from Current, and none
// is in a map loaded from Maps.
function int Markers(PlayerPawn P)
{
	local DeusExDecoration D;
	local int N;

	foreach P.AllActors(class'DeusExDecoration', D, 'EdgesMark')
		N++;
	return N;
}

// A crate tagged EdgesMark left 160 units from the player: ahead, or where
// something is in the way, to a side or behind.
function LeaveCrate(PlayerPawn P)
{
	local Actor A;
	local rotator R;
	local int i;

	R = P.Rotation;
	R.Pitch = 0;
	for (i = 0; i < 4 && A == None; i++)
	{
		A = P.Spawn(class'CrateUnbreakableSmall',, 'EdgesMark', P.Location + Vector(R) * 160 + vect(0,0,20));
		R.Yaw += 16384;
	}
	Log("DXEDGES: step " $ Step $ ": a crate left, markers " $ Markers(P));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string Name, URL;

	Super.Tick(Delta);
	if (Step == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (Loads != LoadsBefore)
	{
		LoadsBefore = Loads;
		LevelTime = 0;
	}
	LevelTime += Delta;
	WaitTime += Delta;
	if (LevelTime < 0 && WaitTime < 15.0)
		return;
	if (LevelTime >= 0 && LevelTime < 3.0)
		return;
	if (LevelTime < 0)
		Log("DXEDGES: step " $ Step $ ": no new level 15 s on");

	Name = "";
	if (P.PlayerReplicationInfo != None)
		Name = P.PlayerReplicationInfo.PlayerName;
	Log("DXEDGES: step " $ Step $ ": package " $ P.Level.Outer.Name $ ", map " $ P.GetURLMap() $ ", URL of " $ Len(P.Level.GetLocalURL())
	    $ " characters, as '" $ Name $ "', net mode " $ int(P.Level.NetMode) $ ", " $ Loads $ " map loads, markers " $ Markers(P));
	if (Step == 1)
	{
		Log("DXEDGES: step 1: the LevelInfo held from before the load the level's " $ (P.Level == Before) $ ", None " $ (Before == None));
		// Let go, so that nothing holds the menu map's level past the next
		// load and the original's mission test reads each level's own
		if (!Hold)
			Before = None;
	}
	URL = NextURL();
	if (URL == "")
	{
		Log("DXEDGES: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	}
	if (Step == 0)
		Before = P.Level;
	if (Step >= 2)
		LeaveCrate(P);
	Log("DXEDGES: open " $ Left(URL, 40) $ " (" $ Len(URL) $ " characters)");
	P.ConsoleCommand("open " $ URL);
	Step++;
	LevelTime = -1000;
	WaitTime = 0;
}
