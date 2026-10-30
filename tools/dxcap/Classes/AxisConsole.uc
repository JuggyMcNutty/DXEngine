//=============================================================================
// AxisConsole: an input axis a script sets, as the player's input reads it,
// in both engines (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md,
// input axes). On Liberty Island, 8 s in, the player at rest: for Ticks of
// the console's ticks (30 by default) it sets the player's aBaseY to Axis
// (1 by default) after each level tick, then lets it be; each of its ticks
// logs its time, the player's acceleration and speed, from the level tick
// before it, with "DXAXIS:" in front. The original's input scales every
// input float by 20 over the player's tick as it reads it, a script's too,
// so run at a fixed rate (DXCAP_FPS=60) for its acceleration to be the same
// from tick to tick. Then an exit.
//=============================================================================
class AxisConsole extends Console;

var config float Axis;
var config int Ticks;

var float MapTime;
var string CurrentMap;
var int Phase;
var int Count;

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

event Tick(float Delta)
{
	local PlayerPawn P;

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
		if (MapTime < 8.0 || DeusExPlayer(P) == None)
			return;
		if (Axis == 0)
			Axis = 1;
		if (Ticks == 0)
			Ticks = 30;
		Log("DXAXIS: at rest at " $ P.Location $ ", accel rate " $ P.AccelRate $ ", ground speed " $ P.GroundSpeed);
		Phase = 1;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Phase = 0;
		return;
	}

	if (Phase == 1)
	{
		Log("DXAXIS: tick " $ Count $ " of " $ Delta $ ": acceleration " $ VSize(P.Acceleration) $ ", speed " $ VSize(P.Velocity));
		if (Count < Ticks)
			P.aBaseY = Axis;
		Count++;
		if (Count > Ticks + 10)
		{
			Log("DXAXIS: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
		}
	}
}
