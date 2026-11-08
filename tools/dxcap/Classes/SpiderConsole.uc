//=============================================================================
// SpiderConsole: pawns climbing in PHYS_Spider (bugs.md PHYS.26), logged
// with "DXSPIDER:". On TargetMap ([DXCapture.SpiderConsole],
// 01_NYC_UNATCOHQ by default), opened from wherever the run starts,
// datalinks and conversations silenced, 2 s into the level's time: the
// nearest wall to the player within Reach (600) along 32 yaws at its middle,
// one whose normal is within 0.2 of level, and the ceiling over it, if any,
// within 1000. Four CapReachers by it, each a DesiredSpeed of 1 (a pawn's 0
// holds it still, SafeNormalConsole) and PHYS_Spider, their Floor none, so
// that their first tick looks for one (findNewFloor: a box of their size
// MaxStepHeight down, then along -Y, +Y, -X and +X); each one's acceleration
// set every tick at its AccelRate:
//   Up: 10 off the wall, 60 over the player's feet, up the wall, on to the
//       ceiling or the wall's top;
//   Along: the same 60 units aside, along the wall, square to its normal;
//   Off: the same 60 units the other side, away from the wall: a floor
//       steeper than 0.6 the acceleration points out of lets it fall;
//   Walker: on the floor 80 off the wall, toward it: it meets the wall and
//       takes it for its floor.
// Every 0.1 s for 3 s each one's place, Floor, velocity, physics, base and
// rotation; then an exit. Floor is const to the script: it is only read.
//=============================================================================
class SpiderConsole extends Console;

var config string TargetMap;
var config float Reach;

var float MapTime, StartTime, NextLog;
var int Step;
var string CurrentMap;
var CapReacher Movers[4];
var vector Accels[4];
var string Names[4];
var vector WallSpot, WallNormal;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOHQ";
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

function string BaseOf(Actor A)
{
	if (A.Base == None)
		return "None";
	if (A.Base == A.Level)
		return "Level";
	return string(A.Base.Name);
}

// The nearest wall within Reach along 32 yaws from the player's middle.
function bool FindWall(PlayerPawn P)
{
	local int d;
	local rotator R;
	local vector HitLoc, HitNorm, Ceiling, CeilNorm;
	local float Best, Range;
	local Actor A;

	Range = Reach;
	if (Range <= 0)
		Range = 600;
	Best = Range + 1;
	for (d = 0; d < 32; d++)
	{
		R.Yaw = d * 2048;
		A = P.Trace(HitLoc, HitNorm, P.Location + Range * vector(R), P.Location, false);
		if (A != None && Abs(HitNorm.Z) < 0.2 && VSize(HitLoc - P.Location) < Best)
		{
			Best = VSize(HitLoc - P.Location);
			WallSpot = HitLoc;
			WallNormal = HitNorm;
		}
	}
	if (Best > Range)
	{
		Log("DXSPIDER: no wall within " $ Range);
		return false;
	}
	Log("DXSPIDER: wall at " $ WallSpot $ ", normal " $ WallNormal $ ", " $ Best $ " from the player at " $ P.Location);
	A = P.Trace(Ceiling, CeilNorm, WallSpot + WallNormal * 30 + vect(0,0,1000), WallSpot + WallNormal * 30, false);
	if (A != None)
		Log("DXSPIDER: ceiling at " $ Ceiling $ ", normal " $ CeilNorm);
	else
		Log("DXSPIDER: no ceiling within 1000");
	return true;
}

function Place(PlayerPawn P, int i, string Who, vector Spot, vector Accel)
{
	Names[i] = Who;
	Movers[i] = P.Spawn(class'CapReacher',,, Spot);
	if (Movers[i] == None)
	{
		Log("DXSPIDER: " $ Who $ " no room at " $ Spot);
		return;
	}
	Movers[i].DesiredSpeed = 1.0;
	Movers[i].SetPhysics(PHYS_Spider);
	Accels[i] = Accel * Movers[i].AccelRate;
	Movers[i].Acceleration = Accels[i];
	Log("DXSPIDER: " $ Who $ " at " $ Movers[i].Location $ ", physics " $ int(Movers[i].Physics) $ ", base " $ BaseOf(Movers[i])
		$ ", floor " $ Movers[i].Floor $ ", acceleration " $ Movers[i].Acceleration);
}

function Report(int i, int Tenths)
{
	if (Movers[i] == None)
		return;
	Log("DXSPIDER: t" $ Tenths $ " " $ Names[i] $ ": at " $ Movers[i].Location $ ", floor " $ Movers[i].Floor
		$ ", velocity " $ Movers[i].Velocity $ ", physics " $ int(Movers[i].Physics) $ ", base " $ BaseOf(Movers[i])
		$ ", rotation " $ Movers[i].Rotation $ ", desired " $ Movers[i].DesiredRotation);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local vector Feet, Aside, Spot;
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
		if (!(CurrentMap ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXSPIDER: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Step = -1;
			}
			return;
		}
		if (P.Level.TimeSeconds < 2.0)
			return;
		if (!FindWall(P))
		{
			P.ConsoleCommand("exit");
			Step = 9;
			return;
		}
		Feet = WallSpot;
		Feet.Z = P.Location.Z - P.CollisionHeight + class'CapReacher'.default.CollisionHeight;
		Aside = Normal(WallNormal cross vect(0,0,1));
		Spot = Feet + WallNormal * (class'CapReacher'.default.CollisionRadius + 10) + vect(0,0,60);
		Place(P, 0, "Up", Spot, vect(0,0,1));
		Place(P, 1, "Along", Spot + Aside * 60, Aside);
		Place(P, 2, "Off", Spot - Aside * 60, WallNormal);
		Place(P, 3, "Walker", Feet + WallNormal * (class'CapReacher'.default.CollisionRadius + 80) + vect(0,0,1), -WallNormal);
		StartTime = P.Level.TimeSeconds;
		NextLog = 0.1;
		Step = 1;
	}
	else if (Step == -1)
	{
		if (CurrentMap ~= Target())
			Step = 0;
	}
	else if (Step == 1)
	{
		for (i = 0; i < 4; i++)
			if (Movers[i] != None && Movers[i].Physics == PHYS_Spider)
				Movers[i].Acceleration = Accels[i];
		if (P.Level.TimeSeconds - StartTime >= NextLog)
		{
			for (i = 0; i < 4; i++)
				Report(i, int(NextLog * 10 + 0.5));
			NextLog += 0.1;
		}
		if (NextLog > 3.05)
		{
			Log("DXSPIDER: done, exiting");
			P.ConsoleCommand("exit");
			Step = 9;
		}
	}
}
