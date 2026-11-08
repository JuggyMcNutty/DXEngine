//=============================================================================
// WallHitConsole (bugs.md PHYS.31): a walker head-on into a wall --
// Liberty Island opened from wherever the run starts; 2 s in, from the
// player's middle, level lines along +X, -X, +Y and -Y, 600 long: the first
// that meets the level with a normal exactly along an axis is the wall. A
// CapWallWalker spawned 60 units out from it along that normal, its
// Destination 200 units behind the wall, its velocity 200 and its
// acceleration its AccelRate exactly along the wall's inward axis (each
// other axis 0), a DesiredSpeed of 1; every tick for 1 s its HitWall events
// (CapWallWalker logs them with the tick), and at each tick its place, its
// gap to the wall and its MoveTimer and bFromWall.
//
// Each walking step into the wall, once it rests 2 units off it, is blocked
// at once and stepped up (stepUp, Engine.dll 0x103ce4a0): the move after the
// rise is blocked again, processHitWall, and the slide along the wall --
// the rest less its part along the normal -- is nothing, head on. The
// original's MoveActor leaves a move of nothing's hit as it was (0x1039915a-
// 0x10399228), so its time is still under 1: processHitWall again and
// TwoWallAdjust. Expected: two HitWalls a tick, the wall's normal and the
// level each time. Each line starts "DXWALLHIT:".
//=============================================================================
class WallHitConsole extends Console;

var float RunTime, ArmedAt, MapTime;
var int Phase, Ticks;
var string CurrentMap;
var CapWallWalker Walker;
var vector WallAt, WallNormal, Dir;

function Say(string S)
{
	Log("DXWALLHIT: " $ S);
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

function bool Axis(vector N)
{
	return (N.X == 0 && N.Y == 0 && (N.Z == 1 || N.Z == -1)) || (N.X == 0 && N.Z == 0 && (N.Y == 1 || N.Y == -1))
		|| (N.Y == 0 && N.Z == 0 && (N.X == 1 || N.X == -1));
}

function bool FindWall(PlayerPawn P)
{
	local vector Dirs[4], HitLoc, HitNorm;
	local Actor Hit;
	local int i;

	Dirs[0] = vect(1,0,0); Dirs[1] = vect(-1,0,0); Dirs[2] = vect(0,1,0); Dirs[3] = vect(0,-1,0);
	for (i = 0; i < 4; i++)
	{
		Hit = P.Trace(HitLoc, HitNorm, P.Location + Dirs[i] * 600, P.Location, false);
		if (Hit == P.Level && Axis(HitNorm) && HitNorm.Z == 0)
		{
			WallAt = HitLoc;
			WallNormal = HitNorm;
			Dir = -HitNorm;
			return true;
		}
	}
	return false;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector Spot;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	if (Phase == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Say("opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Phase = -1;
		}
		return;
	}
	if (Phase == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Phase = 0;
	if (Phase == 0 && P.Level.TimeSeconds > 2.0)
	{
		if (!FindWall(P))
		{
			Say("no wall along an axis within 600, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Spot = WallAt + WallNormal * 60;
		Spot.Z = P.Location.Z;
		Walker = P.Spawn(class'CapWallWalker',,, Spot);
		if (Walker == None)
		{
			Say("no walker at " $ Spot $ ", exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Walker.SetPhysics(PHYS_Walking);
		Walker.DesiredSpeed = 1.0;
		Walker.Destination = WallAt - WallNormal * 200;
		Walker.Velocity = Dir * 200;
		Walker.Acceleration = Dir * Walker.AccelRate;
		Walker.bWatching = true;
		Say("wall at " $ WallAt $ " normal " $ WallNormal $ "; walker at " $ Walker.Location $ " heading " $ Dir);
		ArmedAt = RunTime;
		Phase = 1;
	}
	else if (Phase == 1)
	{
		Ticks++;
		Walker.Acceleration = Dir * Walker.AccelRate;
		Say("tick " $ Walker.Ticks $ " at " $ Walker.Location $ " gap " $ ((Walker.Location - WallAt) dot WallNormal - Walker.CollisionRadius)
			$ " movetimer " $ Walker.MoveTimer $ " fromwall " $ Walker.bFromWall);
		if (RunTime - ArmedAt > 1.0)
		{
			Say("done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
