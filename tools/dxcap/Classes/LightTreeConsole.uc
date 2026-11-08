//=============================================================================
// LightTreeConsole (bugs.md AI.22): the light the AI sees by,
// asked in the tick a light is switched on or moved, logged with "DXLTREE:"
// -- TargetMap ([DXCapture.LightTreeConsole], 01_NYC_UNATCOIsland by
// default) opened from wherever the run starts; 2 s into the level's time,
// 300 units over the player, two CapLightSwitches 1000 units apart along Y,
// both spawned off, and a CapLightProbe, which switches Moved on at its
// first tick and, at its third, Off on and Moved 400 units along X, asking
// AIGetLightLevel 50 units from each in that tick and the next three
// (CapLightProbe); 2 s on, an exit. Each line in both engines from the
// probe; compared: the "tick3 after" line, a light switched on or moved
// within the tick counted at once.
//=============================================================================
class LightTreeConsole extends Console;

var config string TargetMap;

var float MapTime;
var int Step;
var string CurrentMap;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

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

function Run(PlayerPawn P)
{
	local CapLightProbe Probe;
	local vector Over;

	Over = P.Location + vect(0, 0, 300);
	Probe = P.Spawn(class'CapLightProbe',,, Over);
	if (Probe == None)
	{
		Log("DXLTREE: no probe");
		return;
	}
	Probe.OffAt = Over + vect(0, -500, 0);
	Probe.MovedAt = Over + vect(0, 500, 0);
	Probe.Off = P.Spawn(class'CapLightSwitch',,, Probe.OffAt);
	Probe.Moved = P.Spawn(class'CapLightSwitch',,, Probe.MovedAt);
	if (Probe.Off == None || Probe.Moved == None)
	{
		Log("DXLTREE: no lights");
		return;
	}
	// Moved is switched on by the probe's first tick (an Actor's enum), two
	// frames before its third
	Log("DXLTREE: off at " $ Probe.OffAt $ ", moved at " $ Probe.MovedAt $ ", radius " $ Probe.Moved.LightRadius);
}

event Tick(float Delta)
{
	local PlayerPawn P;

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

	if (Step == 0 && !(CurrentMap ~= Target()))
	{
		if (MapTime > 3.0)
		{
			Log("DXLTREE: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		Run(P);
		MapTime = 0;
		Step = 1;
	}
	else if (Step == 1 && MapTime > 2.0)
	{
		Log("DXLTREE: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
