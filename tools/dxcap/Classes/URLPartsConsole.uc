//=============================================================================
// URLPartsConsole: what a URL's text comes to, as the original's FURL reads
// it (bugs.md NET.13; dx-reverse-info network.md, addresses). From the menu
// map, the console's open of five URLs in turn, each from the level the one
// before reached: 01_NYC_UNATCOIsland/tunnel_drop_nyc, a map, a '/' and a
// portal; DX.dx?Name=Two Words, a blank in an option; 01_NYC_UNATCOHQ/a/b,
// text past a second '/'; 01_NYC_UNATCOHQ?listen=1, an option that is not
// ?listen whole; DEUSEX:01_NYC_UNATCOIsland/, the game's protocol in
// capitals and an empty portal. 3 s into each level reached its parts are
// logged with "DXPARTS:" in front: the level's URL (GetLocalURL), its map
// (GetURLMap), the player's name, the net mode, and the player's distance
// from the teleporter tagged tunnel_drop_nyc where the level has one. A new
// level is told by its LevelInfo's name as text, or for the same map loaded
// again by its clock starting again, as URLConsole tells it; an open that
// reaches none in 15 s says so, and the next goes from where the player
// stands. Then an exit.
//=============================================================================
class URLPartsConsole extends Console;

var float LevelTime, WaitTime, LastSeconds;
var string LastLevel;
var int Step;

function string NextURL()
{
	switch (Step)
	{
	case 0:
		return "01_NYC_UNATCOIsland/tunnel_drop_nyc";
	case 1:
		return "DX.dx?Name=Two Words";
	case 2:
		return "01_NYC_UNATCOHQ/a/b";
	case 3:
		return "01_NYC_UNATCOHQ?listen=1";
	case 4:
		return "DEUSEX:01_NYC_UNATCOIsland/";
	}
	return "";
}

function string PortalDistance(PlayerPawn P)
{
	local Teleporter T;

	foreach P.AllActors(class'Teleporter', T)
	{
		if (string(T.Tag) ~= "tunnel_drop_nyc")
			return ", " $ int(VSize(P.Location - T.Location)) $ " from tunnel_drop_nyc";
	}
	return "";
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string Name;

	Super.Tick(Delta);
	if (Step == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (string(P.Level) != LastLevel || P.Level.TimeSeconds < LastSeconds)
		LevelTime = 0;
	LastLevel = string(P.Level);
	LastSeconds = P.Level.TimeSeconds;
	LevelTime += Delta;
	WaitTime += Delta;
	if (LevelTime < 0 && WaitTime < 15.0)
		return;
	if (LevelTime >= 0 && LevelTime < 3.0)
		return;
	if (LevelTime < 0)
		Log("DXPARTS: step " $ Step $ ": no new level 15 s on");

	Name = "";
	if (P.PlayerReplicationInfo != None)
		Name = P.PlayerReplicationInfo.PlayerName;
	Log("DXPARTS: step " $ Step $ ": " $ P.Level.GetLocalURL() $ ", map " $ P.GetURLMap() $ ", as '" $ Name
	    $ "', net mode " $ int(P.Level.NetMode) $ PortalDistance(P));
	if (NextURL() == "")
	{
		Log("DXPARTS: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	}
	Log("DXPARTS: open " $ NextURL());
	P.ConsoleCommand("open " $ NextURL());
	Step++;
	LevelTime = -1000;
	WaitTime = 0;
}
