//=============================================================================
// TurnConsole: what stands on an actor that turns (docs/DEVELOPMENT.md,
// scripted runs; docs/NATIVES.md, small ones). On Liberty Island, opened
// from wherever the run starts, 3 s in: a large crate placed 200 units
// ahead of the player, at rest with no physics, and a small one on top of
// it, 30 units off its middle, based on it; then every half second the large
// crate's SetRotation turns it by a yaw of 5003 and a pitch of 1000 -- an odd
// yaw, which the original's carry rounds to a multiple of 4 -- and the
// small crate's place and rotation are logged, six turns, then an exit.
// Each line starts "DXTURN:".
//=============================================================================
class TurnConsole extends Console;

var float MapTime, StepTime;
var int Step;
var string CurrentMap;
var Decoration Bottom, Top;

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

function Report(string What)
{
	Log("DXTURN: " $ What $ ": top at " $ Top.Location $ " rotation " $ Top.Rotation $ " base " $ Top.Base $ ", bottom rotation " $ Bottom.Rotation);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector Ahead;
	local rotator Facing;

	Super.Tick(Delta);
	if (Step == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0)
	{
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Step = -1;
			}
			return;
		}
		if (MapTime < 3.0)
			return;
		Facing.Yaw = P.Rotation.Yaw;
		Ahead = P.Location + 200 * vector(Facing);
		Bottom = P.Spawn(class'CrateUnbreakableLarge',,, Ahead, rot(0, 0, 0));
		if (Bottom == None)
		{
			Log("DXTURN: no room for the bottom crate, exiting");
			P.ConsoleCommand("exit");
			Step = 99;
			return;
		}
		Bottom.SetPhysics(PHYS_None);
		Top = P.Spawn(class'CrateUnbreakableSmall',,, Bottom.Location + vect(30, 0, 0) + vect(0, 0, 1) * (Bottom.CollisionHeight + 20), rot(0, 0, 0));
		if (Top == None)
		{
			Log("DXTURN: no room for the top crate, exiting");
			P.ConsoleCommand("exit");
			Step = 99;
			return;
		}
		Top.SetPhysics(PHYS_None);
		Top.SetBase(Bottom);
		Report("placed, bottom at " $ Bottom.Location);
		StepTime = MapTime;
		Step = 1;
	}
	else if (Step == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
		{
			MapTime = 0;
			Step = 0;
		}
	}
	else if (Step >= 1 && Step <= 6 && MapTime - StepTime >= 0.5)
	{
		Bottom.SetRotation(Bottom.Rotation + rot(1000, 5003, 0));
		Report("turn " $ Step);
		StepTime = MapTime;
		Step++;
	}
	else if (Step == 7)
	{
		Log("DXTURN: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
	}
}
