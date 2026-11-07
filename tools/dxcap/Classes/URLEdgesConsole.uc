//=============================================================================
// URLEdgesConsole: the URL and travel edges NET.13's fix left (bugs.md
// NET.24; dx-reverse-info network.md, addresses), logged with "DXEDGES:".
// From the menu map, the console's open of four URLs in turn, each from the
// level the one before reached:
//   DX.dx#<1,007 p's>?Name=Cut1234, 1,026 characters: the original's parse
//     keeps the text's first 1,023 (Engine.dll 0x103f7d00), so the menu map
//     is loaded again, its portal the p's, the player named Cut1; the
//     original's same map loaded again makes its objects anew in place, so
//     the LevelInfo held from before the load is the level's again;
//   01_NYC_UNATCOIsland;
//   01_NYC_UNATCOHQ?load=9: a map's browse to the original, the ?load=
//     value the map it tests for its mission (DeusEx.dll 0x100115ce): 9,
//     outside any, Current emptied and UNATCO HQ loaded from Maps;
//   01_NYC_UNATCOIsland?load=01_NYC_UNATCOHQ: mission 1, as the level's,
//     so UNATCO HQ saved into Current and loaded from there, the island
//     the URL names passed over.
// A level reached is told by the console's NotifyLevelChange, which both
// engines' map load sends. 3 s into each, logged: its package's name, its
// map as GetURLMap gives it, its URL's length, the player's name, the net
// mode and the map loads so far; after the first, whether the LevelInfo
// held is the level's and whether it is None. An open that reaches none in
// 15 s says so, and the next goes from where the player stands. Then an
// exit.
//=============================================================================
class URLEdgesConsole extends Console;

var float LevelTime, WaitTime;
var int Step, Loads, LoadsBefore;
var LevelInfo Before;
var config bool Short;

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
		return "01_NYC_UNATCOHQ?load=9";
	case 3:
		return "01_NYC_UNATCOIsland?load=01_NYC_UNATCOHQ";
	}
	return "";
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
	    $ " characters, as '" $ Name $ "', net mode " $ int(P.Level.NetMode) $ ", " $ Loads $ " map loads");
	if (Step == 1)
		Log("DXEDGES: step 1: the LevelInfo held from before the load the level's " $ (P.Level == Before) $ ", None " $ (Before == None));
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
	Log("DXEDGES: open " $ Left(URL, 40) $ " (" $ Len(URL) $ " characters)");
	P.ConsoleCommand("open " $ URL);
	Step++;
	LevelTime = -1000;
	WaitTime = 0;
}
