//=============================================================================
// Pkg44Console: a map opened by a name with folders in it (bugs.md PKG.44).
// 2 s in, from wherever the run starts, OpenPath
// ([DXCapture.Pkg44Console], ..\Pkg44\Maps\01_NYC_UNATCOIsland.dx by
// default) opened: the run's script lays a copy of another map there under
// a stock map's name. 2 s into the map that comes up, its package's name,
// its DeusExLevelInfo's MapName and mission, and its count of actors; or,
// 10 s on with no new map, that none came; then an exit. Each line is
// logged with "DXPKG44:" in front.
//=============================================================================
class Pkg44Console extends Console;

var config string OpenPath;

var string CurrentURL;
var float MapTime;
var int Step;

function string Path()
{
	if (OpenPath == "")
		return "..\\Pkg44\\Maps\\01_NYC_UNATCOIsland.dx";
	return OpenPath;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExLevelInfo Info;
	local Actor A;
	local int N;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (P.Level.GetLocalURL() != CurrentURL)
	{
		CurrentURL = P.Level.GetLocalURL();
		MapTime = 0;
		if (Step == 1)
			Step = 2;
	}
	MapTime += Delta;

	if (Step == 0 && MapTime > 2.0)
	{
		Log("DXPKG44: opening " $ Path());
		Step = 1;
		P.ConsoleCommand("open " $ Path());
	}
	else if (Step == 1 && MapTime > 10.0)
	{
		Log("DXPKG44: no new map, still on " $ P.Level.Outer.Name);
		Step = 3;
		P.ConsoleCommand("exit");
	}
	else if (Step == 2 && MapTime > 2.0)
	{
		foreach P.AllActors(class'DeusExLevelInfo', Info)
			break;
		foreach P.AllActors(class'Actor', A)
			N++;
		if (Info != None)
			Log("DXPKG44: map " $ P.Level.Outer.Name $ ", MapName " $ Info.MapName $ ", mission " $ Info.MissionNumber $ ", " $ N $ " actors");
		else
			Log("DXPKG44: map " $ P.Level.Outer.Name $ ", no DeusExLevelInfo, " $ N $ " actors");
		Step = 3;
		P.ConsoleCommand("exit");
	}
}
