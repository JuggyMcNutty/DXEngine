//=============================================================================
// EntryConsole: actors made in the Entry level while a map is current, as the
// game's LAN and internet server lists make their links there (docs/
// DEVELOPMENT.md, scripted runs). On Liberty Island, opened from wherever the
// run starts, 3 s in: the player's and the two levels' LevelInfos' regions,
// then a CapMarker spawned by Entry's LevelInfo at its own
// place, one at the player's place and one at each of Entry's PlayerStarts,
// each one's level, zone, zone number and leaf logged; then each moved by
// SetLocation to the player's place, and logged again; then an exit. Each
// line starts "DXENTRY:".
//=============================================================================
class EntryConsole extends Console;

var float MapTime;
var string CurrentMap;
var int Phase;

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
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function Report(string What, Actor A)
{
	if (A == None)
	{
		Log("DXENTRY: " $ What $ ": none");
		return;
	}
	Log("DXENTRY: " $ What $ ": at " $ A.Location $ " level " $ A.Level $ " zone " $ A.Region.Zone $ " number " $ A.Region.ZoneNumber $ " leaf " $ A.Region.iLeaf);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local LevelInfo Entry;
	local PlayerStart S;
	local CapMarker M;
	local int n;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;

	if (Phase == 0)
	{
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = -1;
			}
			return;
		}
		if (MapTime < 3.0)
			return;
		Entry = P.GetEntryLevel();
		Report("the current level's player", P);
		Report("the current level's LevelInfo", P.Level);
		Report("Entry's LevelInfo", Entry);
		Report("a marker at Entry's LevelInfo", Entry.Spawn(class'CapMarker'));
		Report("a marker at the player", Entry.Spawn(class'CapMarker',,, P.Location));
		foreach Entry.AllActors(class'PlayerStart', S)
		{
			Report("Entry's " $ S.Name, S);
			Report("a marker at Entry's " $ S.Name, Entry.Spawn(class'CapMarker',,, S.Location));
		}
		foreach Entry.AllActors(class'CapMarker', M)
		{
			M.SetLocation(P.Location);
			Report("marker " $ n $ " moved to the player", M);
			n++;
		}
		Log("DXENTRY: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
		{
			MapTime = 0;
			Phase = 0;
		}
	}
}
