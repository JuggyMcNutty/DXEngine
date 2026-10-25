//=============================================================================
// DoorShotConsole: a dart shot at a closed door -- Liberty Island opened from
// wherever the run starts; 2 s into the level's time the first closed door
// in the level's order (a DeusExMover with bIsDoor at key 0), its box, and
// a Dart spawned 80 units out from the box's thinner side, owned by the
// player and sent at the door at 1500 units a second; every tick for half a
// second the dart's distance from the door's middle along that side's axis,
// its physics, speed and base, and whether it is still in the level; an
// exit. The original's dart stops at the door and sticks to it
// (DeusExProjectile.HitWall: SetBase), where DXEngine's once passed through,
// a mover stopping only what blocked it back (bugs.md E6.6). Each line
// starts "DXSHOT:".
//=============================================================================
class DoorShotConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;
var DeusExMover Door;
var vector DoorC, DoorN;
var float Fired;
var int Ticks;

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

function Shoot(PlayerPawn P)
{
	local DeusExMover M;
	local vector Min, Max, E, Spot, Dir;
	local float Thick;
	local int Sign;
	local Dart D;

	foreach P.AllActors(class'DeusExMover', M)
	{
		if (!M.bIsDoor || M.KeyNum != 0)
			continue;
		if (!M.GetBoundingBox(Min, Max))
			continue;
		Door = M;
		break;
	}
	if (Door == None)
	{
		Log("DXSHOT: no closed door");
		return;
	}
	DoorC = (Min + Max) * 0.5;
	E = (Max - Min) * 0.5;
	if (E.X <= E.Y)
	{
		DoorN = vect(1,0,0);
		Thick = E.X;
	}
	else
	{
		DoorN = vect(0,1,0);
		Thick = E.Y;
	}
	Log("DXSHOT: door " $ Door $ " middle " $ DoorC $ " half " $ E $ " axis " $ DoorN);
	for (Sign = 1; Sign >= -1; Sign -= 2)
	{
		Dir = -DoorN * Sign;
		Spot = DoorC + DoorN * Sign * (Thick + 80);
		D = P.Spawn(class'Dart', P,, Spot, rotator(Dir));
		if (D != None)
			break;
		Log("DXSHOT: no room for the dart at " $ Spot);
	}
	if (D == None)
		return;
	D.Instigator = P;
	D.Velocity = Dir * 1500;
	D.Speed = 1500;
	Fired = P.Level.TimeSeconds;
	Log("DXSHOT: dart " $ D $ " from " $ Spot $ " at " $ (Dir * 1500) $ " physics " $ D.Physics);
}

function Watch(PlayerPawn P)
{
	local Dart D;
	local int Darts;

	foreach P.AllActors(class'Dart', D)
	{
		Darts++;
		Log("DXSHOT: tick " $ Ticks $ " t=" $ (P.Level.TimeSeconds - Fired) $ " distance " $ ((D.Location - DoorC) dot DoorN)
			$ " physics " $ D.Physics $ " speed " $ VSize(D.Velocity) $ " base " $ D.Base $ " stuck " $ D.bStuck);
	}
	if (Darts == 0)
		Log("DXSHOT: tick " $ Ticks $ " t=" $ (P.Level.TimeSeconds - Fired) $ " no dart in the level");
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

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXSHOT: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		Shoot(P);
		Step = 1;
	}
	else if (Step == 1)
	{
		Ticks++;
		Watch(P);
		if (P.Level.TimeSeconds - Fired > 0.5)
			Step = 2;
	}
	else if (Step == 2 && P.Level.TimeSeconds - Fired > 1.5)
	{
		Watch(P);
		Log("DXSHOT: exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
