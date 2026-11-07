//=============================================================================
// SafeNormalConsole: a walk and a flight toward an acceleration shorter than
// 0.0001 (bugs.md PHYS.25), logged with "DXSAFENORMAL:". Liberty Island,
// opened from wherever the run starts, datalinks and conversations
// silenced; 2 s into the level's time, four CapReachers 120 units from the
// player, each heading away from it at 200 units/s, with an acceleration
// square to its heading, to its left, set again each tick: UnderWalk,
// walking ahead, the acceleration 0.00009 long (8.1e-9 squared); OverWalk,
// walking a quarter turn round from ahead, 0.00011 long (1.21e-8 squared);
// UnderFly and OverFly the same flying, 150 units up. The two ways are
// RollConsole's open ones, its crates there not turned in their first
// tenth of a second. Each is given a DesiredSpeed of 1: a pawn's is
// 0 by Engine.Pawn's defaults, and calcVelocity holds any pawn but a player
// pawn to its speed times it (Engine.dll 0x103cdf27), so with 0 it stops at
// once. Every 0.1 s for 0.5 s, each one's velocity along its heading and to
// its left, its physics and its base; then an exit. The original's
// physWalking and physFlying give calcVelocity the acceleration's
// SafeNormal (0x103ca63c, 0x103d3121), which Core makes none under 1e-8
// squared (Core.dll 0x10144be0): given none, the velocity only slows along
// its heading, by the ground's friction (the zone's fluid friction
// flying); given a direction, it turns toward it.
//=============================================================================
class SafeNormalConsole extends Console;

var float MapTime, StartTime, NextLog;
var int Step;
var string CurrentMap;
var CapReacher Movers[4];
var vector Headings[4], Lefts[4];
var float Lengths[4];
var string Names[4];

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

function string BaseOf(Actor A)
{
	if (A.Base == None)
		return "None";
	if (A.Base == A.Level)
		return "Level";
	return string(A.Base.Class.Name);
}

// Mover i, 120 units from the player along its yaw turned by Turn, its feet
// where the player's are and Rise over them, heading away at 200 units/s,
// its acceleration Length long to its left
function Place(PlayerPawn P, int i, string Who, int Turn, float Rise, float Length, bool bFly)
{
	local rotator R;
	local vector Spot;

	R = P.Rotation;
	R.Pitch = 0;
	R.Roll = 0;
	R.Yaw += Turn;
	Headings[i] = vector(R);
	R.Yaw -= 16384;
	Lefts[i] = vector(R);
	Lengths[i] = Length;
	Names[i] = Who;
	Spot = P.Location + Headings[i] * 120;
	Spot.Z += class'CapReacher'.default.CollisionHeight - P.CollisionHeight + Rise;
	Movers[i] = P.Spawn(class'CapReacher',,, Spot);
	if (Movers[i] == None)
	{
		Log("DXSAFENORMAL: " $ Who $ " no room at " $ Spot);
		return;
	}
	if (bFly)
		Movers[i].SetPhysics(PHYS_Flying);
	else
		Movers[i].SetPhysics(PHYS_Walking);
	Movers[i].DesiredSpeed = 1.0;
	Movers[i].Velocity = Headings[i] * 200;
	Movers[i].Acceleration = Lefts[i] * Length;
	Log("DXSAFENORMAL: " $ Who $ " at " $ Movers[i].Location $ ", physics " $ int(Movers[i].Physics)
		$ ", base " $ BaseOf(Movers[i]) $ ", DesiredSpeed " $ Movers[i].DesiredSpeed
		$ ", its acceleration's squared length x 1e10 " $ (Movers[i].Acceleration dot Movers[i].Acceleration) * 10000000000.0);
}

function Report(int i, int Tenths)
{
	local vector V;

	if (Movers[i] == None)
		return;
	V = Movers[i].Velocity;
	Log("DXSAFENORMAL: t" $ Tenths $ " " $ Names[i] $ ": along " $ (V dot Headings[i]) $ ", to its left "
		$ (V dot Lefts[i]) $ ", physics " $ int(Movers[i].Physics) $ ", base " $ BaseOf(Movers[i]));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local int i;

	Super.Tick(Delta);
	if (Step == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP != None)
	{
		if (DXP.dataLinkPlay != None)
			DXP.dataLinkPlay.AbortDataLink();
		if (DXP.conPlay != None)
			DXP.conPlay.TerminateConversation();
	}

	if (Step == 0)
	{
		// The original starts in its menu map.
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Log("DXSAFENORMAL: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Step = -1;
			}
			return;
		}
		if (P.Level.TimeSeconds < 2.0)
			return;
		Place(P, 0, "UnderWalk", 0, 0.0, 0.00009, false);
		Place(P, 1, "OverWalk", 16384, 0.0, 0.00011, false);
		Place(P, 2, "UnderFly", 0, 150.0, 0.00009, true);
		Place(P, 3, "OverFly", 16384, 150.0, 0.00011, true);
		StartTime = P.Level.TimeSeconds;
		NextLog = 0.1;
		Step = 1;
	}
	else if (Step == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Step = 0;
	}
	else if (Step == 1)
	{
		// The acceleration as set, whatever the tick's physics left of it
		for (i = 0; i < 4; i++)
			if (Movers[i] != None)
				Movers[i].Acceleration = Lefts[i] * Lengths[i];
		if (P.Level.TimeSeconds - StartTime >= NextLog)
		{
			for (i = 0; i < 4; i++)
				Report(i, int(NextLog * 10 + 0.5));
			NextLog += 0.1;
		}
		if (NextLog > 0.55)
		{
			Log("DXSAFENORMAL: done, exiting");
			P.ConsoleCommand("exit");
			Step = 9;
		}
	}
}
