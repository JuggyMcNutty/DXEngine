//=============================================================================
// ChangeConsole: what a SET does to the objects of its class after it sets
// them, the original's PostEditChange, in both engines (docs/DEVELOPMENT.md,
// scripted runs; docs/NATIVES.md, the console's GET and SET). On Liberty
// Island, 3 s in: the client's Brightness SET to 5 and read back with GET,
// then set back; the first DeusExMover at rest at its first key whose
// second is away from it sent there in half a second (InterpolateTo), and
// a second later, at rest there, a SET of DeusExMover's bHighlight, its
// place, base and old place logged before and after. Each line is logged
// with "DXCHANGE:" in front, then an exit.
//=============================================================================
class ChangeConsole extends Console;

var float MapTime;
var string CurrentMap;
var int Phase;
var float StepTime;
var DeusExMover Door;
var string OldBrightness;

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

function Note(string What)
{
	Log("DXCHANGE: " $ What $ ": " $ Door.Name $ " at " $ Door.Location $ " base " $ Door.BasePos $ " old " $ Door.OldPos $ " key " $ Door.KeyNum);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExMover M;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

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
		if (MapTime < 3.0 || DeusExPlayer(P) == None)
			return;

		OldBrightness = P.ConsoleCommand("get ini:Engine.Engine.ViewportManager Brightness");
		P.ConsoleCommand("set ini:Engine.Engine.ViewportManager Brightness 5");
		Log("DXCHANGE: Brightness set to 5 from " $ OldBrightness $ ", get " $ P.ConsoleCommand("get ini:Engine.Engine.ViewportManager Brightness"));
		P.ConsoleCommand("set ini:Engine.Engine.ViewportManager Brightness " $ OldBrightness);

		foreach P.AllActors(class'DeusExMover', M)
		{
			if (M.KeyNum == 0 && !M.bInterpolating && VSize(M.KeyPos[1] - M.KeyPos[0]) > 16)
			{
				Door = M;
				break;
			}
		}
		if (Door == None)
		{
			Log("DXCHANGE: no mover found, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		Note("at rest");
		Door.InterpolateTo(1, 0.5);
		StepTime = MapTime;
		Phase = 1;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Phase = 0;
		return;
	}
	else if (Phase == 1 && MapTime - StepTime > 1.5)
	{
		Note("at its second key");
		P.ConsoleCommand("set DeusExMover bHighlight True");
		Note("after a SET of its class");
		StepTime = MapTime;
		Phase = 2;
	}
	else if (Phase == 2 && MapTime - StepTime > 1.0)
	{
		Note("a second later");
		Log("DXCHANGE: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}
